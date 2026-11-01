#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimConfigManager.generated.h"

class USimConfigWidget;

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

private:
    void CreateWidgetInternal();
    void BindOpenKey();
    UFUNCTION()
    void TogglePanel();

    UPROPERTY()
    TObjectPtr<USimConfigWidget> PanelWidget = nullptr;

    FTimerHandle CreateWidgetTimer;
    FTimerHandle BindRetryTimer;
};
