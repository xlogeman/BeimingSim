#include "SimConfigManager.h"
#include "SimConfigWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

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
