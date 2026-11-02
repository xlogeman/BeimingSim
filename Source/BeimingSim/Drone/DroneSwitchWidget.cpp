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
    const FLinearColor TooltipBg(0.05f, 0.06f, 0.11f, 0.97f);
    const FLinearColor ParamLabelColor(0.62f, 0.68f, 0.76f, 1.f);
    constexpr float PanelRadius = 12.f;
    constexpr float EntryRadius = 8.f;
    constexpr float TooltipRadius = 10.f;
    constexpr int32 MaxEntries = 8;
    const FVector2D ThumbSize(80.f, 80.f);
    const FVector2D TooltipImageSize(220.f, 220.f);
    // 介绍文字强制换行宽度
    constexpr float TooltipTextWidth = 240.f;
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
    PanelBorder = Panel;

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
    EntryTextures.Reset();
    EntryInfos.Reset();
    EntryInfos.SetNum(MaxEntries);

    for (int32 i = 0; i < ModelNames.Num() && i < MaxEntries; ++i)
    {
        UButton* Btn = MakeEntryButton(i);
        UVerticalBoxSlot* BtnSlot = Box->AddChildToVerticalBox(Btn);
        BtnSlot->SetPadding(FMargin(0, 4));
        BtnSlot->SetHorizontalAlignment(HAlign_Fill);
    }

    // 悬停预览弹窗（默认隐藏，放在画布顶层）
    MakeTooltipPanel();

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

    // 行尾"设置"按钮：打开该机型的仿真配置面板（嵌套按钮，点击不会触发外层条目选中）
    UButton* SettingsBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
        *FString::Printf(TEXT("Settings%d"), Index));
    FButtonStyle SettingsStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
    SettingsStyle.Normal = FSlateRoundedBoxBrush(EntryBg, EntryRadius);
    SettingsStyle.Hovered = FSlateRoundedBoxBrush(EntryHoverBg, EntryRadius);
    SettingsStyle.Pressed = FSlateRoundedBoxBrush(EntryPressBg, EntryRadius);
    SettingsStyle.SetNormalPadding(FMargin(10.f, 4.f));
    SettingsStyle.SetPressedPadding(FMargin(10.f, 4.f));
    SettingsBtn->SetStyle(SettingsStyle);
    UTextBlock* SettingsLabel = MakeLabel(TEXT("设置"), 12, ParamLabelColor);
    SettingsBtn->AddChild(SettingsLabel);
    switch (Index)
    {
    case 0: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings0); break;
    case 1: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings1); break;
    case 2: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings2); break;
    case 3: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings3); break;
    case 4: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings4); break;
    case 5: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings5); break;
    case 6: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings6); break;
    case 7: SettingsBtn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnSettings7); break;
    default: break;
    }
    Row->AddChildToHorizontalBox(SettingsBtn)->SetVerticalAlignment(VAlign_Center);

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
    case 0: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick0);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover0); break;
    case 1: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick1);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover1); break;
    case 2: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick2);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover2); break;
    case 3: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick3);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover3); break;
    case 4: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick4);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover4); break;
    case 5: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick5);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover5); break;
    case 6: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick6);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover6); break;
    case 7: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick7);
            Btn->OnHovered.AddDynamic(this, &UDroneSwitchWidget::OnHover7); break;
    default: break;
    }
    // 取消悬停统一隐藏弹窗，无需区分条目
    Btn->OnUnhovered.AddDynamic(this, &UDroneSwitchWidget::OnEntryUnhover);

    EntryButtons.Add(Btn);
    EntryImages.Add(Img);
    EntryLabels.Add(Label);
    EntryTextures.Add(nullptr);
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
    if (EntryTextures.IsValidIndex(Index))
    {
        EntryTextures[Index] = Thumbnail;
    }
}

void UDroneSwitchWidget::SetEntryInfo(int32 Index, const FDroneModelInfo& Info)
{
    if (EntryInfos.IsValidIndex(Index))
    {
        EntryInfos[Index] = Info;
    }
}

void UDroneSwitchWidget::MakeTooltipPanel()
{
    TooltipBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TooltipBorder"));
    TooltipBorder->SetBrush(FSlateRoundedBoxBrush(TooltipBg, TooltipRadius, Accent, 1.f));
    TooltipBorder->SetPadding(FMargin(12.f));

    UVerticalBox* TipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TipBox"));
    TooltipBorder->SetContent(TipBox);

    // 大图预览
    UBorder* TipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TipFrame"));
    TipFrame->SetBrush(FSlateRoundedBoxBrush(ThumbBg, 6.f));
    TipFrame->SetPadding(FMargin(4.f));
    TipFrame->SetHorizontalAlignment(HAlign_Fill);
    TooltipImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TipImage"));
    FSlateBrush ImgBrush;
    ImgBrush.DrawAs = ESlateBrushDrawType::Image;
    ImgBrush.ImageSize = TooltipImageSize;
    TooltipImage->SetBrush(ImgBrush);
    TipFrame->SetContent(TooltipImage);

    USizeBox* ImageBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TipImageBox"));
    ImageBox->SetWidthOverride(TooltipImageSize.X + 8.f);
    ImageBox->SetHeightOverride(TooltipImageSize.Y + 8.f);
    ImageBox->SetContent(TipFrame);
    TipBox->AddChildToVerticalBox(ImageBox);

    // 机型名
    TooltipNameLabel = MakeLabel(TEXT(""), 18, Accent);
    TipBox->AddChildToVerticalBox(TooltipNameLabel)->SetPadding(FMargin(0, 10, 0, 4));

    // 介绍文字（固定宽度强制换行）
    TooltipDescLabel = MakeLabel(TEXT(""), 13, FLinearColor::White);
    TooltipDescLabel->SetAutoWrapText(true);
    USizeBox* DescBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TipDescBox"));
    DescBox->SetWidthOverride(TooltipTextWidth);
    DescBox->SetContent(TooltipDescLabel);
    TipBox->AddChildToVerticalBox(DescBox);

    // 参数列表（每行"名称：值"，显示时按内容重建）
    TooltipParamBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TipParams"));
    TipBox->AddChildToVerticalBox(TooltipParamBox)->SetPadding(FMargin(0, 8, 0, 0));

    TooltipSlot = RootCanvas->AddChildToCanvas(TooltipBorder);
    TooltipSlot->SetAutoSize(true);
    TooltipSlot->SetZOrder(100);
    // 弹窗右缘对齐 Position.X、垂直居中对齐 Position.Y（固定在菜单面板左侧）
    TooltipSlot->SetAnchors(FAnchors(0.f, 0.f));
    TooltipSlot->SetAlignment(FVector2D(1.f, 0.5f));
    TooltipSlot->SetPosition(FVector2D::ZeroVector);
    HideTooltip();
}

void UDroneSwitchWidget::ShowTooltip(int32 Index)
{
    if (!TooltipBorder || !ModelNames.IsValidIndex(Index) || !EntryInfos.IsValidIndex(Index))
    {
        return;
    }

    // 大图：复用条目缩略图
    if (TooltipImage)
    {
        FSlateBrush ImgBrush;
        ImgBrush.DrawAs = ESlateBrushDrawType::Image;
        ImgBrush.ImageSize = TooltipImageSize;
        if (EntryTextures.IsValidIndex(Index) && EntryTextures[Index])
        {
            ImgBrush.SetResourceObject(EntryTextures[Index]);
        }
        TooltipImage->SetBrush(ImgBrush);
    }
    if (TooltipNameLabel)
    {
        TooltipNameLabel->SetText(FText::FromString(ModelNames[Index]));
    }
    if (TooltipDescLabel)
    {
        const FText& Desc = EntryInfos[Index].Description;
        TooltipDescLabel->SetText(Desc.IsEmpty() ? FText::FromString(TEXT("暂无介绍（可在 DroneModelSwitcher 的 ModelInfos 中配置）")) : Desc);
    }
    if (TooltipParamBox)
    {
        TooltipParamBox->ClearChildren();
        for (const FDroneModelParam& Param : EntryInfos[Index].Params)
        {
            if (Param.Name.IsEmpty())
            {
                continue;
            }
            UTextBlock* Row = MakeLabel(Param.Name + TEXT("：") + Param.Value.ToString(), 12, ParamLabelColor);
            TooltipParamBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 2));
        }
        // 没有可展示的参数时隐藏参数区，避免残留空白
        TooltipParamBox->SetVisibility(TooltipParamBox->GetChildrenCount() > 0
            ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }

    TooltipBorder->SetVisibility(ESlateVisibility::HitTestInvisible);

    // 弹窗右缘紧贴菜单面板左缘（留 6px 缝隙），与悬停条目垂直居中对齐。
    // 注意：canvas 槽位坐标是本地逻辑坐标，必须用 AbsoluteToLocal 换算，
    // 直接用绝对坐标相减在 DPI 缩放非 100% 的显示器上位置会偏移
    const FGeometry& RootGeom = GetCachedGeometry();
    if (TooltipSlot && PanelBorder && EntryButtons.IsValidIndex(Index))
    {
        const FGeometry& PanelGeom = PanelBorder->GetCachedGeometry();
        const FGeometry& EntryGeom = EntryButtons[Index]->GetCachedGeometry();
        const FVector2D PanelPos = RootGeom.AbsoluteToLocal(PanelGeom.GetAbsolutePosition());
        const FVector2D EntryPos = RootGeom.AbsoluteToLocal(EntryGeom.GetAbsolutePosition());
        const float EntryCenterY = EntryPos.Y + EntryGeom.GetLocalSize().Y * 0.5f;
        TooltipSlot->SetPosition(FVector2D(PanelPos.X - 6.f, EntryCenterY));
    }
}

void UDroneSwitchWidget::HideTooltip()
{
    if (TooltipBorder)
    {
        TooltipBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
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
    // 广播开合状态（收起时外部联动关闭 settings.json 配置面板）
    OnMenuToggled.Broadcast(bOpen);
}

void UDroneSwitchWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (PanelSlot && !FMath::IsNearlyEqual(CurrentX, TargetX, 0.5f))
    {
        CurrentX = FMath::FInterpTo(CurrentX, TargetX, InDeltaTime, 10.f);
        PanelSlot->SetPosition(FVector2D(CurrentX, 80.f));
    }

    // 弹窗为固定位置；仅在内容尺寸就绪后做一次视口边界收拢，防止过高时上下越界
    if (TooltipBorder && TooltipSlot &&
        TooltipBorder->GetVisibility() != ESlateVisibility::Collapsed)
    {
        const FVector2D ViewportSize = MyGeometry.GetLocalSize();
        const FVector2D TipSize = TooltipBorder->GetDesiredSize();
        if (TipSize.Y > 0.f && TipSize.X > 0.f)
        {
            const FVector2D Pos = TooltipSlot->GetPosition();
            const float Half = TipSize.Y * 0.5f;
            const float ClampedY = FMath::Clamp(Pos.Y, Half + 8.f,
                FMath::Max(Half + 8.f, ViewportSize.Y - Half - 8.f));
            if (!FMath::IsNearlyEqual(ClampedY, Pos.Y))
            {
                TooltipSlot->SetPosition(FVector2D(Pos.X, ClampedY));
            }
        }
    }
}

void UDroneSwitchWidget::OnHover0() { ShowTooltip(0); }
void UDroneSwitchWidget::OnHover1() { ShowTooltip(1); }
void UDroneSwitchWidget::OnHover2() { ShowTooltip(2); }
void UDroneSwitchWidget::OnHover3() { ShowTooltip(3); }
void UDroneSwitchWidget::OnHover4() { ShowTooltip(4); }
void UDroneSwitchWidget::OnHover5() { ShowTooltip(5); }
void UDroneSwitchWidget::OnHover6() { ShowTooltip(6); }
void UDroneSwitchWidget::OnHover7() { ShowTooltip(7); }

void UDroneSwitchWidget::OnEntryUnhover()
{
    HideTooltip();
}

void UDroneSwitchWidget::RequestSettings(int32 Index)
{
    OnSettingsRequested.Broadcast(Index);
}

void UDroneSwitchWidget::OnSettings0() { RequestSettings(0); }
void UDroneSwitchWidget::OnSettings1() { RequestSettings(1); }
void UDroneSwitchWidget::OnSettings2() { RequestSettings(2); }
void UDroneSwitchWidget::OnSettings3() { RequestSettings(3); }
void UDroneSwitchWidget::OnSettings4() { RequestSettings(4); }
void UDroneSwitchWidget::OnSettings5() { RequestSettings(5); }
void UDroneSwitchWidget::OnSettings6() { RequestSettings(6); }
void UDroneSwitchWidget::OnSettings7() { RequestSettings(7); }

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
