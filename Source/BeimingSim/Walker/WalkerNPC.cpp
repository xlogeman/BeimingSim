// WalkerNPC.cpp

#include "WalkerNPC.h"

#include "Walker/WalkerAIController.h"
#include "Walker/WalkerWaypoint.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

AWalkerNPC::AWalkerNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// 默认外观：自动引用 Characters 共享包里的人偶资产（详情面板中仍可手动覆盖）
	// 如果工程里没有这些资产，则保持为空，由用户在详情面板手动指定
	{
		ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
		if (MeshFinder.Succeeded())
		{
			SkeletalMeshAsset = MeshFinder.Object;
		}

		ConstructorHelpers::FObjectFinder<UAnimationAsset> WalkFinder(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
		if (WalkFinder.Succeeded())
		{
			WalkAnim = WalkFinder.Object;
		}

		ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleFinder(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
		if (IdleFinder.Succeeded())
		{
			IdleAnim = IdleFinder.Object;
		}
	}

	// 不跟随控制器朝向，改为"自动朝移动方向转身"
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnSpeed, 0.0f);
	GetCharacterMovement()->BrakingDecelerationWalking = 800.0f;

	// 放置到场景中时自动被 AWalkerAIController 接管
	AIControllerClass = AWalkerAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;

	// 人物网格体对齐：人偶锚点在脚底，胶囊体原点在中心，
	// 把 Mesh 下移一个胶囊半高，让脚底正好贴住胶囊体底部
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	// UE 人偶模型原始朝向是 +Y，转 -90° 使其面向角色正前方（+X）
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// 两个巡逻点：绿色箭头，直接在视口中拖动
	PatrolPointA = CreateDefaultSubobject<UArrowComponent>(TEXT("PatrolPointA"));
	PatrolPointA->SetupAttachment(RootComponent);
	PatrolPointA->ArrowSize = 2.5f;
	PatrolPointA->SetHiddenInGame(true);

	PatrolPointB = CreateDefaultSubobject<UArrowComponent>(TEXT("PatrolPointB"));
	PatrolPointB->SetupAttachment(RootComponent);
	PatrolPointB->ArrowSize = 2.5f;
	PatrolPointB->SetHiddenInGame(true);
	PatrolPointB->SetRelativeLocation(FVector(400.0f, 0.0f, 0.0f)); // 默认在身前 4 米，方便找到
}

void AWalkerNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 编辑器中实时显示指定的人物模型
	ApplyAppearance();

	// 仅两点模式显示 A/B 箭头，其他模式隐藏以免干扰
	const bool bShowPoints = (PatrolMode == EWalkerPatrolMode::TwoPoints);
	PatrolPointA->SetVisibility(bShowPoints);
	PatrolPointB->SetVisibility(bShowPoints);
}

void AWalkerNPC::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// 关键：开局瞬间定格两个巡逻点的世界坐标，
	// 否则点会跟着角色移动，路线永远"追着自己跑"，表现为乱走
	CachePatrolPoints();

	if (bDrawDebugPath)
	{
		switch (PatrolMode)
		{
		case EWalkerPatrolMode::TwoPoints:
		{
			const FVector A = GetPatrolPointLocation(0);
			const FVector B = GetPatrolPointLocation(1);
			DrawDebugLine(GetWorld(), A, B, FColor::Green, true, 10.0f, 0, 2.0f);
			DrawDebugSphere(GetWorld(), A, 25.0f, 12, FColor::Green, true, 10.0f);
			DrawDebugSphere(GetWorld(), B, 25.0f, 12, FColor::Green, true, 10.0f);
			break;
		}
		case EWalkerPatrolMode::Waypoints:
		{
			TArray<FVector> Points;
			for (const TObjectPtr<AWalkerWaypoint>& Waypoint : Waypoints)
			{
				if (IsValid(Waypoint))
				{
					Points.Add(Waypoint->GetActorLocation());
				}
			}
			for (int32 i = 0; i < Points.Num(); ++i)
			{
				DrawDebugSphere(GetWorld(), Points[i], 25.0f, 12, FColor::Yellow, true, 10.0f);
				if (Points.Num() > 1)
				{
					DrawDebugLine(GetWorld(), Points[i], Points[(i + 1) % Points.Num()],
						FColor::Yellow, true, 10.0f, 0, 2.0f);
				}
			}
			break;
		}
		case EWalkerPatrolMode::Random:
			DrawDebugCircle(GetWorld(), GetActorLocation(), RandomWanderRadius, 48,
				FColor::Cyan, true, 10.0f, 0, 2.0f,
				FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);
			break;
		}
	}

	// 世界开始运行后启动巡逻（此时控制器已接管完毕、导航数据可用）
	if (AWalkerAIController* AI = Cast<AWalkerAIController>(GetController()))
	{
		AI->BeginPatrol();
	}
}

void AWalkerNPC::CachePatrolPoints()
{
	CachedPatrolPoints[0] = PatrolPointA ? PatrolPointA->GetComponentLocation() : GetActorLocation();
	CachedPatrolPoints[1] = PatrolPointB ? PatrolPointB->GetComponentLocation() : GetActorLocation();
	bPatrolPointsCached = true;

	// 两个点几乎重合时给出提示（会导致原地打转）
	if (FVector::DistXY(CachedPatrolPoints[0], CachedPatrolPoints[1]) < 50.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WalkerNPC] %s 的两个巡逻点几乎重合！请在编辑器里把 PatrolPointA / PatrolPointB 拖开到路线两端。"),
			*GetName());
	}
}

void AWalkerNPC::ApplyAppearance()
{
	if (SkeletalMeshAsset && GetMesh()->GetSkeletalMeshAsset() != SkeletalMeshAsset)
	{
		GetMesh()->SetSkeletalMesh(SkeletalMeshAsset);
	}
}

void AWalkerNPC::SetWalkingVisuals(bool bWalking)
{
	UAnimationAsset* TargetAnim = bWalking ? WalkAnim : IdleAnim;

	if (!TargetAnim)
	{
		// 没有待机动画时：停留期间直接暂停走路动画
		if (!bWalking && WalkAnim)
		{
			GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			GetMesh()->OverrideAnimationData(WalkAnim, /*bIsPlaying*/ false);
		}
		return;
	}

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->OverrideAnimationData(TargetAnim, /*bIsPlaying*/ true, /*bPositionShouldReset*/ true,
		/*InPosition*/ 0.0f, /*InPlayRate*/ bWalking ? WalkAnimPlayRate : 1.0f);

	// 无论资产是否勾选 Looping，都强制循环播放（待机也需要循环）
	if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(GetMesh()->GetAnimInstance()))
	{
		SingleNode->SetLooping(true);

		// 打包(Cooked)版本中 OverrideAnimationData 可能未真正把资产写入单节点实例，
		// 表现为 NPC 以参考姿势平移滑动。这里校验并强制补设一次，
		// 保证编辑器与打包版行为一致。
		if (SingleNode->GetAnimationAsset() != TargetAnim)
		{
			SingleNode->SetAnimationAsset(TargetAnim, /*bInIsLooping*/ true,
				/*InPlayRate*/ bWalking ? WalkAnimPlayRate : 1.0f);
			SingleNode->SetPosition(0.0f);
			SingleNode->SetPlaying(true);
			SingleNode->SetLooping(true);
		}
	}
}

FVector AWalkerNPC::GetPatrolPointLocation(int32 Index) const
{
	const int32 Idx = Index % 2;

	// 开局后一律使用定格的世界坐标（点挂在角色身上会跟着移动）
	if (bPatrolPointsCached)
	{
		return CachedPatrolPoints[Idx];
	}

	const UArrowComponent* Point = (Idx == 0) ? PatrolPointA.Get() : PatrolPointB.Get();
	return Point ? Point->GetComponentLocation() : GetActorLocation();
}
