#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneSwitchWidget.generated.h"

class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTexture;
class UTextBlock;

/**
 * 机型选择侧滑面板。
 * 纯 C++ 构建 UMG（不需要在编辑器里创建 WBP 资产）：
 * 左侧圆角半透明面板，Tab 展开/收起；每个条目含 3D 预览缩略图与机型名，
 * 支持悬停高亮与当前选中机型高亮。
 */
UCLASS()
class BEIMINGSIM_API UDroneSwitchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDroneModelPicked, int32, Index);

    /** 点击某个机型按钮时广播（参数为按钮下标） */
    FOnDroneModelPicked OnModelPicked;

    /** 用机型名构建按钮列表，需在 AddToViewport 之前调用；缩略图可稍后用 SetEntryThumbnail 补充 */
    void BuildMenu(const TArray<FString>& InModelNames);

    /** 设置某个机型的预览缩略图 */
    void SetEntryThumbnail(int32 Index, UTexture* Thumbnail);

    /** 高亮当前选中的机型条目 */
    void HighlightSelected(int32 Index);

    /** 展开/收起面板（带滑动动画） */
    void ToggleMenu();

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // UButton 的 OnClicked 只支持动态委托，这里用固定槽位的 UFUNCTION 分发（最多 8 个机型）
    UFUNCTION() void OnPick0();
    UFUNCTION() void OnPick1();
    UFUNCTION() void OnPick2();
    UFUNCTION() void OnPick3();
    UFUNCTION() void OnPick4();
    UFUNCTION() void OnPick5();
    UFUNCTION() void OnPick6();
    UFUNCTION() void OnPick7();

private:
    UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color) const;
    UButton* MakeEntryButton(int32 Index);
    void ApplyEntryStyle(int32 Index);

    UPROPERTY()
    UCanvasPanel* RootCanvas = nullptr;

    UPROPERTY()
    UCanvasPanelSlot* PanelSlot = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<UButton>> EntryButtons;

    UPROPERTY()
    TArray<TObjectPtr<UImage>> EntryImages;

    UPROPERTY()
    TArray<TObjectPtr<UTextBlock>> EntryLabels;

    TArray<FString> ModelNames;
    int32 SelectedIndex = 0;
    bool bOpen = false;
    float CurrentX = 390.f;
    float TargetX = 390.f;
    float PanelWidth = 330.f;
};
