#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneSwitchWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UVerticalBox;

/**
 * 机型选择侧滑面板。
 * 纯 C++ 构建 UMG（不需要在编辑器里创建 WBP 资产）：
 * 左侧半透明面板，Tab 展开/收起，内含机型按钮列表。
 */
UCLASS()
class BEIMINGSIM_API UDroneSwitchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDroneModelPicked, int32, Index);

    /** 点击某个机型按钮时广播（参数为按钮下标） */
    FOnDroneModelPicked OnModelPicked;

    /** 用机型名构建按钮列表，需在 AddToViewport 之前调用 */
    void BuildMenu(const TArray<FString>& InModelNames);

    /** 展开/收起面板（带滑动动画） */
    void ToggleMenu();

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // UButton 的 OnClicked 只支持动态委托，这里用固定槽位的 UFUNCTION 分发
    UFUNCTION() void OnPick0();
    UFUNCTION() void OnPick1();
    UFUNCTION() void OnPick2();
    UFUNCTION() void OnPick3();

private:
    UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color) const;

    UPROPERTY()
    UCanvasPanel* RootCanvas = nullptr;

    UPROPERTY()
    UCanvasPanelSlot* PanelSlot = nullptr;

    TArray<FString> ModelNames;
    bool bOpen = false;
    float CurrentX = -340.f;
    float TargetX = -340.f;
    float PanelWidth = 300.f;
};
