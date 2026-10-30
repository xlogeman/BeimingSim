#include "DroneModelSwitcher.h"
#include "Drone/DroneSwitchWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Blueprint/UserWidget.h"

// AirSim 多旋翼 Pawn（Rotation0~3 旋翼组件由其蓝图定义）
#include "Vehicles/Multirotor/FlyingPawn.h"

ADroneModelSwitcher::ADroneModelSwitcher()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADroneModelSwitcher::BeginPlay()
{
    Super::BeginPlay();

    DiscoverModels();
    CreateWidgetInternal();
    BindTabKey();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID(), 10.f, FColor::Cyan,
            TEXT("按 Tab 打开机型选择菜单"));
    }
}

void ADroneModelSwitcher::DiscoverModels()
{
    DiscoveredBlueprints.Reset();
    DisplayNames.Reset();

    // 第 0 项固定为 AirSim 自带的默认机型（支持"切回默认"）
    DiscoveredBlueprints.Add(TSoftObjectPtr<UBlueprint>(FSoftObjectPath(DefaultPawnBlueprintPath)));
    DisplayNames.Add(DefaultModelName);

    IAssetRegistry* Registry = nullptr;
    if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
    {
        Registry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    }
    if (!Registry)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: AssetRegistry 不可用，菜单将只有默认机型"));
        return;
    }

    // 确保扫描目录已入注册表（编辑器/游戏启动早期可能还没扫到）
    Registry->ScanPathsSynchronous({ ScanPath }, true);

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

    TArray<FAssetData> Assets;
    Registry->GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
    {
        const UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            continue;
        }
        // 只要 AirSim 飞行 Pawn 的子类
        if (Blueprint->GeneratedClass->IsChildOf(AFlyingPawn::StaticClass()))
        {
            DiscoveredBlueprints.Add(TSoftObjectPtr<UBlueprint>(Asset.ToSoftObjectPath()));
            // 显示名取蓝图资产名（BP_X450 -> X450）
            FString Name = Asset.AssetName.ToString();
            Name.RemoveFromStart(TEXT("BP_"));
            DisplayNames.Add(Name);
            UE_LOG(LogTemp, Log, TEXT("DroneSwitcher: 发现机型 %s (%s)"),
                *Name, *Asset.GetSoftObjectPath().ToString());
        }
    }
}

void ADroneModelSwitcher::CreateWidgetInternal()
{
    if (!MenuWidget)
    {
        MenuWidget = CreateWidget<UDroneSwitchWidget>(GetWorld(), UDroneSwitchWidget::StaticClass());
    }
    if (!MenuWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 机型选择菜单创建失败"));
        return;
    }

    MenuWidget->BuildMenu(DisplayNames);
    MenuWidget->OnModelPicked.AddDynamic(this, &ADroneModelSwitcher::ApplyModel);
    MenuWidget->AddToViewport(100);
}

void ADroneModelSwitcher::BindTabKey()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->InputComponent)
    {
        PC->InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ADroneModelSwitcher::ToggleMenu);
    }
    else
    {
        // PlayerController 尚未就绪时稍后重试
        GetWorldTimerManager().SetTimer(TabBindRetryTimer, this,
            &ADroneModelSwitcher::BindTabKey, 1.0f, false);
    }
}

void ADroneModelSwitcher::ToggleMenu()
{
    if (MenuWidget)
    {
        MenuWidget->ToggleMenu();
    }
}

AFlyingPawn* ADroneModelSwitcher::FindDronePawn() const
{
    for (TActorIterator<AFlyingPawn> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void ADroneModelSwitcher::ApplyModel(int32 Index)
{
    if (!DiscoveredBlueprints.IsValidIndex(Index))
    {
        return;
    }
    AFlyingPawn* Pawn = FindDronePawn();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 场景中没有 AFlyingPawn，无法切换外观"));
        return;
    }
    UBlueprint* Blueprint = DiscoveredBlueprints[Index].LoadSynchronous();
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 蓝图加载失败"));
        return;
    }
    ApplyAppearanceFromBlueprint(Pawn, Blueprint);

    const FString Name = DisplayNames.IsValidIndex(Index) ? DisplayNames[Index]
        : DiscoveredBlueprints[Index].ToSoftObjectPath().GetAssetName();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID() + Index + 1, 3.f, FColor::Green,
            FString::Printf(TEXT("外观已切换为 %s"), *Name));
    }
    UE_LOG(LogTemp, Log, TEXT("DroneSwitcher: 外观已切换为 %s"), *Name);
}

void ADroneModelSwitcher::ApplyAppearanceFromBlueprint(AFlyingPawn* Pawn, UBlueprint* Blueprint)
{
    if (!Blueprint->GeneratedClass)
    {
        return;
    }

    // 注意：蓝图 CDO 上不包含在蓝图编辑器里添加的组件（SCS 组件只在实例构造时创建），
    // 因此这里临时生成一个实例来读取装配状态，读完立刻销毁（同帧内，不会渲染出来）
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    AActor* Source = GetWorld()->SpawnActor<AActor>(Blueprint->GeneratedClass, FTransform::Identity, Params);
    if (!Source)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 生成蓝图 %s 的临时实例失败"),
            *Blueprint->GetName());
        return;
    }

    // 1. 机身：把蓝图根组件的网格套到当前 Pawn 的根组件（碰撞跟随网格资产）
    if (UStaticMeshComponent* LiveRoot = Cast<UStaticMeshComponent>(Pawn->GetRootComponent()))
    {
        if (const UStaticMeshComponent* SourceRoot = Cast<UStaticMeshComponent>(Source->GetRootComponent()))
        {
            if (SourceRoot->GetStaticMesh())
            {
                LiveRoot->SetStaticMesh(SourceRoot->GetStaticMesh());
            }
        }
    }

    // 2. 旋翼槽位：按 Rotation0~3 逐个复制蓝图里对应桨叶的网格/位置/可见性
    for (int32 i = 0; i < 4; ++i)
    {
        const FName RotName = FName(*FString::Printf(TEXT("Rotation%d"), i));

        // 当前正在飞的 Pawn 上的桨叶组件
        URotatingMovementComponent* LiveRot = nullptr;
        TArray<URotatingMovementComponent*> LiveRots;
        Pawn->GetComponents<URotatingMovementComponent>(LiveRots);
        for (URotatingMovementComponent* Comp : LiveRots)
        {
            if (Comp->GetFName() == RotName)
            {
                LiveRot = Comp;
                break;
            }
        }
        UStaticMeshComponent* LiveBlade = LiveRot ? Cast<UStaticMeshComponent>(LiveRot->UpdatedComponent) : nullptr;
        if (!LiveBlade)
        {
            continue;
        }

        // 临时实例上的同槽位桨叶组件
        URotatingMovementComponent* SourceRot = nullptr;
        TArray<URotatingMovementComponent*> SourceRots;
        Source->GetComponents<URotatingMovementComponent>(SourceRots);
        for (URotatingMovementComponent* Comp : SourceRots)
        {
            if (Comp->GetFName() == RotName)
            {
                SourceRot = Comp;
                break;
            }
        }
        const UStaticMeshComponent* SourceBlade = SourceRot ? Cast<UStaticMeshComponent>(SourceRot->UpdatedComponent) : nullptr;

        if (SourceBlade && SourceBlade->GetStaticMesh())
        {
            LiveBlade->SetStaticMesh(SourceBlade->GetStaticMesh());
            LiveBlade->SetRelativeTransform(SourceBlade->GetRelativeTransform());
            LiveBlade->SetVisibility(SourceBlade->IsVisible(), true);
        }
        else
        {
            // 蓝图里该槽位为空：清空并隐藏
            LiveBlade->SetStaticMesh(nullptr);
            LiveBlade->SetVisibility(false, true);
        }
    }

    // 读完了，销毁临时实例
    Source->Destroy();
}
