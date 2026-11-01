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

// AirSim 运行时 API：天气/暂停回读、时钟速度回读
#include "SimMode/SimModeBase.h"
#include "Weather/WeatherLib.h"
#include "common/AirSimSettings.hpp"

namespace
{
    // 与 DroneSwitchWidget 保持一致的配色
    const FLinearColor PanelBg(0.04f, 0.05f, 0.09f, 0.94f);
    const FLinearColor SectionBg(0.10f, 0.12f, 0.18f, 0.55f);
    const FLinearColor Accent(0.15f, 0.62f, 0.95f, 1.f);
    const FLinearColor ParamLabelColor(0.62f, 0.68f, 0.76f, 1.f);
    const FLinearColor NoteColor(0.55f, 0.60f, 0.68f, 1.f);
    constexpr float PanelRadius = 12.f;
    constexpr float NameWidth = 110.f;
    constexpr float ValueWidth = 52.f;
    constexpr float ScrollHeight = 560.f;
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
    Box->AddChildToVerticalBox(MakeLabel(TEXT("仿真配置"), 20, FLinearColor::White))
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
    PauseEnabled->OnCheckStateChanged.AddDynamic(this, &USimConfigWidget::OnPauseChanged);
    UHorizontalBoxSlot* PauseCheckSlot = PauseRow->AddChildToHorizontalBox(PauseEnabled);
    PauseCheckSlot->SetPadding(FMargin(4, 0, 8, 0));
    PauseCheckSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* PauseLabelSlot = PauseRow->AddChildToHorizontalBox(
        MakeLabel(TEXT("暂停仿真"), 14, FLinearColor::White));
    PauseLabelSlot->SetVerticalAlignment(VAlign_Center);
    Content->AddChildToVerticalBox(PauseRow)->SetPadding(FMargin(0, 4));
    PauseCheck = PauseEnabled;

    MakeSliderRow(Content, TEXT("时钟速度"), ClockSpeed, 0.1f, 10.f, 1.f);
    Content->AddChildToVerticalBox(MakeLabel(TEXT("* 时钟速度：保存并重建仿真后生效"), 11, NoteColor))
        ->SetPadding(FMargin(4, 0, 0, 2));

    // ---- 状态与操作按钮 ----
    StatusLabel = MakeLabel(TEXT(""), 12, FLinearColor::Yellow);
    Content->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(4, 8, 0, 2));

    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BtnRow"));
    UButton* ApplyBtn = MakeButton(TEXT("应用（即时生效）"));
    ApplyBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnApplyLive);
    UHorizontalBoxSlot* ApplySlot = BtnRow->AddChildToHorizontalBox(ApplyBtn);
    ApplySlot->SetPadding(FMargin(2, 0));
    UButton* RebuildBtn = MakeButton(TEXT("保存并重建仿真"));
    RebuildBtn->OnClicked.AddDynamic(this, &USimConfigWidget::OnApplyRebuild);
    UHorizontalBoxSlot* RebuildSlot = BtnRow->AddChildToHorizontalBox(RebuildBtn);
    RebuildSlot->SetPadding(FMargin(2, 0));
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
    Sliders[ClockSpeed]->OnValueChanged.AddDynamic(this, &USimConfigWidget::OnSlider8);

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
    Sliders[ClockSpeed]->SetValue(msr::airlib::AirSimSettings::singleton().clock_speed);
    UpdateValueLabel(ClockSpeed);
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
void USimConfigWidget::OnSlider8(float Value) { OnSliderChanged(ClockSpeed, Value); }

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

void USimConfigWidget::OnApplyRebuild()
{
    if (!Manager.IsValid())
    {
        SetStatusText(TEXT("配置管理器未就绪"));
        return;
    }
    const bool bOk = Manager->ApplyAndRebuild(Sliders[ClockSpeed]->GetValue());
    SetStatusText(bOk ? TEXT("settings.json 已保存，SimMode 重建中…")
                      : TEXT("重建失败，详见日志"));
}

void USimConfigWidget::OnTimeMorning() { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(0); SetStatusText(TEXT("时间已设为清晨 06:30")); } }
void USimConfigWidget::OnTimeNoon()    { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(1); SetStatusText(TEXT("时间已设为正午 12:00")); } }
void USimConfigWidget::OnTimeEvening() { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(2); SetStatusText(TEXT("时间已设为黄昏 18:30")); } }
void USimConfigWidget::OnTimeNight()   { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(3); SetStatusText(TEXT("时间已设为夜晚 22:00")); } }
void USimConfigWidget::OnTimeDisable() { if (Manager.IsValid()) { Manager->ApplyTimeOfDay(-1); SetStatusText(TEXT("时间模拟已关闭")); } }

// ---- 控件工厂 ----

UTextBlock* USimConfigWidget::MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color) const
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Label->SetText(FText::FromString(Text));
    Label->SetColorAndOpacity(FSlateColor(Color));
    Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
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

UButton* USimConfigWidget::MakeButton(const FString& Text)
{
    UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    Btn->SetBackgroundColor(Accent);
    Btn->AddChild(MakeLabel(Text, 13, FLinearColor::White));
    if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Btn->GetContent()->Slot))
    {
        ContentSlot->SetPadding(FMargin(8, 3));
    }
    return Btn;
}

void USimConfigWidget::AddSectionHeader(UVerticalBox* Parent, const FString& Title)
{
    UBorder* Header = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Header->SetBrush(FSlateRoundedBoxBrush(SectionBg, 6.f));
    Header->SetPadding(FMargin(8, 4));
    Header->SetContent(MakeLabel(Title, 15, FLinearColor::White));
    Parent->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 8, 0, 4));
}

void USimConfigWidget::UpdateValueLabel(ESliderIndex Index)
{
    if (!ValueLabels.IsValidIndex(Index) || !ValueLabels[Index] || !Sliders.IsValidIndex(Index))
    {
        return;
    }
    const float Value = Sliders[Index]->GetValue();
    ValueLabels[Index]->SetText(FText::FromString(
        Index == ClockSpeed ? FString::Printf(TEXT("%.2f"), Value)
                            : FString::Printf(TEXT("%.1f"), Value)));
}

void USimConfigWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel)
    {
        StatusLabel->SetText(FText::FromString(Text));
    }
}
