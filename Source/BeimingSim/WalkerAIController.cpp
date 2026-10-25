// WalkerAIController.cpp

#include "WalkerAIController.h"

#include "WalkerNPC.h"
#include "WalkerWaypoint.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AWalkerAIController::AWalkerAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AWalkerAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledNPC = Cast<AWalkerNPC>(InPawn);
	if (!ControlledNPC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WalkerAI] %s 只能控制 WalkerNPC，当前 Pawn 是 %s"),
			*GetName(), *GetNameSafe(InPawn));
	}
}

void AWalkerAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(WaitTimerHandle);
	if (ControlledNPC)
	{
		ControlledNPC->SetWalkingVisuals(false);
	}
	ControlledNPC = nullptr;
	bPatrolStarted = false;
	bWaitingAtPoint = false;
	bManualMoveActive = false;

	Super::OnUnPossess();
}

void AWalkerAIController::BeginPatrol()
{
	if (bPatrolStarted || !ControlledNPC)
	{
		return;
	}
	bPatrolStarted = true;

	// 读取巡逻模式并做初始化
	PatrolMode = ControlledNPC->PatrolMode;
	RandomHomeLocation = ControlledNPC->GetActorLocation();

	WaypointLocations.Reset();
	if (PatrolMode == EWalkerPatrolMode::Waypoints)
	{
		for (const TObjectPtr<AWalkerWaypoint>& Waypoint : ControlledNPC->Waypoints)
		{
			if (IsValid(Waypoint))
			{
				WaypointLocations.Add(Waypoint->GetActorLocation());
			}
		}
		if (WaypointLocations.Num() < 2)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[WalkerAI] %s 的 Waypoints 有效路径点少于 2 个，退回两点往返模式。"),
				*GetNameSafe(ControlledNPC));
			PatrolMode = EWalkerPatrolMode::TwoPoints;
		}
	}

	CurrentTargetIndex = 0;
	ControlledNPC->SetWalkingVisuals(true);
	MoveToNextPoint();
}

void AWalkerAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 直线移动模式：每帧朝目标施加移动输入
	if (!bManualMoveActive || !ControlledNPC)
	{
		return;
	}

	FVector ToTarget = ManualMoveTarget - ControlledNPC->GetActorLocation();
	ToTarget.Z = 0.0f;

	if (ToTarget.Size() <= AcceptanceRadius)
	{
		bManualMoveActive = false;
		OnArriveAtPoint();
		return;
	}

	ControlledNPC->AddMovementInput(ToTarget.GetSafeNormal(), 1.0f);
}

void AWalkerAIController::OnMoveCompleted(FAIRequestID MoveRequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(MoveRequestID, Result);

	if (!ControlledNPC || bWaitingAtPoint)
	{
		return;
	}

	if (Result.IsSuccess())
	{
		OnArriveAtPoint();
	}
	else
	{
		// 寻路失败（例如导航没覆盖端点），本段退化为直线移动，下一段自动重试寻路
		UE_LOG(LogTemp, Warning,
			TEXT("[WalkerAI] %s 寻路失败，本段改用直线移动。"),
			*GetNameSafe(ControlledNPC));
		bUseNavMeshPathing = false;
		bManualMoveActive = true;
		ManualMoveTarget = ClampToNavMesh(CurrentTargetLocation);
	}
}

void AWalkerAIController::MoveToNextPoint()
{
	if (!ControlledNPC)
	{
		return;
	}

	// 每段路重新检测导航网格（导航可能在开局后才构建完成）
	bUseNavMeshPathing = DetectNavMesh();
	if (!bUseNavMeshPathing && !bWarnedNoNavMesh)
	{
		bWarnedNoNavMesh = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[WalkerAI] %s 未检测到导航网格，将直线来回走。如需自动绕障碍并限制在绿色区域内，请放置 NavMeshBoundsVolume 覆盖地面（按 P 键查看）。"),
			*GetNameSafe(ControlledNPC));
	}
	if (bUseNavMeshPathing)
	{
		bWarnedNoNavMesh = false;
	}

	// 按巡逻模式确定这一段的目标点
	FVector Target = FVector::ZeroVector;
	switch (PatrolMode)
	{
	case EWalkerPatrolMode::TwoPoints:
		Target = ControlledNPC->GetPatrolPointLocation(CurrentTargetIndex);
		break;

	case EWalkerPatrolMode::Waypoints:
		Target = WaypointLocations.IsValidIndex(CurrentTargetIndex)
			? WaypointLocations[CurrentTargetIndex]
			: ControlledNPC->GetActorLocation();
		break;

	case EWalkerPatrolMode::Random:
		Target = PickRandomPoint();
		break;
	}
	CurrentTargetLocation = Target;

	if (bUseNavMeshPathing)
	{
		// 关键：把巡逻点吸附到导航网格上，保证目标一定在绿色区域内且可寻路
		Target = ClampToNavMesh(Target);

		const EPathFollowingRequestResult::Type Result =
			MoveToLocation(Target, AcceptanceRadius);

		if (Result == EPathFollowingRequestResult::RequestSuccessful)
		{
			return; // 等待 OnMoveCompleted 回调
		}

		if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			OnArriveAtPoint();
			return;
		}

		// Failed：本段退化为直线移动（下一段会重新尝试寻路）
		UE_LOG(LogTemp, Warning,
			TEXT("[WalkerAI] %s 寻路请求失败，本段改用直线移动。"),
			*GetNameSafe(ControlledNPC));
	}

	bManualMoveActive = true;
	ManualMoveTarget = ClampToNavMesh(Target);
}

void AWalkerAIController::OnArriveAtPoint()
{
	if (!ControlledNPC)
	{
		return;
	}

	bWaitingAtPoint = true;
	bManualMoveActive = false;
	ControlledNPC->SetWalkingVisuals(false);

	const float Wait = FMath::Max(0.0f, ControlledNPC->WaitTimeAtPoint);
	if (Wait <= KINDA_SMALL_NUMBER)
	{
		ResumePatrol();
		return;
	}

	GetWorldTimerManager().SetTimer(WaitTimerHandle, this, &AWalkerAIController::ResumePatrol, Wait, false);
}

void AWalkerAIController::ResumePatrol()
{
	bWaitingAtPoint = false;
	if (!ControlledNPC)
	{
		return;
	}

	// 按模式推进到下一个目标
	switch (PatrolMode)
	{
	case EWalkerPatrolMode::TwoPoints:
		CurrentTargetIndex = (CurrentTargetIndex + 1) % 2;
		break;

	case EWalkerPatrolMode::Waypoints:
		if (WaypointLocations.Num() > 0)
		{
			CurrentTargetIndex = (CurrentTargetIndex + 1) % WaypointLocations.Num();
		}
		break;

	case EWalkerPatrolMode::Random:
		break; // 随机模式每次重新选点，无需索引
	}

	ControlledNPC->SetWalkingVisuals(true);
	MoveToNextPoint();
}

bool AWalkerAIController::DetectNavMesh() const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys || !ControlledNPC)
	{
		return false;
	}

	// 依次尝试 NPC 当前位置与两个巡逻点，任一投影成功即认为导航可用
	FNavLocation Projected;
	const FVector TestPoints[3] = {
		ControlledNPC->GetActorLocation(),
		ControlledNPC->GetPatrolPointLocation(0),
		ControlledNPC->GetPatrolPointLocation(1)
	};

	for (const FVector& Point : TestPoints)
	{
		if (NavSys->ProjectPointToNavigation(Point, Projected, FVector(500.0f, 500.0f, 1000.0f)))
		{
			return true;
		}
	}
	return false;
}

FVector AWalkerAIController::ClampToNavMesh(const FVector& Point) const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return Point;
	}

	// 在附近 10 米范围内寻找导航网格上的最近点
	FNavLocation Projected;
	if (NavSys->ProjectPointToNavigation(Point, Projected, FVector(1000.0f, 1000.0f, 1000.0f)))
	{
		return Projected.Location;
	}
	return Point;
}

FVector AWalkerAIController::PickRandomPoint() const
{
	const float Radius = FMath::Max(100.0f,
		ControlledNPC ? ControlledNPC->RandomWanderRadius : 800.0f);

	// 有导航网格：取半径内随机可达点（保证走得过去）
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation RandomPoint;
		if (NavSys->GetRandomReachablePointInRadius(RandomHomeLocation, Radius, RandomPoint))
		{
			return RandomPoint.Location;
		}
	}

	// 没有导航网格：在半径内随机取一个平面点
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist = FMath::Sqrt(FMath::FRandRange(0.0f, 1.0f)) * Radius;
	return RandomHomeLocation + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.0f);
}
