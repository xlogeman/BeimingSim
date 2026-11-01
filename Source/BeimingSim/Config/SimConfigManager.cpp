#include "SimConfigManager.h"
#include "SimConfigWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/PrettyJsonPrintPolicy.h"

// AirSim 运行时 API
#include "SimMode/SimModeBase.h"
#include "SimHUD/SimHUD.h"
#include "PIPCamera.h"
#include "Weather/WeatherLib.h"
#include "common/AirSimSettings.hpp"
#include "common/Settings.hpp"

ASimConfigManager::ASimConfigManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ASimConfigManager::BeginPlay()
{
    Super::BeginPlay();

    // GameMode 的 BeginPlay 早于 SimHUD → SimMode 的 BeginPlay（SIMMODE 静态指针在构造函数
    // 里就已赋值，但 physics_world_ 要到 SimModeWorldBase::BeginPlay 才创建）。
    // 面板构建时会回读 SimMode 状态（暂停/天气/时钟），必须延迟到 BeginPlay 链完成后创建，
    // 否则 isPaused() 解引用空 physics_world_ 直接崩溃。
    GetWorldTimerManager().SetTimer(CreateWidgetTimer, this,
        &ASimConfigManager::CreateWidgetInternal, 0.5f, false);

    BindOpenKey();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID(), 10.f, FColor::Cyan,
            TEXT("按 O 打开仿真配置面板"));
    }
}

void ASimConfigManager::CreateWidgetInternal()
{
    if (!PanelWidget)
    {
        PanelWidget = CreateWidget<USimConfigWidget>(GetWorld(), USimConfigWidget::StaticClass());
    }
    if (!PanelWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: 配置面板创建失败"));
        return;
    }
    PanelWidget->BuildPanel(this);
    PanelWidget->AddToViewport(101);
}

void ASimConfigManager::BindOpenKey()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->InputComponent)
    {
        PC->InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ASimConfigManager::TogglePanel);
    }
    else
    {
        // PlayerController 尚未就绪时稍后重试（与 DroneModelSwitcher 的 Tab 绑定同一模式）
        GetWorldTimerManager().SetTimer(BindRetryTimer, this,
            &ASimConfigManager::BindOpenKey, 1.0f, false);
    }
}

void ASimConfigManager::TogglePanel()
{
    if (PanelWidget)
    {
        PanelWidget->TogglePanel();
    }
}

void ASimConfigManager::ApplyLive(bool bWeatherEnabled, float Rain, float Snow, float Dust, float Fog,
    const FVector& WindNed, float CameraFov, bool bPaused)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 天气（与 WorldSimApi::enableWeather/setWeatherParameter 同一底层实现）
    if (UWeatherLib::getIsWeatherEnabled(World) != bWeatherEnabled)
    {
        UWeatherLib::setWeatherEnabled(World, bWeatherEnabled);
    }
    UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN, Rain);
    UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_SNOW, Snow);
    UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_DUST, Dust);
    UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG, Fog);

    if (ASimModeBase* Sim = ASimModeBase::getSimMode())
    {
        // 风：与 RPC API simSetWind 相同入口，NED 框 m/s，作用于物理引擎
        Sim->setWind(msr::airlib::Vector3r(WindNed.X, WindNed.Y, WindNed.Z));

        // 暂停（AirSim 侧暂停物理，渲染与 UI 继续走）
        Sim->pause(bPaused);

        // 默认相机 FOV（camera_name/vehicle_name 传空即默认载具默认相机）
        if (APIPCamera* Camera = Sim->getCamera(msr::airlib::CameraDetails("", "")))
        {
            Camera->setCameraFoV(CameraFov);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: 未找到 AirSim SimMode，风/暂停/FOV 未应用"));
    }
}

void ASimConfigManager::ApplyTimeOfDay(int32 PresetIndex)
{
    ASimModeBase* Sim = ASimModeBase::getSimMode();
    if (!Sim)
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: 未找到 AirSim SimMode，时间模拟未应用"));
        return;
    }

    // start_datetime 格式 %Y-%m-%d %H:%M:%S（固定日期，太阳位置随时刻变化）
    static const std::string Datetimes[] = {
        "2026-06-01 06:30:00", // 清晨
        "2026-06-01 12:00:00", // 正午
        "2026-06-01 18:30:00", // 黄昏
        "2026-06-01 22:00:00", // 夜晚
    };
    if (PresetIndex >= 0 && PresetIndex < 4)
    {
        Sim->setTimeOfDay(true, Datetimes[PresetIndex], /*is_start_datetime_dst*/ false,
            /*celestial_clock_speed*/ 1.f, /*update_interval_secs*/ 60.f, /*move_sun*/ true);
    }
    else
    {
        Sim->setTimeOfDay(false, "", false, 1.f, 60.f, true);
    }
}

bool ASimConfigManager::ApplyAndRebuild(float ClockSpeed)
{
    const FString JsonPath = GetSettingsFilePath();
    if (JsonPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: 无法定位 settings.json，放弃写回"));
        return false;
    }

    // 写前备份，防止写坏后无配置可用（AirSim 读不到合法 settings 会自动生成默认文件）
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString BackupPath = JsonPath + TEXT(".bak");
    PlatformFile.CopyFile(*BackupPath, *JsonPath);

    // 把结构化修改写进 Settings 单例再落盘（Settings::saveJSonFile 会序列化整个文档）
    msr::airlib::Settings& SettingsJson = msr::airlib::Settings::singleton();
    SettingsJson.setDouble("ClockSpeed", static_cast<double>(ClockSpeed));

    // 固化 SimMode 类型，避免重载时弹出"选择载具"对话框
    std::string SimModeName = msr::airlib::AirSimSettings::singleton().simmode_name;
    SettingsJson.setString("SimMode", SimModeName.empty() ? std::string("Multirotor") : SimModeName);

    SettingsJson.saveJSonFile(TCHAR_TO_UTF8(*JsonPath));
    UE_LOG(LogTemp, Log, TEXT("SimConfig: settings.json 已写回 %s（备份 %s，ClockSpeed=%.2f）"),
        *JsonPath, *BackupPath, ClockSpeed);

    // 原地销毁重建 SimMode（延迟一帧，见 ASimHUD::RecreateSimMode）
    for (TActorIterator<ASimHUD> It(GetWorld()); It; ++It)
    {
        It->RecreateSimMode();
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("SimConfig: 未找到 ASimHUD，SimMode 未重建"));
    return false;
}

FString ASimConfigManager::GetSettingsFilePath() const
{
    for (TActorIterator<ASimHUD> It(GetWorld()); It; ++It)
    {
        return It->getSettingsFilePath();
    }
    return FString();
}

// ---- 传感器参数（通道 B）----

TSharedPtr<FJsonObject> ASimConfigManager::LoadSettingsRoot(FString* OutError) const
{
    const FString JsonPath = GetSettingsFilePath();
    FString Text;
    if (JsonPath.IsEmpty() || !FFileHelper::LoadFileToString(Text, *JsonPath))
    {
        if (OutError) *OutError = TEXT("无法读取 settings.json（") + JsonPath + TEXT("）");
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        if (OutError) *OutError = TEXT("settings.json 解析失败");
        return nullptr;
    }
    return Root;
}

FString ASimConfigManager::SensorTypeToName(int32 SensorType)
{
    switch (SensorType)
    {
    case 1:  return TEXT("气压计");
    case 2:  return TEXT("IMU");
    case 3:  return TEXT("GPS");
    case 4:  return TEXT("磁力计");
    case 5:  return TEXT("距离");
    case 6:  return TEXT("Lidar");
    case 7:  return TEXT("Echo");
    case 8:  return TEXT("GPULidar");
    case 9:  return TEXT("模板");
    case 10: return TEXT("UWB");
    case 11: return TEXT("WiFi");
    default: return TEXT("未知类型");
    }
}

namespace
{
    // 将 json 值解析为对象（无效返回空）
    TSharedPtr<FJsonObject> AsJsonObject(const TSharedPtr<FJsonValue>& Value)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (Value.IsValid() && Value->TryGetObject(Obj) && Obj)
        {
            return *Obj;
        }
        return nullptr;
    }

    /**
     * 按字段路径取对象（不创建）。路径元素 "#<下标>" 表示对上一级字段（数组）取元素，
     * 其余元素为对象字段名；空路径即根对象。
     */
    TSharedPtr<FJsonObject> NavigateObject(const TSharedPtr<FJsonObject>& Root, const TArray<FString>& Path)
    {
        TSharedPtr<FJsonValue> Current = MakeShared<FJsonValueObject>(Root);
        for (const FString& Key : Path)
        {
            if (!Current.IsValid())
            {
                return nullptr;
            }
            if (Key.StartsWith(TEXT("#")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
                if (!Current->TryGetArray(Array) || !Array)
                {
                    return nullptr;
                }
                const int32 Index = FCString::Atoi(*Key.Mid(1));
                if (!Array->IsValidIndex(Index))
                {
                    return nullptr;
                }
                Current = (*Array)[Index];
            }
            else
            {
                const TSharedPtr<FJsonObject> Obj = AsJsonObject(Current);
                if (!Obj.IsValid())
                {
                    return nullptr;
                }
                TSharedPtr<FJsonValue> Field = Obj->TryGetField(Key);
                if (!Field.IsValid())
                {
                    return nullptr;
                }
                Current = Field;
            }
        }
        return AsJsonObject(Current);
    }

    /** 按字段路径取对象，缺失的对象字段自动创建（数组元素不支持创建） */
    TSharedPtr<FJsonObject> NavigateOrCreateObject(const TSharedPtr<FJsonObject>& Root, const TArray<FString>& Path)
    {
        TSharedPtr<FJsonValue> Current = MakeShared<FJsonValueObject>(Root);
        for (const FString& Key : Path)
        {
            if (Key.StartsWith(TEXT("#")))
            {
                return nullptr;
            }
            const TSharedPtr<FJsonObject> Obj = AsJsonObject(Current);
            if (!Obj.IsValid())
            {
                return nullptr;
            }
            TSharedPtr<FJsonObject> Next;
            TSharedPtr<FJsonValue> Field = Obj->TryGetField(Key);
            if (Field.IsValid())
            {
                Next = AsJsonObject(Field);
                if (!Next.IsValid())
                {
                    return nullptr;
                }
            }
            else
            {
                Next = MakeShared<FJsonObject>();
                Obj->SetObjectField(Key, Next);
            }
            Current = MakeShared<FJsonValueObject>(Next);
        }
        return AsJsonObject(Current);
    }

    bool LooksLikeNumber(const FString& Value)
    {
        const TCHAR* Chars = *Value;
        bool bHasDigit = false;
        for (int32 i = 0; Chars[i] != '\0'; ++i)
        {
            const TCHAR C = Chars[i];
            if (FChar::IsDigit(C)) { bHasDigit = true; continue; }
            if (C == '+' || C == '-' || C == '.' || C == 'e' || C == 'E') { continue; }
            return false;
        }
        return bHasDigit;
    }
}

int32 ASimConfigManager::DetectValueType(const FString& Value)
{
    const FString Trimmed = Value.TrimStartAndEnd();
    if (Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
        Trimmed.Equals(TEXT("false"), ESearchCase::IgnoreCase))
    {
        return 0;
    }
    if (LooksLikeNumber(Trimmed))
    {
        return 1;
    }
    return 2;
}

void ASimConfigManager::GetEntryList(TArray<FSimSettingsEntry>& OutEntries) const
{
    OutEntries.Reset();
    const TSharedPtr<FJsonObject> Root = LoadSettingsRoot();
    if (!Root)
    {
        return;
    }

    auto MakeEntry = [](TArray<FString>&& InPath, const FString& DisplayName,
        int32 SensorType = 0, bool bSynthetic = false)
    {
        FSimSettingsEntry Entry;
        Entry.Path = MoveTemp(InPath);
        Entry.DisplayName = DisplayName;
        Entry.SensorType = SensorType;
        Entry.bSynthetic = bSynthetic;
        return Entry;
    };

    // 根设置：顶层标量（ClockType/ViewMode/ClockSpeed/InitialInstanceSegmentation 等）
    OutEntries.Add(MakeEntry({}, TEXT("根设置（顶层标量）")));

    // 顶层对象段（Wind/OriginGeopoint/TimeOfDay/Recording/Annotation/CameraDefaults/PawnPaths…）
    // 与数组段（ExternalCameras/SubWindows/WorldLights…，逐元素展开）
    for (const auto& Pair : Root->Values)
    {
        // UE 5.8 中 FJsonObject 键类型为 UE::FSharedString，需显式转 FString
        const FString Key(Pair.Key.Len(), *Pair.Key);
        if (Key == TEXT("Vehicles") || Key == TEXT("DefaultSensors"))
        {
            continue; // 下面单独枚举
        }
        if (Pair.Value->Type == EJson::Object)
        {
            OutEntries.Add(MakeEntry({ Key }, Key));
        }
        else if (Pair.Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
            if (Pair.Value->TryGetArray(Array) && Array)
            {
                for (int32 i = 0; i < Array->Num(); ++i)
                {
                    if (AsJsonObject((*Array)[i]).IsValid())
                    {
                        OutEntries.Add(MakeEntry({ Key, FString::Printf(TEXT("#%d"), i) },
                            FString::Printf(TEXT("%s #%d"), *Key, i)));
                    }
                }
            }
        }
    }

    bool bHasSensorEntries = false;

    // 全局默认传感器段
    if (const TSharedPtr<FJsonObject> DefaultSensors = NavigateObject(Root, { TEXT("DefaultSensors") }))
    {
        for (const auto& Pair : DefaultSensors->Values)
        {
            const FString Name(Pair.Key.Len(), *Pair.Key);
            int32 Type = 0;
            if (const TSharedPtr<FJsonObject> Obj = AsJsonObject(Pair.Value))
            {
                double Num = 0;
                Obj->TryGetNumberField(TEXT("SensorType"), Num);
                Type = static_cast<int32>(Num);
            }
            OutEntries.Add(MakeEntry({ TEXT("DefaultSensors"), Name },
                FString::Printf(TEXT("%s（%s）"), *Name, *SensorTypeToName(Type)), Type));
            bHasSensorEntries = true;
        }
    }

    // 各载具：载具本体、其相机、其传感器
    if (const TSharedPtr<FJsonObject> Vehicles = NavigateObject(Root, { TEXT("Vehicles") }))
    {
        for (const auto& Pair : Vehicles->Values)
        {
            const TSharedPtr<FJsonObject> Vehicle = AsJsonObject(Pair.Value);
            if (!Vehicle.IsValid())
            {
                continue;
            }
            const FString VehicleName(Pair.Key.Len(), *Pair.Key);
            OutEntries.Add(MakeEntry({ TEXT("Vehicles"), VehicleName },
                FString::Printf(TEXT("载具 %s"), *VehicleName)));

            if (const TSharedPtr<FJsonObject> Cameras = NavigateObject(Vehicle, { TEXT("Cameras") }))
            {
                for (const auto& CameraPair : Cameras->Values)
                {
                    if (AsJsonObject(CameraPair.Value).IsValid())
                    {
                        const FString CameraName(CameraPair.Key.Len(), *CameraPair.Key);
                        OutEntries.Add(MakeEntry(
                            { TEXT("Vehicles"), VehicleName, TEXT("Cameras"), CameraName },
                            FString::Printf(TEXT("载具 %s 相机 %s"), *VehicleName, *CameraName)));
                    }
                }
            }

            if (const TSharedPtr<FJsonObject> Sensors = NavigateObject(Vehicle, { TEXT("Sensors") }))
            {
                for (const auto& SensorPair : Sensors->Values)
                {
                    const FString SensorName(SensorPair.Key.Len(), *SensorPair.Key);
                    int32 Type = 0;
                    if (const TSharedPtr<FJsonObject> Obj = AsJsonObject(SensorPair.Value))
                    {
                        double Num = 0;
                        Obj->TryGetNumberField(TEXT("SensorType"), Num);
                        Type = static_cast<int32>(Num);
                    }
                    OutEntries.Add(MakeEntry(
                        { TEXT("Vehicles"), VehicleName, TEXT("Sensors"), SensorName },
                        FString::Printf(TEXT("[%s] %s（%s）"), *VehicleName, *SensorName,
                            *SensorTypeToName(Type)), Type));
                    bHasSensorEntries = true;
                }
            }
        }
    }

    // 传感器完全未定义时，合成多旋翼内置默认四件套
    // （与 AirSimSettings::createDefaultSensorSettings 一致，应用时自动落盘 DefaultSensors 段）
    if (!bHasSensorEntries)
    {
        struct FDefaultSensor { int32 Type; const TCHAR* Name; };
        const FDefaultSensor Defaults[] = {
            { 2, TEXT("imu") }, { 4, TEXT("magnetometer") },
            { 3, TEXT("gps") }, { 1, TEXT("barometer") },
        };
        for (const FDefaultSensor& Default : Defaults)
        {
            OutEntries.Add(MakeEntry({ TEXT("DefaultSensors"), Default.Name },
                FString::Printf(TEXT("%s（%s，内置默认）"), Default.Name,
                    *SensorTypeToName(Default.Type)), Default.Type, true));
        }
    }
}

void ASimConfigManager::GetEntryParams(const FSimSettingsEntry& Entry, TArray<FSimSensorParam>& OutParams) const
{
    OutParams.Reset();

    // 合成条目：json 里还没有，给一个最小骨架（SensorType/Enabled），应用时创建
    if (Entry.bSynthetic)
    {
        OutParams.Add({ TEXT("SensorType"), FString::FromInt(Entry.SensorType), 1 });
        OutParams.Add({ TEXT("Enabled"), TEXT("true"), 0 });
        return;
    }

    const TSharedPtr<FJsonObject> Root = LoadSettingsRoot();
    if (!Root)
    {
        return;
    }
    const TSharedPtr<FJsonObject> Target = NavigateObject(Root, Entry.Path);
    if (!Target.IsValid())
    {
        return;
    }

    for (const auto& Pair : Target->Values)
    {
        FSimSensorParam Param;
        Param.Key = FString(Pair.Key.Len(), *Pair.Key);
        const TSharedPtr<FJsonValue>& Value = Pair.Value;
        switch (Value->Type)
        {
        case EJson::Boolean:
            Param.Type = 0;
            Param.RawValue = Value->AsBool() ? TEXT("true") : TEXT("false");
            break;
        case EJson::Number:
            Param.Type = 1;
            Param.RawValue = FString::SanitizeFloat(Value->AsNumber());
            break;
        case EJson::String:
            Param.Type = 2;
            Param.RawValue = Value->AsString();
            break;
        default:
            // 对象/数组等复合字段不在此编辑：有枚举子条目的走下拉框，
            // 其余（如相机的 NoiseSettings/Gimbal）在 settings.json 手工编辑后重建同样生效
            continue;
        }
        OutParams.Add(Param);
    }
}

bool ASimConfigManager::ApplyEntryParams(const FSimSettingsEntry& Entry, const TArray<FSimSensorParam>& Params)
{
    const TSharedPtr<FJsonObject> Root = LoadSettingsRoot();
    if (!Root)
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: 参数写回失败，无法读取 %s"), *GetSettingsFilePath());
        return false;
    }

    // 定位目标对象；合成条目（内置默认传感器）自动创建缺失的段
    TSharedPtr<FJsonObject> Target;
    if (Entry.bSynthetic)
    {
        Target = NavigateOrCreateObject(Root, Entry.Path);
        if (!Target.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("SimConfig: 无法创建条目路径"));
            return false;
        }
        if (!Target->HasField(TEXT("SensorType")))
        {
            Target->SetNumberField(TEXT("SensorType"), Entry.SensorType);
        }
        if (!Target->HasField(TEXT("Enabled")))
        {
            Target->SetBoolField(TEXT("Enabled"), true);
        }
    }
    else
    {
        Target = NavigateObject(Root, Entry.Path);
        if (!Target.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("SimConfig: 条目定位失败：%s"), *Entry.DisplayName);
            return false;
        }
    }

    // 应用键值（Type==3 为只读展示项，跳过）
    for (const FSimSensorParam& Param : Params)
    {
        if (Param.Type == 3 || Param.Key.IsEmpty())
        {
            continue;
        }
        switch (Param.Type)
        {
        case 0:
            Target->SetBoolField(Param.Key,
                Param.RawValue == TEXT("true") || Param.RawValue == TEXT("1"));
            break;
        case 1:
            Target->SetNumberField(Param.Key, FCString::Atod(*Param.RawValue));
            break;
        default:
            Target->SetStringField(Param.Key, Param.RawValue);
            break;
        }
    }

    // 序列化（美化缩进）→ 备份 → 落盘 → 重建 SimMode
    FString OutText;
    const auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutText);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer.Get());
    Writer->Close();

    if (!PersistSettingsText(OutText))
    {
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("SimConfig: 条目 %s 已写回并重建 SimMode"), *Entry.DisplayName);

    for (TActorIterator<ASimHUD> It(GetWorld()); It; ++It)
    {
        It->RecreateSimMode();
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("SimConfig: 未找到 ASimHUD，SimMode 未重建"));
    return false;
}

bool ASimConfigManager::PersistSettingsText(const FString& NewText)
{
    const FString JsonPath = GetSettingsFilePath();
    if (JsonPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: 无法定位 settings.json，放弃写回"));
        return false;
    }

    // 写前备份，防止写坏后无配置可用（AirSim 读不到合法 settings 会自动生成默认文件）
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CopyFile(*(JsonPath + TEXT(".bak")), *JsonPath);
    if (!FFileHelper::SaveStringToFile(NewText, *JsonPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("SimConfig: settings.json 写入失败"));
        return false;
    }

    // 同步到 Settings 单例（nlohmann doc_），保证后续 ClockSpeed 写回 / 重建读到的内容一致
    msr::airlib::Settings::loadJSonString(TCHAR_TO_UTF8(*NewText));
    return true;
}
