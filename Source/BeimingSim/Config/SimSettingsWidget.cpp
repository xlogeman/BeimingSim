#include "SimSettingsWidget.h"
#include "SimConfigManager.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"

namespace
{
    // 与 DroneSwitchWidget 保持一致的配色
    const FLinearColor PanelBg(0.04f, 0.05f, 0.09f, 0.94f);
    const FLinearColor Accent(0.15f, 0.62f, 0.95f, 1.f);
    const FLinearColor AccentHover(0.24f, 0.72f, 1.f, 1.f);
    const FLinearColor AccentPress(0.10f, 0.48f, 0.78f, 1.f);
    const FLinearColor ParamLabelColor(0.62f, 0.68f, 0.76f, 1.f);
    const FLinearColor NoteColor(0.55f, 0.60f, 0.68f, 1.f);
    const FLinearColor InputBg(0.09f, 0.11f, 0.17f, 1.f);
    const FLinearColor InputFocusBg(0.07f, 0.09f, 0.14f, 1.f);
    const FLinearColor BtnBg(0.13f, 0.16f, 0.24f, 1.f);
    const FLinearColor BtnHoverBg(0.19f, 0.24f, 0.35f, 1.f);
    const FLinearColor BtnPressBg(0.10f, 0.12f, 0.19f, 1.f);
    constexpr float PanelRadius = 12.f;
    constexpr float ScrollHeight = 600.f;

    /** 圆角矩形刷子（可指定尺寸，用于需要固定大小的场合） */
    FSlateRoundedBoxBrush MakeRoundBrush(const FLinearColor& Color, float Radius, FVector2D Size = FVector2D::ZeroVector)
    {
        FSlateRoundedBoxBrush Brush(Color, Radius);
        if (Size != FVector2D::ZeroVector)
        {
            Brush.ImageSize = Size;
        }
        return Brush;
    }

    /** 带描边的圆角矩形刷子 */
    FSlateRoundedBoxBrush MakeRoundBrush(const FLinearColor& Color, float Radius,
        const FLinearColor& OutlineColor, float OutlineWidth, FVector2D Size = FVector2D::ZeroVector)
    {
        FSlateRoundedBoxBrush Brush(Color, Radius, OutlineColor, OutlineWidth);
        if (Size != FVector2D::ZeroVector)
        {
            Brush.ImageSize = Size;
        }
        return Brush;
    }

    FButtonStyle MakeButtonStyle(bool bPrimary)
    {
        const FLinearColor Normal = bPrimary ? Accent : BtnBg;
        const FLinearColor Hover = bPrimary ? AccentHover : BtnHoverBg;
        const FLinearColor Pressed = bPrimary ? AccentPress : BtnPressBg;
        FButtonStyle Style;
        Style.SetNormal(MakeRoundBrush(Normal, 6.f));
        Style.SetHovered(MakeRoundBrush(Hover, 6.f));
        Style.SetPressed(MakeRoundBrush(Pressed, 6.f));
        Style.SetNormalPadding(FMargin(12, 4));
        Style.SetPressedPadding(FMargin(12, 4));
        return Style;
    }

    FCheckBoxStyle MakeCheckBoxStyle()
    {
        FCheckBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CheckBox");
        const FVector2D Size(18.f, 18.f);
        const FLinearColor TrackBg(0.24f, 0.27f, 0.34f, 1.f);
        Style.SetUncheckedImage(MakeRoundBrush(TrackBg, 4.f, Size));
        Style.SetUncheckedHoveredImage(MakeRoundBrush(FLinearColor(0.30f, 0.34f, 0.42f, 1.f), 4.f, Size));
        Style.SetUncheckedPressedImage(MakeRoundBrush(TrackBg, 4.f, Size));
        Style.SetCheckedImage(MakeRoundBrush(Accent, 4.f, Size));
        Style.SetCheckedHoveredImage(MakeRoundBrush(AccentHover, 4.f, Size));
        Style.SetCheckedPressedImage(MakeRoundBrush(AccentPress, 4.f, Size));
        return Style;
    }

    FEditableTextBoxStyle MakeTextBoxStyle()
    {
        FEditableTextBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
        Style.SetBackgroundImageNormal(MakeRoundBrush(InputBg, 4.f));
        Style.SetBackgroundImageHovered(MakeRoundBrush(InputBg, 4.f, FLinearColor::White, 1.f));
        Style.SetBackgroundImageFocused(MakeRoundBrush(InputFocusBg, 4.f, Accent, 1.f));
        Style.SetBackgroundImageReadOnly(MakeRoundBrush(InputBg, 4.f));
        Style.SetForegroundColor(FSlateColor(FLinearColor::White));
        Style.SetReadOnlyForegroundColor(FSlateColor(ParamLabelColor));
        Style.SetPadding(FMargin(6, 3));
        return Style;
    }

    FComboBoxStyle MakeComboStyle()
    {
        FComboBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
        Style.ComboButtonStyle.ButtonStyle = MakeButtonStyle(false);
        Style.ComboButtonStyle.MenuBorderBrush = MakeRoundBrush(PanelBg, 6.f, FLinearColor::White, 1.f);
        return Style;
    }
}

void USimSettingsWidget::BuildPanel(ASimConfigManager* InManager)
{
    Manager = InManager;

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    WidgetTree->RootWidget = RootCanvas;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrush(FSlateRoundedBoxBrush(PanelBg, PanelRadius, Accent, 1.f));
    Panel->SetPadding(FMargin(14.f, 12.f));
    Panel->SetHorizontalAlignment(HAlign_Fill);
    PanelRoot = Panel;
    // 初始隐藏（bOpen=false，由淡入动画控制显示）
    Panel->SetVisibility(ESlateVisibility::Collapsed);
    Panel->SetRenderOpacity(0.f);

    // 锚定屏幕正中央，面板中心对齐锚点（经机型条目"设置"按钮呼出）
    PanelSlot = RootCanvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    PanelSlot->SetAutoSize(true);
    PanelSlot->SetZOrder(60);

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
    Panel->SetContent(Box);

    // 标题行（右缘带关闭按钮）+ 强调色下划线
    UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
    UHorizontalBoxSlot* TitleSlot = TitleRow->AddChildToHorizontalBox(
        MakeLabel(TEXT("settings.json 配置"), 20, FLinearColor::White, true));
    TitleSlot->SetVerticalAlignment(VAlign_Center);
    TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UButton* CloseBtn = MakeButton(TEXT("关闭"));
    CloseBtn->OnClicked.AddDynamic(this, &USimSettingsWidget::OnCloseClicked);
    UHorizontalBoxSlot* CloseSlot = TitleRow->AddChildToHorizontalBox(CloseBtn);
    CloseSlot->SetVerticalAlignment(VAlign_Center);
    Box->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(4, 0, 0, 6));

    UBorder* AccentBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AccentBar"));
    AccentBar->SetBrush(FSlateRoundedBoxBrush(Accent, 2.f));
    USpacer* BarFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("BarFill"));
    BarFill->SetSize(FVector2D(64.f, 3.f));
    AccentBar->SetContent(BarFill);
    Box->AddChildToVerticalBox(AccentBar)->SetPadding(FMargin(4, 0, 0, 10));

    // 固定高度滚动区（配置项较多时内部滚动，面板整体尺寸不变）
    USizeBox* ScrollArea = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScrollArea"));
    ScrollArea->SetHeightOverride(ScrollHeight);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
    ScrollArea->SetContent(Scroll);
    Box->AddChildToVerticalBox(ScrollArea)->SetPadding(FMargin(0, 2));

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
    Scroll->AddChild(Content);

    // ---- settings.json 对象编辑 ----
    AddEntrySection(Content);

    // ---- 状态与操作按钮 ----
    StatusLabel = MakeLabel(TEXT(""), 12, FLinearColor::Yellow);
    Content->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(4, 8, 0, 2));

    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BtnRow"));
    UButton* SaveRebuildBtn = MakeButton(TEXT("保存并重建"), true);
    SaveRebuildBtn->OnClicked.AddDynamic(this, &USimSettingsWidget::OnSaveRebuild);
    UHorizontalBoxSlot* RebuildSlot = BtnRow->AddChildToHorizontalBox(SaveRebuildBtn);
    RebuildSlot->SetPadding(FMargin(2, 0));
    RebuildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Box->AddChildToVerticalBox(BtnRow)->SetPadding(FMargin(0, 6, 0, 0));

    Box->AddChildToVerticalBox(MakeLabel(TEXT("机型菜单内条目右侧“设置”按钮 呼出"), 12, FLinearColor::Gray))
        ->SetPadding(FMargin(4, 8, 0, 0));

    RefreshFromSettings();
}

void USimSettingsWidget::RefreshFromSettings()
{
    // 打开/刷新时回读条目参数显示（json 可能被手工编辑过）
    if (Manager.IsValid() && !CurrentEntryRef.DisplayName.IsEmpty() &&
        EntryCombo && EntryCombo->GetSelectedOption().Len() > 0)
    {
        RebuildSensorRows();
    }
    SetStatusText(TEXT(""));
}

void USimSettingsWidget::OpenPanel(const FString& PreferEntryName)
{
    bOpen = true;
    RefreshFromSettings();
    SelectEntryByName(PreferEntryName);
}

void USimSettingsWidget::TogglePanel()
{
    bOpen = !bOpen;
    if (bOpen)
    {
        RefreshFromSettings();
    }
}

void USimSettingsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 居中面板淡入淡出（收起后彻底隐藏，不遮挡视野）
    if (PanelRoot)
    {
        const float TargetAlpha = bOpen ? 1.f : 0.f;
        if (!FMath::IsNearlyEqual(CurrentAlpha, TargetAlpha, 0.01f))
        {
            CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, InDeltaTime, 14.f);
            PanelRoot->SetRenderOpacity(CurrentAlpha);
            PanelRoot->SetVisibility(CurrentAlpha > 0.05f
                ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
}

// ---- settings 对象编辑区 ----

void USimSettingsWidget::AddEntrySection(UVerticalBox* Content)
{
    AddSectionHeader(Content, TEXT("settings 对象（保存后重建 SimMode 生效）"));

    // 可编辑对象条目下拉框（根设置/顶层段/载具/相机/传感器/数组元素）
    EntryCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("EntryCombo"));
    EntryCombo->SetWidgetStyle(MakeComboStyle());
    EntryCombo->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::OnEntrySelected);
    Content->AddChildToVerticalBox(EntryCombo)->SetPadding(FMargin(0, 2));

    // 参数键值行容器
    SensorRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SensorRowsBox"));
    Content->AddChildToVerticalBox(SensorRowsBox)->SetPadding(FMargin(0, 2));

    // 候选列表：根设置 + 顶层段/数组元素 + 各载具及其相机 + 全部传感器
    EntryRefs.Reset();
    if (Manager.IsValid())
    {
        Manager->GetEntryList(EntryRefs);
    }
    EntryCombo->ClearOptions();
    for (const FSimSettingsEntry& Ref : EntryRefs)
    {
        EntryCombo->AddOption(Ref.DisplayName);
    }
    if (EntryRefs.Num() > 0)
    {
        // SetSelectedOption 会触发 OnEntrySelected，进而构建参数行
        EntryCombo->SetSelectedOption(EntryRefs[0].DisplayName);
    }
}

void USimSettingsWidget::SelectEntryByName(const FString& Name)
{
    if (Name.IsEmpty() || !EntryCombo)
    {
        return;
    }
    for (const FSimSettingsEntry& Ref : EntryRefs)
    {
        if (Ref.DisplayName == Name)
        {
            // 下拉框选项与 EntryRefs 一一对应；SetSelectedOption 触发 OnEntrySelected 重建参数行
            EntryCombo->SetSelectedOption(Name);
            return;
        }
    }
}

void USimSettingsWidget::RebuildSensorRows(bool bFetchFromManager)
{
    // 清掉旧行（从 WidgetTree 移除，避免残留）
    for (UWidget* Row : SensorRowWidgets)
    {
        WidgetTree->RemoveWidget(Row);
    }
    SensorRowWidgets.Reset();
    SensorChecks.Reset();
    SensorTexts.Reset();
    SensorRowTypes.Reset();
    if (!SensorRowsBox)
    {
        return;
    }

    if (bFetchFromManager)
    {
        SensorParams.Reset();
        if (Manager.IsValid())
        {
            Manager->GetEntryParams(CurrentEntryRef, SensorParams);
        }
    }

    for (const FSimSensorParam& Param : SensorParams)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

        // 键名
        USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>();
        KeyBox->SetWidthOverride(170.f);
        KeyBox->SetContent(MakeLabel(Param.Key, 12, ParamLabelColor));
        Row->AddChildToHorizontalBox(KeyBox)->SetVerticalAlignment(VAlign_Center);

        int32 RowType = 3;
        if (Param.Type == 0)
        {
            // 布尔 → 复选框
            UCheckBox* Check = WidgetTree->ConstructWidget<UCheckBox>();
            Check->SetWidgetStyle(MakeCheckBoxStyle());
            Check->SetIsChecked(Param.RawValue == TEXT("true"));
            Row->AddChildToHorizontalBox(Check)->SetVerticalAlignment(VAlign_Center);
            SensorChecks.Add(Check);
            RowType = 0;
        }
        else if (Param.Type == 1 || Param.Type == 2)
        {
            // 数值/字符串 → 文本框
            UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>();
            Box->SetWidgetStyle(MakeTextBoxStyle());
            Box->SetText(FText::FromString(Param.RawValue));
            UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(Box);
            BoxSlot->SetVerticalAlignment(VAlign_Center);
            BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            SensorTexts.Add(Box);
            RowType = Param.Type;
        }
        else
        {
            // 复合值：只读展示
            UTextBlock* ReadOnly = MakeLabel(Param.RawValue, 11, NoteColor);
            ReadOnly->SetAutoWrapText(true);
            Row->AddChildToHorizontalBox(ReadOnly);
        }

        SensorRowTypes.Add(RowType);
        SensorRowsBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 2));
        SensorRowWidgets.Add(Row);
    }

    // 新增参数行
    UHorizontalBox* AddRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    NewParamKeyBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    NewParamKeyBox->SetWidgetStyle(MakeTextBoxStyle());
    NewParamKeyBox->SetHintText(FText::FromString(TEXT("新参数名")));
    USizeBox* NewKeyBox = WidgetTree->ConstructWidget<USizeBox>();
    NewKeyBox->SetWidthOverride(170.f);
    NewKeyBox->SetContent(NewParamKeyBox);
    AddRow->AddChildToHorizontalBox(NewKeyBox)->SetVerticalAlignment(VAlign_Center);

    NewParamValueBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    NewParamValueBox->SetWidgetStyle(MakeTextBoxStyle());
    NewParamValueBox->SetHintText(FText::FromString(TEXT("值（true/false/数字/文本）")));
    UHorizontalBoxSlot* NewValueSlot = AddRow->AddChildToHorizontalBox(NewParamValueBox);
    NewValueSlot->SetVerticalAlignment(VAlign_Center);
    NewValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    UButton* AddBtn = MakeButton(TEXT("添加"));
    AddBtn->OnClicked.AddDynamic(this, &USimSettingsWidget::OnEntryParamAdd);
    AddRow->AddChildToHorizontalBox(AddBtn)->SetVerticalAlignment(VAlign_Center);

    SensorRowsBox->AddChildToVerticalBox(AddRow)->SetPadding(FMargin(0, 4, 0, 2));
    SensorRowWidgets.Add(AddRow);
}

void USimSettingsWidget::OnEntrySelected(FString SelectedItem, ESelectInfo::Type /*SelectionType*/)
{
    for (const FSimSettingsEntry& Ref : EntryRefs)
    {
        if (Ref.DisplayName == SelectedItem)
        {
            CurrentEntryRef = Ref;
            RebuildSensorRows();
            return;
        }
    }
}

void USimSettingsWidget::OnEntryParamAdd()
{
    if (!NewParamKeyBox || !NewParamValueBox)
    {
        return;
    }
    const FString Key = NewParamKeyBox->GetText().ToString().TrimStartAndEnd();
    const FString Value = NewParamValueBox->GetText().ToString().TrimStartAndEnd();
    if (Key.IsEmpty() || Value.IsEmpty())
    {
        SetStatusText(TEXT("新增参数：请填写参数名和值"));
        return;
    }
    for (const FSimSensorParam& Existing : SensorParams)
    {
        if (Existing.Key == Key)
        {
            SetStatusText(TEXT("参数已存在，直接在列表中修改即可"));
            return;
        }
    }

    FSimSensorParam Param;
    Param.Key = Key;
    Param.RawValue = Value;
    Param.Type = Manager.IsValid() ? Manager->DetectValueType(Value) : 2;
    SensorParams.Add(Param);
    NewParamKeyBox->SetText(FText::GetEmpty());
    NewParamValueBox->SetText(FText::GetEmpty());
    // 保留内存中的 SensorParams（含新增项），不重新从 json 拉取
    RebuildSensorRows(false);
    SetStatusText(TEXT("参数已加入列表，点“保存并重建”生效"));
}

void USimSettingsWidget::OnCloseClicked()
{
    TogglePanel();
}

void USimSettingsWidget::OnSaveRebuild()
{
    if (!Manager.IsValid())
    {
        SetStatusText(TEXT("配置管理器未就绪"));
        return;
    }
    if (CurrentEntryRef.DisplayName.IsEmpty())
    {
        SetStatusText(TEXT("请先选择要编辑的 settings 条目"));
        return;
    }

    // 从行控件收集最新值（行类型与 SensorParams/SensorChecks/SensorTexts 一一对应）
    TArray<FSimSensorParam> Edited;
    int32 CheckIdx = 0;
    int32 TextIdx = 0;
    for (int32 i = 0; i < SensorParams.Num(); ++i)
    {
        FSimSensorParam Param = SensorParams[i];
        const int32 RowType = SensorRowTypes.IsValidIndex(i) ? SensorRowTypes[i] : Param.Type;
        switch (RowType)
        {
        case 0:
            if (SensorChecks.IsValidIndex(CheckIdx))
            {
                Param.RawValue = SensorChecks[CheckIdx]->IsChecked() ? TEXT("true") : TEXT("false");
            }
            ++CheckIdx;
            break;
        case 1:
        case 2:
            if (SensorTexts.IsValidIndex(TextIdx))
            {
                Param.RawValue = SensorTexts[TextIdx]->GetText().ToString();
            }
            ++TextIdx;
            break;
        default:
            break; // 只读项原样保留（Manager 侧会跳过）
        }
        Edited.Add(Param);
    }

    const bool bOk = Manager->ApplyEntryParams(CurrentEntryRef, Edited);
    SetStatusText(bOk
        ? FString::Printf(TEXT("条目 %s 已保存，SimMode 重建中…"), *CurrentEntryRef.DisplayName)
        : TEXT("保存失败，详见日志"));
}

// ---- 控件工厂 ----

UTextBlock* USimSettingsWidget::MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color, bool bBold) const
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Label->SetText(FText::FromString(Text));
    Label->SetColorAndOpacity(FSlateColor(Color));
    Label->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FontSize));
    return Label;
}

UButton* USimSettingsWidget::MakeButton(const FString& Text, bool bPrimary)
{
    UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    Btn->SetStyle(MakeButtonStyle(bPrimary));
    UTextBlock* Label = MakeLabel(Text, 13, FLinearColor::White, bPrimary);
    Btn->AddChild(Label);
    if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Btn->GetContent()->Slot))
    {
        ContentSlot->SetPadding(FMargin(0.f));
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
    }
    return Btn;
}

void USimSettingsWidget::AddSectionHeader(UVerticalBox* Parent, const FString& Title)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

    // 强调色竖条
    USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
    BarBox->SetWidthOverride(4.f);
    BarBox->SetHeightOverride(18.f);
    UBorder* Bar = WidgetTree->ConstructWidget<UBorder>();
    Bar->SetBrush(FSlateRoundedBoxBrush(Accent, 2.f));
    BarBox->SetContent(Bar);
    UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarBox);
    BarSlot->SetVerticalAlignment(VAlign_Center);
    BarSlot->SetPadding(FMargin(4, 0, 8, 0));

    UTextBlock* Label = MakeLabel(Title, 15, FLinearColor::White, true);
    UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
    LabelSlot->SetVerticalAlignment(VAlign_Center);

    Parent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 10, 0, 6));
}

void USimSettingsWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel)
    {
        StatusLabel->SetText(FText::FromString(Text));
    }
}
