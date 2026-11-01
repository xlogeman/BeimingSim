#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimConfigManager.generated.h"

class USimConfigWidget;

/**
 * settings.json 可编辑对象条目：从根到某个 json 对象的字段路径。
 * 覆盖根设置、任意顶层对象段（Wind/OriginGeopoint/TimeOfDay/Recording/
 * Annotation/CameraDefaults/PawnPaths…）、各载具、各载具相机、外部相机、
 * 全部传感器（DefaultSensors + 载具级）以及数组元素（外部相机 #i 等）。
 */
struct FSimSettingsEntry
{
    /**
     * 从根到目标对象的字段路径；数组元素用 "#<下标>" 表示
     * （如 ["Vehicles","MyDrone","Cameras","front"]、["ExternalCameras","#0"]；根设置为空）。
     */
    TArray<FString> Path;
    /** 面板显示名 */
    FString DisplayName;
    /** 传感器条目的 SensorType（AirLib SensorBase.hpp：1 气压计 2 IMU 3 GPS 4 磁力计 5 距离 6 Lidar 7 Echo 8 GPULidar 9 模板 10 UWB 11 WiFi） */
    int32 SensorType = 0;
    /** settings.json 中尚不存在该定义（当前用内置默认），应用时自动创建 */
    bool bSynthetic = false;
    /** 载具相机条目（Vehicles.X.Cameras.Y）——面板为其注入 Enabled 开关（AirSimSettings 已支持解析） */
    bool bCamera = false;
};

/** 条目内单个参数（键值对，值以字符串形式传递与展示） */
struct FSimSensorParam
{
    FString Key;
    FString RawValue;
    /** 0=布尔 1=数值 2=字符串 3=其他（只读展示，不写回） */
    int32 Type = 2;
};

/**
 * 仿真配置管理器：运行时按 O 呼出配置面板（USimConfigWidget），
 * 把面板配置落到 AirSim 运行时 API 或 settings.json。
 * 由 BeimingSimGameMode 在 BeginPlay 自动生成，无需在关卡里摆放。
 *
 * 两条生效通道：
 * - 即时：天气/风/相机 FOV/暂停/时间模拟 → UWeatherLib 与 ASimModeBase 的运行时 setter；
 * - 重建：时钟速度等结构性配置 → 写回 settings.json（自动备份 .bak）→
 *   调用 ASimHUD::RecreateSimMode() 原地销毁重建 SimMode（API server 一并重启）。
 */
UCLASS()
class BEIMINGSIM_API ASimConfigManager : public AActor
{
    GENERATED_BODY()

public:
    ASimConfigManager();

    virtual void BeginPlay() override;

    /** 即时应用：天气/风（NED m/s）/默认相机 FOV/暂停（不重建 SimMode） */
    void ApplyLive(bool bWeatherEnabled, float Rain, float Snow, float Dust, float Fog,
        const FVector& WindNed, float CameraFov, bool bPaused);

    /** 时间模拟预设：0 清晨 06:30 / 1 正午 12:00 / 2 黄昏 18:30 / 3 夜晚 22:00 / 其它=关闭 */
    void ApplyTimeOfDay(int32 PresetIndex);

    /**
     * 写回 settings.json（自动备份为 .bak）并原地销毁重建 SimMode。
     * 当前支持的结构化配置：时钟速度（ClockSpeed）；
     * 写回后 SimMode 按新配置重建，也可先手工编辑 json 再点重建（重建时以文件为准）。
     */
    bool ApplyAndRebuild(float ClockSpeed);

    /** 当前实际生效的 settings.json 路径（找不到返回空串） */
    FString GetSettingsFilePath() const;

    // ---- settings.json 对象编辑（通道 B：写回 settings.json → 原地重建 SimMode 生效）----

    /**
     * 列出 settings.json 中全部可编辑对象条目：根设置、顶层对象段、数组元素、
     * 各载具、载具相机、全部传感器（DefaultSensors + 各载具 Sensors）。
     * 传感器完全未定义时合成多旋翼默认四件套（bSynthetic=true），应用时自动落盘。
     */
    void GetEntryList(TArray<FSimSettingsEntry>& OutEntries) const;

    /** 读取某个条目对象的全部键值对（标量可编辑；对象/数组复合值只读展示） */
    void GetEntryParams(const FSimSettingsEntry& Entry, TArray<FSimSensorParam>& OutParams) const;

    /**
     * 写回某个条目的参数（Type==3 的只读项自动跳过），自动备份 .bak 并原地重建 SimMode。
     * ClockSpeed >= 0 时同时把时钟速度写入根设置（单次落盘、单次重建）。
     */
    bool ApplyEntryParams(const FSimSettingsEntry& Entry, const TArray<FSimSensorParam>& Params,
        float ClockSpeed = -1.f);

    /** 值字符串类型探测（面板"新增参数"用）：0 布尔 / 1 数值 / 2 字符串 */
    static int32 DetectValueType(const FString& Value);

private:
    void CreateWidgetInternal();
    void BindOpenKey();
    UFUNCTION()
    void TogglePanel();

    /** 解析当前 settings.json 为 UE JSON 根对象，失败返回空并填 OutError */
    TSharedPtr<FJsonObject> LoadSettingsRoot(FString* OutError = nullptr) const;
    /** 备份现有文件（.bak）→ 写入新文本 → 同步到 Settings 单例 */
    bool PersistSettingsText(const FString& NewText);
    /** 传感器类型编号 → 显示名 */
    static FString SensorTypeToName(int32 SensorType);

    UPROPERTY()
    TObjectPtr<USimConfigWidget> PanelWidget = nullptr;

    FTimerHandle CreateWidgetTimer;
    FTimerHandle BindRetryTimer;
};
