#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SimConfigManager.h"
#include "Types/SlateEnums.h"
#include "SimConfigWidget.generated.h"

class ASimConfigManager;
class UBorder;
class UButton;
class UCheckBox;
class UCanvasPanel;
class UCanvasPanelSlot;
class USlider;
class UTextBlock;
class UVerticalBox;

/**
 * 仿真环境配置面板（纯 C++ 构建 UMG，无需 WBP 资产，样式与 DroneSwitchWidget 一致）。
 * 按 O 从屏幕右侧滑出，分组编辑 AirSim 运行环境参数，仅两个操作按钮：
 *
 * - 【应用（即时生效）】天气（雨/雪/尘/雾）、风（NED m/s）、相机 FOV、暂停：
 *   调用 AirSim 运行时 API（UWeatherLib / ASimModeBase setter），不重建 SimMode、
 *   不写 settings.json。
 * settings.json 的对象级编辑在独立面板 SimSettingsWidget（机型菜单内各条目的设置按钮呼出）。
 */
UCLASS()
class BEIMINGSIM_API USimConfigWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 构建面板并绑定管理器，需在 AddToViewport 之前调用 */
    void BuildPanel(ASimConfigManager* InManager);

    /** 用场景当前状态刷新控件显示值（天气/暂停/时钟速度可回读，风与 FOV 保持用户输入） */
    void RefreshFromWorld();

    /** 展开/收起面板（带滑动动画） */
    void TogglePanel();

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // UMG 动态委托不支持绑定参数，滑条/时间按钮用固定槽位分发
    UFUNCTION() void OnSlider0(float Value);
    UFUNCTION() void OnSlider1(float Value);
    UFUNCTION() void OnSlider2(float Value);
    UFUNCTION() void OnSlider3(float Value);
    UFUNCTION() void OnSlider4(float Value);
    UFUNCTION() void OnSlider5(float Value);
    UFUNCTION() void OnSlider6(float Value);
    UFUNCTION() void OnSlider7(float Value);

    UFUNCTION() void OnWeatherEnabledChanged(bool bChecked);
    UFUNCTION() void OnPauseChanged(bool bChecked);
    UFUNCTION() void OnApplyLive();

    UFUNCTION() void OnTimeMorning();
    UFUNCTION() void OnTimeNoon();
    UFUNCTION() void OnTimeEvening();
    UFUNCTION() void OnTimeNight();
    UFUNCTION() void OnTimeDisable();

private:
    /** 滑条在 Sliders/ValueLabels 数组中的固定下标 */
    enum ESliderIndex
    {
        Rain = 0,
        Snow,
        Dust,
        Fog,
        WindX,
        WindY,
        WindZ,
        CameraFov,
        SliderCount
    };

    UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color, bool bBold = false) const;
    /** 创建一行"名称 + 滑条 + 数值"，返回创建的滑条（回调绑定由 BuildPanel 统一完成） */
    USlider* MakeSliderRow(UVerticalBox* Parent, const FString& Name, ESliderIndex Index,
        float Min, float Max, float Value);
    UButton* MakeButton(const FString& Text, bool bPrimary = false);
    void AddSectionHeader(UVerticalBox* Parent, const FString& Title);
    void OnSliderChanged(ESliderIndex Index, float Value);
    void UpdateValueLabel(ESliderIndex Index);
    void SetStatusText(const FString& Text);

    UPROPERTY()
    UCanvasPanel* RootCanvas = nullptr;

    UPROPERTY()
    UCanvasPanelSlot* PanelSlot = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<USlider>> Sliders;

    UPROPERTY()
    TArray<TObjectPtr<UTextBlock>> ValueLabels;

    UPROPERTY()
    TObjectPtr<UCheckBox> WeatherCheck = nullptr;

    UPROPERTY()
    TObjectPtr<UCheckBox> PauseCheck = nullptr;

    UPROPERTY()
    TObjectPtr<UTextBlock> StatusLabel = nullptr;

    TWeakObjectPtr<ASimConfigManager> Manager;

    bool bOpen = false;
    float CurrentX = 460.f;
    float TargetX = 460.f;
    static constexpr float PanelWidth = 400.f;
};
