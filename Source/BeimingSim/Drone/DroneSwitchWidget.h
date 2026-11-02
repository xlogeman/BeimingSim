#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneSwitchWidget.generated.h"

class UButton;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTexture;
class UTextBlock;
class UVerticalBox;

/** 单条机型参数（名称 + 值），按数组顺序展示 */
USTRUCT(BlueprintType)
struct FDroneModelParam
{
    GENERATED_BODY()

    /** 参数名，如 "轴距"、"最大起飞重量" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DroneSwitch")
    FString Name;

    /** 参数值，如 "450 mm"、"1.4 kg" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DroneSwitch")
    FText Value;
};

/** 机型的介绍与参数（悬停预览弹窗内容） */
USTRUCT(BlueprintType)
struct FDroneModelInfo
{
    GENERATED_BODY()

    /** 机型介绍文字，留空则显示兜底文案 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DroneSwitch")
    FText Description;

    /** 参数列表，按数组顺序展示 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DroneSwitch")
    TArray<FDroneModelParam> Params;
};

/**
 * 机型选择侧滑面板。
 * 纯 C++ 构建 UMG（不需要在编辑器里创建 WBP 资产）：
 * 左侧圆角半透明面板，Tab 展开/收起；每个条目含 3D 预览缩略图与机型名，
 * 支持悬停高亮与当前选中机型高亮；当前选中的机型不可再次点击。
 * 悬停条目时在菜单面板左侧弹出固定位置的大图预览与机型介绍/参数（不随鼠标移动）。
 */
UCLASS()
class BEIMINGSIM_API UDroneSwitchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDroneModelPicked, int32, Index);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsRequested, int32, ModelIndex);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuToggled, bool, bOpen);

    /** 点击某个机型按钮时广播（参数为按钮下标） */
    FOnDroneModelPicked OnModelPicked;

    /** 点击某机型条目行尾"设置"按钮时广播（参数为机型下标） */
    FOnSettingsRequested OnSettingsRequested;

    /** 菜单展开/收起时广播（收起时用于联动关闭 settings.json 配置面板） */
    FOnMenuToggled OnMenuToggled;

    /** 用机型名构建按钮列表，需在 AddToViewport 之前调用；缩略图可稍后用 SetEntryThumbnail 补充 */
    void BuildMenu(const TArray<FString>& InModelNames);

    /** 设置某个机型的预览缩略图 */
    void SetEntryThumbnail(int32 Index, UTexture* Thumbnail);

    /** 设置某个机型的介绍与参数（悬停弹窗内容），需在 AddToViewport 之前调用 */
    void SetEntryInfo(int32 Index, const FDroneModelInfo& Info);

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

    // 悬停事件同样只支持无参动态委托，用固定槽位分发；取消悬停统一走 OnEntryUnhover
    UFUNCTION() void OnHover0();
    UFUNCTION() void OnHover1();
    UFUNCTION() void OnHover2();
    UFUNCTION() void OnHover3();
    UFUNCTION() void OnHover4();
    UFUNCTION() void OnHover5();
    UFUNCTION() void OnHover6();
    UFUNCTION() void OnHover7();
    UFUNCTION() void OnEntryUnhover();
    // 行尾"设置"按钮：同样受动态委托无参限制，按机型下标用固定槽位分发
    UFUNCTION() void OnSettings0();
    UFUNCTION() void OnSettings1();
    UFUNCTION() void OnSettings2();
    UFUNCTION() void OnSettings3();
    UFUNCTION() void OnSettings4();
    UFUNCTION() void OnSettings5();
    UFUNCTION() void OnSettings6();
    UFUNCTION() void OnSettings7();

private:
    UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color) const;
    UButton* MakeEntryButton(int32 Index);
    void ApplyEntryStyle(int32 Index);
    /** 统一的选中入口：当前选中的机型直接忽略，不广播也不收起面板 */
    void RequestPick(int32 Index);
    /** 构建悬停预览弹窗（默认隐藏） */
    void MakeTooltipPanel();
    /** 显示第 Index 个机型的悬停预览弹窗 */
    void ShowTooltip(int32 Index);
    /** 隐藏悬停预览弹窗 */
    void HideTooltip();
    /** 统一的设置入口：广播机型下标 */
    void RequestSettings(int32 Index);

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

    /** 与条目一一对应的机型缩略图纹理（悬停弹窗用大图） */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture>> EntryTextures;

    /** 与条目一一对应的机型介绍/参数 */
    TArray<FDroneModelInfo> EntryInfos;

    // ---- 悬停预览弹窗 ----
    UPROPERTY()
    TObjectPtr<UBorder> TooltipBorder = nullptr;

    UPROPERTY()
    UCanvasPanelSlot* TooltipSlot = nullptr;

    /** 菜单面板 Border（弹窗定位参考：贴其左缘） */
    UPROPERTY()
    TObjectPtr<UBorder> PanelBorder = nullptr;

    UPROPERTY()
    TObjectPtr<UImage> TooltipImage = nullptr;

    UPROPERTY()
    TObjectPtr<UTextBlock> TooltipNameLabel = nullptr;

    UPROPERTY()
    TObjectPtr<UTextBlock> TooltipDescLabel = nullptr;

    UPROPERTY()
    TObjectPtr<UVerticalBox> TooltipParamBox = nullptr;

    TArray<FString> ModelNames;
    int32 SelectedIndex = 0;
    bool bOpen = false;
    float CurrentX = 390.f;
    float TargetX = 390.f;
    float PanelWidth = 330.f;
};
