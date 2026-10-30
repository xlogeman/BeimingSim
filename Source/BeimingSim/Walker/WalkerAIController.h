// WalkerAIController.h - 驱动 WalkerNPC 巡逻的 AI 控制器

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Walker/WalkerNPC.h"
#include "WalkerAIController.generated.h"

/**
 * 巡逻逻辑（状态机）：
 *   走向目标点 -> 到达后停留 WaitTimeAtPoint 秒（播放站立动画）-> 走向下一个点 -> 无限循环。
 *
 * 支持三种巡逻模式（由 WalkerNPC 的 PatrolMode 决定）：
 *   1. TwoPoints  ：在 A、B 两个箭头之间往返；
 *   2. Waypoints  ：按顺序依次走完路径点列表，循环；
 *   3. Random     ：在出生点周围随机游走。
 *
 * 移动方式（自动选择）：
 *   场景中存在 NavMeshBoundsVolume 时用 AI 寻路移动（会绕障碍物）；
 *   没有导航网格时，退化为直接直线移动，保证一定能走（会输出警告日志提醒）。
 */
UCLASS()
class BEIMINGSIM_API AWalkerAIController : public AAIController
{
	GENERATED_BODY()

public:
	AWalkerAIController();

	/** 由 WalkerNPC 的 BeginPlay 调用，开始巡逻（幂等） */
	void BeginPatrol();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID MoveRequestID, const FPathFollowingResult& Result) override;

	/** 到达判定距离（cm），距离目标小于该值即认为到达 */
	UPROPERTY(EditAnywhere, Category = "WalkerAI")
	float AcceptanceRadius = 50.0f;

private:
	void MoveToNextPoint();
	void OnArriveAtPoint();
	void ResumePatrol();
	bool DetectNavMesh() const;
	FVector ClampToNavMesh(const FVector& Point) const;
	FVector PickRandomPoint() const;

	/** 指挥的 NPC（Possess 后由引擎自动维护引用） */
	UPROPERTY()
	TObjectPtr<AWalkerNPC> ControlledNPC;

	/** 当前目标点索引：两点模式下 0=A, 1=B；多点模式下为 WaypointLocations 的下标 */
	int32 CurrentTargetIndex = 0;

	/** 当前巡逻模式（BeginPatrol 时从 NPC 读取） */
	EWalkerPatrolMode PatrolMode = EWalkerPatrolMode::TwoPoints;

	/** 多点模式的路径点世界坐标（开局快照，不随标记移动） */
	TArray<FVector> WaypointLocations;

	/** 当前这一段的目标点（寻路失败退化为直线移动时使用） */
	FVector CurrentTargetLocation = FVector::ZeroVector;

	/** 随机游走的中心（NPC 开局位置） */
	FVector RandomHomeLocation = FVector::ZeroVector;

	/** 是否已开始巡逻 */
	bool bPatrolStarted = false;

	/** 是否正处于端点停留状态 */
	bool bWaitingAtPoint = false;

	/** 是否使用导航网格寻路（false 时走直线移动） */
	bool bUseNavMeshPathing = true;

	/** 是否已输出过"无导航网格"警告（避免刷屏） */
	bool bWarnedNoNavMesh = false;

	/** 直线移动模式的当前目标 */
	bool bManualMoveActive = false;
	FVector ManualMoveTarget = FVector::ZeroVector;

	/** 端点停留计时器 */
	FTimerHandle WaitTimerHandle;
};
