#include "SimConfigWidget.h"
#include "SimConfigManager.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"

// AirSim 运行时 API：天气/暂停回读
#include "SimMode/SimModeBase.h"
#include "Weather/WeatherLib.h"

namespace
{
    // 与 DroneSwitchWidget 保持一致的配色
    const FLinearColor PanelBg(0.04f, 0.05f, 0.09f, 0.94f);
    const FLinearColor Accent(0.15f, 0.62f, 0.95f, 1.f);
    const FLinearColor AccentHover(0.24f, 0.72f, 1.f, 1.f);
    const FLinearColor AccentPress(0.10f, 0.48f, 0.78f, 1.f);
    const FLinearColor ParamLabelColor(0.62f, 0.68f, 0.76f, 1.f);
    const FLinearColor NoteColor(0.55f, 0.60f, 0.68f, 1.f);
    // 控件配色
    const FLinearColor TrackBg(0.24f, 0.27f, 0.34f, 1.f);
    const FLinearColor BtnBg(0.13f, 0.16f, 0.24f, 1.f);
    const FLinearColor BtnHoverBg(0.19f, 0.24f, 0.35f, 1.f);
    const FLinearColor BtnPressBg(0.10f, 0.12f, 0.19f, 1.f);
    constexpr float PanelRadius = 12.f;
    constexpr float NameWidth = 110.f;
    constexpr float ValueWidth = 52.f;
    constexpr float ScrollHeight = 600.f;

    /** 圆角矩形刷子（可指定尺寸，用于复选框/滑条拇指等需要固定大小的场合） */
    FSlateRoundedBoxBrush MakeRoundBrush(const FLinearColor& Color, float Radius, FVector2D Size = FVector2D::ZeroVector)
    {
        FSlateRoundedBoxBrush Brush(Color, Radius);
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

    FSliderStyle MakeSliderStyle()
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

    FCheckBoxStyle MakeCheckBoxStyle()
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
}

void USimConfigWidget::BuildPanel(ASimConfigManager* InManager)
{
    Manager = InManager;

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    WidgetTree->RootWidget = RootCanvas;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrush(FSlateRoundedBoxBrush(PanelBg, PanelRadius, Accent, 1.f));
    Panel->SetPadding(FMargin(14.f, 12.f));
    Panel->SetHorizontalAlignment(HAlign_Fill);

    // 锚定屏幕右上角，以面板右上点为轴定位（滑出方向为屏幕右侧）
    PanelSlot = RootCanvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(1.f, 0.05f));
    PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
    PanelSlot->SetAutoSize(true);
    PanelSlot->SetPosition(FVector2D(CurrentX, 60.f));
    PanelSlot->SetZOrder(60);

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
    Panel->SetContent(Box);

    // 标题 + 强调色下划线
    Box->AddChildToVerticalBox(MakeLabel(TEXT("仿真环境"), 20, FLinearColor::White, true))
        ->SetPadding(FMargin(4, 0, 0, 6));
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

    Sliders.Reset();
    ValueLabels.Reset();
    Sliders.SetNum(SliderCount);
    ValueLabels.SetNum(SliderCount);

    // ---- 环境天气 ----
    AddSectionHeader(Content, TEXT("环境天气"));

    UHorizontalBox* WeatherRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WeatherRow"));
    UCheckBox* WeatherEnabled = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("WeatherEnabled"));
    WeatherEnabled->SetWidgetStyle(MakeCheckBoxStyle());
    WeatherEnabled->OnCheckStateChanged.AddDynamic(this, &USimConfigWidget::OnWeatherEnabledChanged);
    UHorizontalBoxSlot* WeatherCheckSlot = WeatherRow->AddChildToHorizontalBox(WeatherEnabled);
    WeatherCheckSlot->SetPadding(FMargin(4, 0, 8, 0));
    WeatherCheckSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* WeatherLabelSlot = WeatherRow->AddChildToHorizontalBox(
        MakeLabel(TEXT("启用天气特效"), 14, FLinearColor::White));
    WeatherLabelSlot->SetVerticalAlignment(VAlign_Center);
    Content->AddChildToVerticalBox(WeatherRow)->SetPadding(FMargin(0, 4));
    WeatherCheck = WeatherEnabled;

    MakeSliderRow(Content, TEXT("雨量"), Rain, 0.f, 1.f, 0.f);
    MakeSliderRow(Content, TEXT("积雪"), Snow, 0.f, 1.f, 0.f);
    MakeSliderRow(Content, TEXT("沙尘"), Dust, 0.f, 1.f, 0.f);
    MakeSliderRow(Content, TEXT("雾气"), Fog, 0.f, 1.f, 0.f);

    // ---- 风 ----
    AddSectionHeader(Content, TEXT("风（NED，m/s）"));
    MakeSliderRow(Content, TEXT("北向 X"), WindX, -10.f, 10.f, 0.f);
    MakeSliderRow(Content, TEXT("东向 Y"), WindY, -10.f, 10.f, 0.f);
    MakeSliderRow(Content, TEXT("垂向 Z"), WindZ, -10.f, 10.f, 0.f);

    // ---- 相机 ----
    AddSectionHeader(Content, TEXT("相机"));
    MakeSliderRow(Content, TEXT("默认相机 FOV"), CameraFov, 30.f, 170.f, 90.f);

    // ---- 时间模拟 ----
    AddSectionHeader(Content, TEXT("时间模拟"));
    UHorizontalBox* TimeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TimeRow"));
    // AddDynamic 宏要求成员函数表达式直接可见（内部依赖 __FUNCTION__），不能通过指针变量绑定，逐个显式绑定
    UButton* MorningBtn = MakeButton(TEXT("清晨"));
    MorningBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnTimeMorning);
    TimeRow->AddChildToHorizontalBox(MorningBtn)->SetPadding(FMargin(2, 0));
    UButton* NoonBtn = MakeButton(TEXT("正午"));
    NoonBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnTimeNoon);
    TimeRow->AddChildToHorizontalBox(NoonBtn)->SetPadding(FMargin(2, 0));
    UButton* EveningBtn = MakeButton(TEXT("黄昏"));
    EveningBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnTimeEvening);
    TimeRow->AddChildToHorizontalBox(EveningBtn)->SetPadding(FMargin(2, 0));
    UButton* NightBtn = MakeButton(TEXT("夜晚"));
    NightBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnTimeNight);
    TimeRow->AddChildToHorizontalBox(NightBtn)->SetPadding(FMargin(2, 0));
    UButton* DisableBtn = MakeButton(TEXT("关闭"));
    DisableBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnTimeDisable);
    TimeRow->AddChildToHorizontalBox(DisableBtn)->SetPadding(FMargin(2, 0));
    Content->AddChildToVerticalBox(TimeRow)->SetPadding(FMargin(0, 4));

    // ---- 仿真 ----
    AddSectionHeader(Content, TEXT("仿真"));

    UHorizontalBox* PauseRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PauseRow"));
    UCheckBox* PauseEnabled = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("PauseEnabled"));
    PauseEnabled->SetWidgetStyle(MakeCheckBoxStyle());
    PauseEnabled->OnCheckStateChanged.AddDynamic(this, &USimConfigWidget::OnPauseChanged);
    UHorizontalBoxSlot* PauseCheckSlot = PauseRow->AddChildToHorizontalBox(PauseEnabled);
    PauseCheckSlot->SetPadding(FMargin(4, 0, 8, 0));
    PauseCheckSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* PauseLabelSlot = PauseRow->AddChildToHorizontalBox(
        MakeLabel(TEXT("暂停仿真"), 14, FLinearColor::White));
    PauseLabelSlot->SetVerticalAlignment(VAlign_Center);
    Content->AddChildToVerticalBox(PauseRow)->SetPadding(FMargin(0, 4));
    PauseCheck = PauseEnabled;

    // ---- 状态与操作按钮 ----
    StatusLabel = MakeLabel(TEXT(""), 12, FLinearColor::Yellow);
    Content->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(4, 8, 0, 2));

    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BtnRow"));
    UButton* ApplyBtn = MakeButton(TEXT("应用（即时生效）"), true);
    ApplyBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnApplyLive);
    UHorizontalBoxSlot* ApplySlot = BtnRow->AddChildToHorizontalBox(ApplyBtn);
    ApplySlot->SetPadding(FMargin(2, 0));
    ApplySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Box->AddChildToVerticalBox(BtnRow)->SetPadding(FMargin(0, 6, 0, 0));

    Box->AddChildToVerticalBox(MakeLabel(TEXT("O 展开 / 收起"), 12, FLinearColor::Gray))
        ->SetPadding(FMargin(4, 8, 0, 0));

    // 统一绑定滑条回调（AddDynamic 宏要求成员函数表达式直接可见，逐个显式绑定）
    Sliders[Rain]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider0);
    Sliders[Snow]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider1);
    Sliders[Dust]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider2);
    Sliders[Fog]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider3);
    Sliders[WindX]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider4);
    Sliders[WindY]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider5);
    Sliders[WindZ]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider6);
    Sliders[CameraFov]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider7);

    RefreshFromWorld();
}

void USimConfigWidget::RefreshFromWorld()
{
    if (UWorld* World = GetWorld())
    {
        WeatherCheck->SetIsChecked(UWeatherLib::getIsWeatherEnabled(World));
        // 滑条下标 → 天气参数枚举（枚举顺序不同，需显式映射）
        const EWeatherParamScalar Params[] = {
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_SNOW,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_DUST,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG,
        };
        for (int32 i = Rain; i <= Fog; ++i)
        {
            Sliders[i]->SetValue(UWeatherLib::getWeatherParamScalar(World, Params[i - Rain]));
            UpdateValueLabel(static_cast<ESliderIndex>(i));
        }
    }
    if (ASimModeBase* Sim = ASimModeBase::getSimMode())
    {
        PauseCheck->SetIsChecked(Sim->isPaused());
    }
    // 风与 FOV 没有回读接口，保持用户上次输入
    UpdateValueLabel(WindX);
    UpdateValueLabel(WindY);
    UpdateValueLabel(WindZ);
    UpdateValueLabel(CameraFov);
    SetStatusText(TEXT(""));
}

void USimConfigWidget::TogglePanel()
{
    bOpen = !bOpen;
    TargetX = bOpen ? 16.f : PanelWidth + 60.f;
    if (bOpen)
    {
        RefreshFromWorld();
    }
}

void USimConfigWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (PanelSlot && !FMath::IsNearlyEqual(CurrentX, TargetX, 0.5f))
    {
        CurrentX = FMath::FInterpTo(CurrentX, TargetX, InDeltaTime, 12.f);
        PanelSlot->SetPosition(FVector2D(CurrentX, 60.f));
    }
}

// ---- 固定槽位分发 ----

void USimConfigWidget::OnSlider0(float Value) { OnSliderChanged(Rain, Value); }
void USimConfigWidget::OnSlider1(float Value) { OnSliderChanged(Snow, Value); }
void USimConfigWidget::OnSlider2(float Value) { OnSliderChanged(Dust, Value); }
void USimConfigWidget::OnSlider3(float Value) { OnSliderChanged(Fog, Value); }
void USimConfigWidget::OnSlider4(float Value) { OnSliderChanged(WindX, Value); }
void USimConfigWidget::OnSlider5(float Value) { OnSliderChanged(WindY, Value); }
void USimConfigWidget::OnSlider6(float Value) { OnSliderChanged(WindZ, Value); }
void USimConfigWidget::OnSlider7(float Value) { OnSliderChanged(CameraFov, Value); }

void USimConfigWidget::OnSliderChanged(ESliderIndex Index, float Value)
{
    UpdateValueLabel(Index);
    if (Index >= Rain && Index <= Fog && WeatherCheck && !WeatherCheck->IsChecked())
    {
        // 调天气滑条时自动打开天气特效，避免"拉了滑条没反应"的困惑
        WeatherCheck->SetIsChecked(true);
    }
}

void USimConfigWidget::OnWeatherEnabledChanged(bool bChecked)
{
    if (UWorld* World = GetWorld())
    {
        UWeatherLib::setWeatherEnabled(World, bChecked);
        SetStatusText(bChecked ? TEXT("天气特效已启用") : TEXT("天气特效已关闭"));
    }
}

void USimConfigWidget::OnPauseChanged(bool bChecked)
{
    if (ASimModeBase* Sim = ASimModeBase::getSimMode())
    {
        Sim->pause(bChecked);
        SetStatusText(bChecked ? TEXT("仿真已暂停") : TEXT("仿真已恢复"));
    }
}

void USimConfigWidget::OnApplyLive()
{
    if (!Manager.IsValid())
    {
        SetStatusText(TEXT("配置管理器未就绪"));
        return;
    }
    Manager->ApplyLive(
        WeatherCheck ? WeatherCheck->IsChecked() : false,
        Sliders[Rain]->GetValue(),
        Sliders[Snow]->GetValue(),
        Sliders[Dust]->GetValue(),
        Sliders[Fog]->GetValue(),
        FVector(Sliders[WindX]->GetValue(), Sliders[WindY]->GetValue(), Sliders[WindZ]->GetValue()),
        Sliders[CameraFov]->GetValue(),
        PauseCheck ? PauseCheck->IsChecked() : false);
    SetStatusText(TEXT("已应用：天气 / 风 / FOV / 暂停"));
}

void USimConfigWidget::OnTimeMorning() { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(0); SetStatusText(TEXT("时间已设为清晨 06:30")); } }
void USimConfigWidget::OnTimeNoon()    { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(1); SetStatusText(TEXT("时间已设为正午 12:00")); } }
void USimConfigWidget::OnTimeEvening() { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(2); SetStatusText(TEXT("时间已设为黄昏 18:30")); } }
void USimConfigWidget::OnTimeNight()   { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(3); SetStatusText(TEXT("时间已设为夜晚 22:00")); } }
void USimConfigWidget::OnTimeDisable() { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(-1); SetStatusText(TEXT("时间模拟已关闭")); } }

// ---- 控件工厂 ----

UTextBlock* USimConfigWidget::MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color, bool bBold) const
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Label->SetText(FText::FromString(Text));
    Label->SetColorAndOpacity(FSlateColor(Color));
    Label->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FontSize));
    return Label;
}

USlider* USimConfigWidget::MakeSliderRow(UVerticalBox* Parent, const FString& Name, ESliderIndex Index,
    float Min, float Max, float Value)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
        *FString::Printf(TEXT("SliderRow%d"), static_cast<int32>(Index)));

    // 参数名
    USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
        *FString::Printf(TEXT("NameBox%d"), static_cast<int32>(Index)));
    NameBox->SetWidthOverride(NameWidth);
    NameBox->SetContent(MakeLabel(Name, 13, ParamLabelColor));
    UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameBox);
    NameSlot->SetVerticalAlignment(VAlign_Center);

    // 滑条
    USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),
        *FString::Printf(TEXT("Slider%d"), static_cast<int32>(Index)));
    Slider->SetWidgetStyle(MakeSliderStyle());
    Slider->SetMinValue(Min);
    Slider->SetMaxValue(Max);
    Slider->SetStepSize((Max - Min) / 200.f);
    Slider->SetValue(Value);
    UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
    SliderSlot->SetVerticalAlignment(VAlign_Center);
    SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 数值
    USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
        *FString::Printf(TEXT("ValueBox%d"), static_cast<int32>(Index)));
    ValueBox->SetWidthOverride(ValueWidth);
    UTextBlock* ValueLabel = MakeLabel(TEXT(""), 13, FLinearColor::White);
    ValueBox->SetContent(ValueLabel);
    UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueBox);
    ValueSlot->SetVerticalAlignment(VAlign_Center);

    Sliders[Index] = Slider;
    ValueLabels[Index] = ValueLabel;
    UpdateValueLabel(Index);

    Parent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 3));
    return Slider;
}

UButton* USimConfigWidget::MakeButton(const FString& Text, bool bPrimary)
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

void USimConfigWidget::AddSectionHeader(UVerticalBox* Parent, const FString& Title)
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

void USimConfigWidget::UpdateValueLabel(ESliderIndex Index)
{
    if (!ValueLabels.IsValidIndex(Index) || !ValueLabels[Index] || !Sliders.IsValidIndex(Index))
    {
        return;
    }
    const float Value = Sliders[Index]->GetValue();
    ValueLabels[Index]->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
}

void USimConfigWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel)
    {
        StatusLabel->SetText(FText::FromString(Text));
    }
}
