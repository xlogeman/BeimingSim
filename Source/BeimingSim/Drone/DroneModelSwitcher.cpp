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
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TimerManager.h"

// AirSim 多旋翼 Pawn（Rotation0~3 旋翼组件由其蓝图定义）
#include "Vehicles/Multirotor/FlyingPawn.h"
// AirSim SimMode：提供 reset() 整体重置（回到初始位姿 + 飞控/物理重置）
#include "SimMode/SimModeBase.h"

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
    SyncInitialSelection();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID(), 10.f, FColor::Cyan,
            TEXT("按 Tab 打开机型选择菜单"));
    }

    // 延迟一帧多，等关卡与渲染状态就绪后再生成预览缩略图
    GetWorldTimerManager().SetTimer(ThumbnailTimer, this,
        &ADroneModelSwitcher::GenerateThumbnails, 0.25f, false);
}

void ADroneModelSwitcher::DiscoverModels()
{
    DiscoveredClasses.Reset();
    DisplayNames.Reset();

    // 第 0 项固定为 AirSim 自带的默认机型（支持"切回默认"）。
    // 存的是蓝图生成的类（*_C）：UBlueprint 是编辑器专用类型，打包版无法加载，
    // 资产路径追加 "_C" 后即指向其 BlueprintGeneratedClass（编辑器/打包版均可加载）
    {
        FString ClassPath = DefaultPawnBlueprintPath;
        if (!ClassPath.EndsWith(TEXT("_C")))
        {
            ClassPath += TEXT("_C");
        }
        DiscoveredClasses.Add(TSoftClassPtr<AFlyingPawn>(FSoftObjectPath(ClassPath)));
        DisplayNames.Add(DefaultModelName);
    }

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

#if WITH_EDITOR
    // 编辑器下确保扫描目录已入注册表（启动早期可能还没异步扫到）
    // 注意：打包版绝不能调用（尤其带 ForceRescan）——打包资产在 pak 里、磁盘上不存在，
    // ForceRescan 会把预置注册表中该路径的资产条目按"磁盘已删除"清空，
    // 导致后续 GetAssets 查不到任何机型（AssetRegistry.cpp: OldAssetsToRemove）
    Registry->ScanPathsSynchronous({ ScanPath }, true);
#endif

    // 查询扫描目录下的全部资产（不按类过滤，也不依赖类派生数据——
    // 打包版注册表中不保证有派生关系数据，按目录扫描最稳妥），
    // 随后在循环里按"蓝图/生成的类"特征预筛并加载验证是否 AFlyingPawn 子类
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry->GetAssets(Filter, Assets);
    UE_LOG(LogTemp, Log, TEXT("DroneSwitcher: 扫描 %s 下资产 %d 个"), *ScanPath, Assets.Num());

    TArray<FString> TempNames;
    TArray<TSoftClassPtr<AFlyingPawn>> TempClasses;
    TSet<FName> SeenPackages;
    for (const FAssetData& Asset : Assets)
    {
        // 预筛：只要蓝图资产（编辑器下类为 Blueprint；打包下资产即生成的类，
        // 对象路径以 "_C" 结尾）。材质/网格/纹理等直接跳过，避免无谓加载
        const FString ObjectPath = Asset.ToSoftObjectPath().ToString();
        const bool bIsBlueprintAsset =
            Asset.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName();
        const bool bIsGeneratedClass = ObjectPath.EndsWith(TEXT("_C"));
        if (!bIsBlueprintAsset && !bIsGeneratedClass)
        {
            continue;
        }

        // 编辑器下同一蓝图可能同时命中多个查询，按包名去重
        if (SeenPackages.Contains(Asset.PackageName))
        {
            continue;
        }
        SeenPackages.Add(Asset.PackageName);

        // 资产对象路径在打包版即生成的类（*_C）；编辑器下为蓝图资产路径，同样补 "_C"
        FString ClassPath = ObjectPath;
        if (!ClassPath.EndsWith(TEXT("_C")))
        {
            ClassPath += TEXT("_C");
        }

        // 同步加载验证：确为 AFlyingPawn 子类才收入菜单（加载失败/类型不符则跳过）
        // 注意用花括号初始化：圆括号会被 MSVC 解析成函数声明（most vexing parse）
        TSoftClassPtr<AFlyingPawn> ClassRef{FSoftObjectPath(ClassPath)};
        UClass* LoadedClass = ClassRef.LoadSynchronous();
        if (!LoadedClass || !LoadedClass->IsChildOf(AFlyingPawn::StaticClass()))
        {
            continue;
        }

        TempClasses.Add(ClassRef);
        // 显示名取蓝图资产名（BP_X450 -> X450）。打包版资产名可能带 "_C" 后缀
        // （资产即生成的类），所以从包名推导更稳妥：/Game/UAV/BP_X450 -> X450
        FString PackageName = Asset.PackageName.ToString();
        int32 SlashIdx = 0;
        PackageName.FindLastChar('/', SlashIdx);
        FString Name = PackageName.Mid(SlashIdx + 1);
        Name.RemoveFromStart(TEXT("BP_"));
        TempNames.Add(Name);
    }

    // 按显示名字母序排序（不区分大小写），菜单展示与索引对应关系保持一致
    TArray<int32> Order;
    for (int32 i = 0; i < TempNames.Num(); ++i)
    {
        Order.Add(i);
    }
    Order.Sort([&TempNames](int32 A, int32 B)
    {
        return FCString::Stricmp(*TempNames[A], *TempNames[B]) < 0;
    });
    for (int32 Idx : Order)
    {
        DiscoveredClasses.Add(TempClasses[Idx]);
        DisplayNames.Add(TempNames[Idx]);
        UE_LOG(LogTemp, Log, TEXT("DroneSwitcher: 发现机型 %s (%s)"),
            *TempNames[Idx], *TempClasses[Idx].ToSoftObjectPath().ToString());
    }
}

void ADroneModelSwitcher::GenerateThumbnails()
{
    if (!GetWorld())
    {
        return;
    }
    ModelThumbnails.Reset();

    // 阶段 1：先生成全部临时实例（远离场景的高空），等一拍再拍，
    // 避免同帧生成+拍照时网格渲染代理尚未就绪拍到黑图
    TempInstances.Reset();
    const FVector SpawnLoc(0.0, 0.0, 5000.0);
    for (int32 i = 0; i < DiscoveredClasses.Num(); ++i)
    {
        AActor* Source = nullptr;
        UClass* ModelClass = DiscoveredClasses[i].LoadSynchronous();
        if (ModelClass)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.ObjectFlags |= RF_Transient;
            Source = GetWorld()->SpawnActor<AActor>(ModelClass, FTransform(SpawnLoc), Params);
        }
        TempInstances.Add(Source);
    }
    GetWorldTimerManager().SetTimer(ThumbnailTimer, this,
        &ADroneModelSwitcher::CapturePendingThumbnails, 0.2f, false);
}

void ADroneModelSwitcher::CapturePendingThumbnails()
{
    // 阶段 2：逐个拍照、销毁临时实例并更新菜单
    for (int32 i = 0; i < TempInstances.Num(); ++i)
    {
        UTextureRenderTarget2D* Thumb = nullptr;
        if (TempInstances[i])
        {
            Thumb = CaptureThumbnail(TempInstances[i]);
            TempInstances[i]->Destroy();
        }
        ModelThumbnails.Add(Thumb);
        if (MenuWidget)
        {
            MenuWidget->SetEntryThumbnail(i, Thumb);
        }
    }
    TempInstances.Reset();
}

UTextureRenderTarget2D* ADroneModelSwitcher::CaptureThumbnail(AActor* Source) const
{
    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    RT->ClearColor = FLinearColor(0.02f, 0.03f, 0.06f, 1.f);
    RT->InitAutoFormat(256, 256);
    RT->UpdateResourceImmediate(true);

    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Source);
    Capture->TextureTarget = RT;
    // 根据包围盒自适应取景距离（系数越小模型在预览图中越大），约 3/4 俯视角度
    const FBox Bounds = Source->GetComponentsBoundingBox();
    const float Radius = FMath::Max3(Bounds.GetExtent().X, Bounds.GetExtent().Y, Bounds.GetExtent().Z);
    const float Dist = FMath::Clamp(Radius * 2.2f, 60.f, 400.f);
    const FVector Center = Bounds.GetCenter();
    const FVector Offset = FVector(-0.7f, -0.6f, 0.45f).GetSafeNormal() * Dist;
    Capture->SetWorldLocationAndRotation(Center + Offset, (-Offset).Rotation());
    Capture->FOVAngle = 60.f;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->bAlwaysPersistRenderingState = true;
    // 只渲染该机型实例，背景为 ClearColor，得到干净的预览图
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->ShowOnlyActors.Add(Source);
    Capture->RegisterComponent();
    Capture->CaptureScene();

    return RT;
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
    // 填充各机型的介绍与参数（悬停弹窗内容），未配置的机型由控件内部兜底
    for (int32 i = 0; i < DisplayNames.Num(); ++i)
    {
        if (const FDroneModelInfo* Info = ModelInfos.Find(DisplayNames[i]))
        {
            MenuWidget->SetEntryInfo(i, *Info);
        }
    }
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
    // 兜底：首次打开菜单前若同步重试还没等到 Pawn，此时 Pawn 必然已就绪，做最后一次同步
    if (!bInitialSelectionSynced)
    {
        GetWorldTimerManager().ClearTimer(SyncSelectionTimer);
        if (MenuWidget)
        {
            MenuWidget->HighlightSelected(FindDronePawn() ? GetCurrentModelIndex() : -1);
        }
        bInitialSelectionSynced = true;
    }
    if (MenuWidget)
    {
        MenuWidget->ToggleMenu();
    }
}

void ADroneModelSwitcher::SyncInitialSelection()
{
    if (FindDronePawn())
    {
        // 根据场景中实际飞行的机型同步初始选中态（选中项不可点击）
        if (MenuWidget)
        {
            MenuWidget->HighlightSelected(GetCurrentModelIndex());
        }
        bInitialSelectionSynced = true;
        return;
    }

    // 无人机 Pawn 由 AirSim SimMode 在 BeginPlay 流程中延迟生成，稍后重试
    if (++SyncSelectionAttempts >= MaxSyncAttempts)
    {
        if (MenuWidget)
        {
            MenuWidget->HighlightSelected(-1);
        }
        bInitialSelectionSynced = true;
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 等待 %d 次仍未找到 AFlyingPawn，初始选中态未同步"),
            MaxSyncAttempts);
        return;
    }
    GetWorldTimerManager().SetTimer(SyncSelectionTimer, this,
        &ADroneModelSwitcher::SyncInitialSelection, 0.5f, false);
}

AFlyingPawn* ADroneModelSwitcher::FindDronePawn() const
{
    for (TActorIterator<AFlyingPawn> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void ADroneModelSwitcher::ResetSimulation()
{
    // SimMode 由 AirSim 的 SimHUD 在运行时动态生成。reset() 会把所有载具瞬移回
    // 初始起飞位姿（TeleportPhysics）并重置飞控与物理引擎，效果等同重新点击 Play
    for (TActorIterator<ASimModeBase> It(GetWorld()); It; ++It)
    {
        It->reset();
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 未找到 AirSim SimMode，跳过位置重置"));
}

int32 ADroneModelSwitcher::GetCurrentModelIndex() const
{
    const AFlyingPawn* Pawn = FindDronePawn();
    if (!Pawn)
    {
        return -1;
    }
    // 与已发现的机型类逐一比对。启动时机型类都还是未加载的软引用，
    // 必须同步加载后才能比较（缩略图阶段本就会全部加载，无额外开销）
    for (int32 i = 0; i < DiscoveredClasses.Num(); ++i)
    {
        UClass* ModelClass = DiscoveredClasses[i].LoadSynchronous();
        if (ModelClass && Pawn->GetClass()->IsChildOf(ModelClass))
        {
            return i;
        }
    }
    return -1;
}

void ADroneModelSwitcher::ApplyModel(int32 Index)
{
    if (!DiscoveredClasses.IsValidIndex(Index))
    {
        return;
    }
    AFlyingPawn* Pawn = FindDronePawn();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 场景中没有 AFlyingPawn，无法切换外观"));
        return;
    }
    UClass* ModelClass = DiscoveredClasses[Index].LoadSynchronous();
    if (!ModelClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 机型类加载失败"));
        return;
    }
    ApplyAppearanceFromBlueprint(Pawn, ModelClass);
    // 换外观后整体重置：无人机回到初始起飞坐标、从空中掉落（等效重新点击 Play）
    ResetSimulation();
    if (MenuWidget)
    {
        MenuWidget->HighlightSelected(Index);
    }

    const FString Name = DisplayNames.IsValidIndex(Index) ? DisplayNames[Index]
        : DiscoveredClasses[Index].ToSoftObjectPath().GetAssetName();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID() + Index + 1, 3.f, FColor::Green,
            FString::Printf(TEXT("外观已切换为 %s"), *Name));
    }
    UE_LOG(LogTemp, Log, TEXT("DroneSwitcher: 外观已切换为 %s"), *Name);
}

void ADroneModelSwitcher::ApplyAppearanceFromBlueprint(AFlyingPawn* Pawn, UClass* SourceClass)
{
    // 注意：蓝图 CDO 上不包含在蓝图编辑器里添加的组件（SCS 组件只在实例构造时创建），
    // 因此这里临时生成一个实例来读取装配状态，读完立刻销毁（同帧内，不会渲染出来）
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    AActor* Source = GetWorld()->SpawnActor<AActor>(SourceClass, FTransform::Identity, Params);
    if (!Source)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneSwitcher: 生成机型 %s 的临时实例失败"),
            *SourceClass->GetName());
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
