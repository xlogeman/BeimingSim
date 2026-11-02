#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Blueprint/WidgetTree.h"

/**
 * 三个纯 C++ 面板（DroneSwitchWidget / SimConfigWidget / SimSettingsWidget）
 * 共享的配色与控件样式工厂，保证各面板视觉一致。
 * 使用处 #include 后直接以 using namespace SimPanelStyle 引入。
 */
namespace SimPanelStyle
{
    // ---- 配色 ----
    inline const FLinearColor PanelBg(0.04f, 0.05f, 0.09f, 0.94f);
    inline const FLinearColor Accent(0.15f, 0.62f, 0.95f, 1.f);
    inline const FLinearColor AccentHover(0.24f, 0.72f, 1.f, 1.f);
    inline const FLinearColor AccentPress(0.10f, 0.48f, 0.78f, 1.f);
    inline const FLinearColor ParamLabelColor(0.62f, 0.68f, 0.76f, 1.f);
    inline const FLinearColor NoteColor(0.55f, 0.60f, 0.68f, 1.f);
    inline const FLinearColor TrackBg(0.24f, 0.27f, 0.34f, 1.f);
    inline const FLinearColor BtnBg(0.13f, 0.16f, 0.24f, 1.f);
    inline const FLinearColor BtnHoverBg(0.19f, 0.24f, 0.35f, 1.f);
    inline const FLinearColor BtnPressBg(0.10f, 0.12f, 0.19f, 1.f);

    inline constexpr float PanelRadius = 12.f;

    /** 圆角矩形刷子（可指定尺寸，用于复选框/滑条拇指等需要固定大小的场合） */
    inline FSlateRoundedBoxBrush MakeRoundBrush(const FLinearColor& Color, float Radius,
        FVector2D Size = FVector2D::ZeroVector)
    {
        FSlateRoundedBoxBrush Brush(Color, Radius);
        if (Size != FVector2D::ZeroVector)
        {
            Brush.ImageSize = Size;
        }
        return Brush;
    }

    /** 带描边的圆角矩形刷子 */
    inline FSlateRoundedBoxBrush MakeRoundBrush(const FLinearColor& Color, float Radius,
        const FLinearColor& OutlineColor, float OutlineWidth, FVector2D Size = FVector2D::ZeroVector)
    {
        FSlateRoundedBoxBrush Brush(Color, Radius, OutlineColor, OutlineWidth);
        if (Size != FVector2D::ZeroVector)
        {
            Brush.ImageSize = Size;
        }
        return Brush;
    }

    inline FButtonStyle MakeButtonStyle(bool bPrimary)
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

    inline FCheckBoxStyle MakeCheckBoxStyle()
    {
        FCheckBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CheckBox");
        const FVector2D Size(18.f, 18.f);
        Style.SetUncheckedImage(MakeRoundBrush(TrackBg, 4.f, Size));
        Style.SetUncheckedHoveredImage(MakeRoundBrush(FLinearColor(0.30f, 0.34f, 0.42f, 1.f), 4.f, Size));
        Style.SetUncheckedPressedImage(MakeRoundBrush(TrackBg, 4.f, Size));
        Style.SetCheckedImage(MakeRoundBrush(Accent, 4.f, Size));
        Style.SetCheckedHoveredImage(MakeRoundBrush(AccentHover, 4.f, Size));
        Style.SetCheckedPressedImage(MakeRoundBrush(AccentPress, 4.f, Size));
        return Style;
    }

    inline FSliderStyle MakeSliderStyle()
    {
        // 基于引擎默认样式微调：细轨道 + 圆形强调色拇指
        FSliderStyle Style = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
        Style.SetNormalBarImage(MakeRoundBrush(TrackBg, 3.f));
        Style.SetHoveredBarImage(MakeRoundBrush(TrackBg, 3.f));
        Style.SetNormalThumbImage(MakeRoundBrush(Accent, 8.f, FVector2D(14.f, 14.f)));
        Style.SetHoveredThumbImage(MakeRoundBrush(AccentHover, 8.f, FVector2D(14.f, 14.f)));
        Style.SetBarThickness(5.f);
        return Style;
    }

    inline UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Text, int32 FontSize,
        FLinearColor Color, bool bBold = false)
    {
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FontSize));
        return Label;
    }

    inline UButton* MakeButton(UWidgetTree* Tree, const FString& Text, bool bPrimary = false)
    {
        UButton* Btn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
        Btn->SetStyle(MakeButtonStyle(bPrimary));
        UTextBlock* Label = MakeLabel(Tree, Text, 13, FLinearColor::White, bPrimary);
        Btn->AddChild(Label);
        if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Btn->GetContent()->Slot))
        {
            ContentSlot->SetPadding(FMargin(0.f));
            ContentSlot->SetHorizontalAlignment(HAlign_Fill);
            ContentSlot->SetVerticalAlignment(VAlign_Fill);
        }
        return Btn;
    }

    /** 分区标题行：强调色竖条 + 标题 */
    inline void AddSectionHeader(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Title)
    {
        UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();

        USizeBox* BarBox = Tree->ConstructWidget<USizeBox>();
        BarBox->SetWidthOverride(4.f);
        BarBox->SetHeightOverride(18.f);
        UBorder* Bar = Tree->ConstructWidget<UBorder>();
        Bar->SetBrush(FSlateRoundedBoxBrush(Accent, 2.f));
        BarBox->SetContent(Bar);
        UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarBox);
        BarSlot->SetVerticalAlignment(VAlign_Center);
        BarSlot->SetPadding(FMargin(4, 0, 8, 0));

        UTextBlock* Label = MakeLabel(Tree, Title, 15, FLinearColor::White, true);
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetVerticalAlignment(VAlign_Center);

        Parent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 10, 0, 6));
    }
}
