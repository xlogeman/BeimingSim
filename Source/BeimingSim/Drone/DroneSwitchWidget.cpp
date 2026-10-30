#include "DroneSwitchWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"

void UDroneSwitchWidget::BuildMenu(const TArray<FString>& InModelNames)
{
    ModelNames = InModelNames;

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    WidgetTree->RootWidget = RootCanvas;

    // 半透明深色面板
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    FSlateColorBrush PanelBrush(FLinearColor(0.02f, 0.03f, 0.06f, 0.88f));
    Panel->SetBrush(PanelBrush);
    Panel->SetPadding(FMargin(16.f, 14.f));
    Panel->SetHorizontalAlignment(HAlign_Fill);

    PanelSlot = RootCanvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.f, 0.1f));
    PanelSlot->SetAutoSize(true);
    PanelSlot->SetPosition(FVector2D(CurrentX, 80.f));
    PanelSlot->SetZOrder(50);

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
    Panel->SetContent(Box);

    // 标题
    Box->AddChildToVerticalBox(MakeLabel(TEXT("机型选择"), 20, FLinearColor::White))
        ->SetPadding(FMargin(0, 0, 0, 10));

    // 机型按钮（最多 4 个，与 OnPick0~3 对应）
    for (int32 i = 0; i < ModelNames.Num() && i < 4; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
            *FString::Printf(TEXT("Pick%d"), i));
        Btn->AddChild(MakeLabel(ModelNames[i], 16, FLinearColor::White));

        UVerticalBoxSlot* BtnSlot = Box->AddChildToVerticalBox(Btn);
        BtnSlot->SetPadding(FMargin(0, 4));
        BtnSlot->SetHorizontalAlignment(HAlign_Fill);

        switch (i)
        {
        case 0: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick0); break;
        case 1: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick1); break;
        case 2: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick2); break;
        case 3: Btn->OnClicked.AddDynamic(this, &UDroneSwitchWidget::OnPick3); break;
        default: break;
        }
    }

    // 底部提示
    Box->AddChildToVerticalBox(MakeLabel(TEXT("Tab 展开 / 收起"), 12, FLinearColor::Gray))
        ->SetPadding(FMargin(0, 10, 0, 0));
}

void UDroneSwitchWidget::ToggleMenu()
{
    bOpen = !bOpen;
    TargetX = bOpen ? 0.f : -PanelWidth - 40.f;
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

void UDroneSwitchWidget::OnPick0() { if (ModelNames.Num() > 0) { OnModelPicked.Broadcast(0); ToggleMenu(); } }
void UDroneSwitchWidget::OnPick1() { if (ModelNames.Num() > 1) { OnModelPicked.Broadcast(1); ToggleMenu(); } }
void UDroneSwitchWidget::OnPick2() { if (ModelNames.Num() > 2) { OnModelPicked.Broadcast(2); ToggleMenu(); } }
void UDroneSwitchWidget::OnPick3() { if (ModelNames.Num() > 3) { OnModelPicked.Broadcast(3); ToggleMenu(); } }
