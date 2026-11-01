#include "DroneSwitchWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"

namespace
{
    // 配色
    const FLinearColor PanelBg(0.04f, 0.05f, 0.09f, 0.94f);
    const FLinearColor EntryBg(0.10f, 0.12f, 0.18f, 0.75f);
    const FLinearColor EntryHoverBg(0.16f, 0.21f, 0.31f, 0.90f);
    const FLinearColor EntryPressBg(0.21f, 0.28f, 0.41f, 0.95f);
    // 与快照 RT 的 ClearColor 完全一致且不透明，避免方块内出现深浅不一的边界
    const FLinearColor ThumbBg(0.02f, 0.03f, 0.06f, 1.f);
    const FLinearColor Accent(0.15f, 0.62f, 0.95f, 1.f);
    constexpr float PanelRadius = 12.f;
    constexpr float EntryRadius = 8.f;
    constexpr int32 MaxEntries = 8;
    const FVector2D ThumbSize(80.f, 80.f);
}

void UDroneSwitchWidget::BuildMenu(const TArray<FString>& InModelNames)
{
    ModelNames = InModelNames;

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    WidgetTree->RootWidget = RootCanvas;

    // 圆角深色半透明面板，带一圈细强调色描边
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrush(FSlateRoundedBoxBrush(PanelBg, PanelRadius, Accent, 1.f));
    Panel->SetPadding(FMargin(16.f, 14.f));
    Panel->SetHorizontalAlignment(HAlign_Fill);

    PanelSlot = RootCanvas->AddChildToCanvas(Panel);
    // 锚定屏幕右上角，以面板右上点为轴定位（滑出方向为屏幕右侧）
    PanelSlot->SetAnchors(FAnchors(1.f, 0.1f));
    PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
    PanelSlot->SetAutoSize(true);
    PanelSlot->SetPosition(FVector2D(CurrentX, 80.f));
    PanelSlot->SetZOrder(50);

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
    Panel->SetContent(Box);

    // 标题
    Box->AddChildToVerticalBox(MakeLabel(TEXT("机型选择"), 20, FLinearColor::White))
        ->SetPadding(FMargin(4, 0, 0, 6));

    // 强调色下划线
    UBorder* AccentBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AccentBar"));
    AccentBar->SetBrush(FSlateRoundedBoxBrush(Accent, 2.f));
    USpacer* BarFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("BarFill"));
    BarFill->SetSize(FVector2D(64.f, 3.f));
    AccentBar->SetContent(BarFill);
    Box->AddChildToVerticalBox(AccentBar)->SetPadding(FMargin(4, 0, 0, 12));

    // 机型条目（名称 + 预览图）
    EntryButtons.Reset();
    EntryImages.Reset();
    EntryLabels.Reset();

    for (int32 i = 0; i < ModelNames.Num() && i < MaxEntries; ++i)
    {
        UButton* Btn = MakeEntryButton(i);
        UVerticalBoxSlot* BtnSlot = Box->AddChildToVerticalBox(Btn);
        BtnSlot->SetPadding(FMargin(0, 4));
        BtnSlot->SetHorizontalAlignment(HAlign_Fill);
    }

    // 底部提示
    Box->AddChildToVerticalBox(MakeLabel(TEXT("Tab 展开 / 收起"), 12, FLinearColor::Gray))
        ->SetPadding(FMargin(4, 10, 0, 0));
}

UButton* UDroneSwitchWidget::MakeEntryButton(int32 Index)
{
    UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
        *FString::Printf(TEXT("Pick%d"), Index));

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
        *FString::Printf(TEXT("Row%d"), Index));

    // 预览图（固定尺寸方块，固定放行首，所有条目天然左对齐）
    UBorder* ThumbFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
        *FString::Printf(TEXT("ThumbFrame%d"), Index));
    ThumbFrame->SetBrush(FSlateRoundedBoxBrush(ThumbBg, 4.f));
    ThumbFrame->SetPadding(FMargin(3.f));
    ThumbFrame->SetHorizontalAlignment(HAlign_Center);
    ThumbFrame->SetVerticalAlignment(VAlign_Center);

    UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
        *FString::Printf(TEXT("Thumb%d"), Index));
    FSlateBrush ImgBrush;
    ImgBrush.DrawAs = ESlateBrushDrawType::Image;
    ImgBrush.ImageSize = ThumbSize;
    Img->SetBrush(ImgBrush);
    ThumbFrame->SetContent(Img);

    USizeBox* ThumbBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
        *FString::Printf(TEXT("ThumbBox%d"), Index));
    ThumbBox->SetWidthOverride(ThumbSize.X + 6.f);
    ThumbBox->SetHeightOverride(ThumbSize.Y + 6.f);
    ThumbBox->SetContent(ThumbFrame);

    UHorizontalBoxSlot* ImgSlot = Row->AddChildToHorizontalBox(ThumbBox);
    ImgSlot->SetPadding(FMargin(0, 0, 14, 0));
    ImgSlot->SetVerticalAlignment(VAlign_Center);

    // 机型名
    UTextBlock* Label = MakeLabel(Index < ModelNames.Num() ? ModelNames[Index] : TEXT(""), 16, FLinearColor::White);
    UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
    LabelSlot->SetVerticalAlignment(VAlign_Center);
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    Btn->AddChild(Row);
    // 显式固定按钮内容槽位：内边距为 0、填满按钮，消除默认值带来的行间差异
    if (UButtonSlot* BtnContentSlot = Cast<UButtonSlot>(Row->Slot))
    {
        BtnContentSlot->SetPadding(FMargin(0.f));
        BtnContentSlot->SetHorizontalAlignment(HAlign_Fill);
        BtnContentSlot->SetVerticalAlignment(VAlign_Fill);
    }

    switch (Index)
    {
    case 0: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick0); break;
    case 1: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick1); break;
    case 2: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick2); break;
    case 3: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick3); break;
    case 4: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick4); break;
    case 5: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick5); break;
    case 6: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick6); break;
    case 7: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick7); break;
    default: break;
    }

    EntryButtons.Add(Btn);
    EntryImages.Add(Img);
    EntryLabels.Add(Label);
    ApplyEntryStyle(Index);
    return Btn;
}

void UDroneSwitchWidget::ApplyEntryStyle(int32 Index)
{
    if (!EntryButtons.IsValidIndex(Index))
    {
        return;
    }
    const bool bSelected = (Index == SelectedIndex);
    // 描边宽度所有条目保持一致（仅颜色区分选中态），
    // 否则选中行会因描边向外多画一圈，导致整体比其他行大、向左凸出
    const float OutlineWidth = 1.5f;
    const FLinearColor Outline = bSelected ? Accent : FLinearColor(0.f, 0.f, 0.f, 0.f);

    FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
    Style.Normal = FSlateRoundedBoxBrush(EntryBg, EntryRadius, Outline, OutlineWidth);
    Style.Hovered = FSlateRoundedBoxBrush(EntryHoverBg, EntryRadius, Outline, OutlineWidth);
    Style.Pressed = FSlateRoundedBoxBrush(EntryPressBg, EntryRadius, bSelected ? Accent : Outline, OutlineWidth);
    // 选中机型整体禁用（不可再次点击），Disabled 态保持选中高亮观感
    Style.Disabled = FSlateRoundedBoxBrush(EntryBg, EntryRadius, Accent, OutlineWidth);
    Style.SetNormalPadding(FMargin(12.f, 8.f));
    Style.SetPressedPadding(FMargin(12.f, 8.f));
    EntryButtons[Index]->SetStyle(Style);
    EntryButtons[Index]->SetIsEnabled(!bSelected);

    if (EntryLabels.IsValidIndex(Index))
    {
        EntryLabels[Index]->SetColorAndOpacity(FSlateColor(bSelected ? Accent : FLinearColor::White));
    }
}

void UDroneSwitchWidget::SetEntryThumbnail(int32 Index, UTexture* Thumbnail)
{
    if (!EntryImages.IsValidIndex(Index) || !Thumbnail)
    {
        return;
    }
    FSlateBrush ImgBrush = EntryImages[Index]->GetBrush();
    ImgBrush.SetResourceObject(Thumbnail);
    ImgBrush.ImageSize = ThumbSize;
    EntryImages[Index]->SetBrush(ImgBrush);
}

void UDroneSwitchWidget::HighlightSelected(int32 Index)
{
    SelectedIndex = Index;
    for (int32 i = 0; i < EntryButtons.Num(); ++i)
    {
        ApplyEntryStyle(i);
    }
}

void UDroneSwitchWidget::ToggleMenu()
{
    bOpen = !bOpen;
    // 右侧面板：展开时贴右缘留 16px 边距，收起时滑出屏幕右侧
    TargetX = bOpen ? -16.f : PanelWidth + 60.f;
}

void UDroneSwitchWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (PanelSlot && !FMath::IsNearlyEqual(CurrentX, TargetX, 0.5f))
    {
        CurrentX = FMath::FInterpTo(CurrentX, TargetX, InDeltaTime, 10.f);
        PanelSlot->SetPosition(FVector2D(CurrentX, 80.f));
    }
}

UTextBlock* UDroneSwitchWidget::MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color) const
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Label->SetText(FText::FromString(Text));
    Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
    Label->SetColorAndOpacity(FSlateColor(Color));
    return Label;
}

void UDroneSwitchWidget::RequestPick(int32 Index)
{
    // 无效下标或重复选择当前机型：直接忽略，不广播、不收起面板
    if (!ModelNames.IsValidIndex(Index) || Index == SelectedIndex)
    {
        return;
    }
    OnModelPicked.Broadcast(Index);
    ToggleMenu();
}

void UDroneSwitchWidget::OnPick0() { RequestPick(0); }
void UDroneSwitchWidget::OnPick1() { RequestPick(1); }
void UDroneSwitchWidget::OnPick2() { RequestPick(2); }
void UDroneSwitchWidget::OnPick3() { RequestPick(3); }
void UDroneSwitchWidget::OnPick4() { RequestPick(4); }
void UDroneSwitchWidget::OnPick5() { RequestPick(5); }
void UDroneSwitchWidget::OnPick6() { RequestPick(6); }
void UDroneSwitchWidget::OnPick7() { RequestPick(7); }
