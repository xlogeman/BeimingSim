#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SimConfigManager.h"
#include "Types/SlateEnums.h"
#include "SimSettingsWidget.generated.h"

class ASimConfigManager;
class UBorder;
class UButton;
class UCheckBox;
class UComboBoxString;
class UCanvasPanel;
class UCanvasPanelSlot;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * settings.json 配置面板（纯 C++ 构建 UMG，无需 WBP 资产，样式与 DroneSwitchWidget 一致）。
 * 由机型选择菜单内各机型条目的"设置"按钮呼出，屏幕居中显示（淡入淡出）。
 * 仅编辑 settings.json：下拉框选择可编辑对象条目（根设置/载具/相机/传感器…），
 * 修改参数后"保存并重建"一次写入文件（自动备份 .bak）并原地重建 SimMode 生效。
 * 从某机型条目呼出且 settings 中存在同名载具条目时，自动选中该载具。
 * 环境调节（天气/风/FOV/时间等）在独立的 SimConfigWidget（O 键呼出）。
 */
UCLASS()
class BEIMINGSIM_API USimSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 构建面板并绑定管理器，需在 AddToViewport 之前调用 */
    void BuildPanel(ASimConfigManager* InManager);

    /** 从 settings.json 回读刷新条目参数显示（json 可能被手工编辑过） */
    void RefreshFromSettings();

    /** 展开面板并尽量选中指定名称的条目（如机型对应的载具）；名称为空/未命中则保持当前选中 */
    void OpenPanel(const FString& PreferEntryName = TEXT(""));

    /** 展开/收起面板（居中淡入淡出） */
    void TogglePanel();

    /** 收起面板（机型菜单关闭时联动调用） */
    void ClosePanel() { bOpen = false; }

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION() void OnSaveRebuild();
    UFUNCTION() void OnCloseClicked();

    // ---- settings 对象编辑区（写回 settings.json → 重建 SimMode 生效）----
    UFUNCTION() void OnEntrySelected(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnEntryParamAdd();

private:
    UButton* MakeButton(const FString& Text, bool bPrimary = false);
    UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color, bool bBold = false) const;
    void AddSectionHeader(UVerticalBox* Parent, const FString& Title);
    /** 构建"settings 对象"区：下拉框 + 参数键值行 + 新增参数行 */
    void AddEntrySection(UVerticalBox* Content);
    /** 按当前选中条目重建参数键值行；bFetchFromManager=false 时保留内存中的 SensorParams（新增参数后重建行用） */
    void RebuildSensorRows(bool bFetchFromManager = true);
    /** 选中指定显示名的条目并重建参数行；名称为空或未命中时保持当前选中 */
    void SelectEntryByName(const FString& Name);
    void SetStatusText(const FString& Text);

    UPROPERTY()
    UCanvasPanel* RootCanvas = nullptr;

    UPROPERTY()
    UCanvasPanelSlot* PanelSlot = nullptr;

    /** 面板根 Border（居中显示 + 淡入淡出动画作用对象） */
    UPROPERTY()
    TObjectPtr<UBorder> PanelRoot = nullptr;

    UPROPERTY()
    TObjectPtr<UTextBlock> StatusLabel = nullptr;

    // ---- settings 对象编辑区 ----
    UPROPERTY()
    TObjectPtr<UComboBoxString> EntryCombo = nullptr;

    UPROPERTY()
    TObjectPtr<UVerticalBox> SensorRowsBox = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<UCheckBox>> SensorChecks;

    UPROPERTY()
    TArray<TObjectPtr<UEditableTextBox>> SensorTexts;

    UPROPERTY()
    TObjectPtr<UEditableTextBox> NewParamKeyBox = nullptr;

    UPROPERTY()
    TObjectPtr<UEditableTextBox> NewParamValueBox = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<UWidget>> SensorRowWidgets;

    /** 当前条目的参数列表（与键值行一一对应） */
    TArray<FSimSensorParam> SensorParams;
    /** 与键值行一一对应的行类型（0 布尔 / 1 数值 / 2 字符串 / 3 只读） */
    TArray<int32> SensorRowTypes;
    /** 候选条目列表（与下拉框选项一一对应） */
    TArray<FSimSettingsEntry> EntryRefs;
    FSimSettingsEntry CurrentEntryRef;

    TWeakObjectPtr<ASimConfigManager> Manager;

    bool bOpen = false;
    /** 面板透明度（0 收起 → 1 展开，淡入淡出） */
    float CurrentAlpha = 0.f;
};
