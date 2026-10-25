// WalkerNPC.h - 会来回走动的 NPC 角色

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WalkerNPC.generated.h"

class USkeletalMesh;
class UAnimationAsset;
class UArrowComponent;
class AWalkerWaypoint;

/** 巡逻模式 */
UENUM(BlueprintType)
enum class EWalkerPatrolMode : uint8
{
	/** 在 A、B 两个箭头之间往返 */
	TwoPoints UMETA(DisplayName = "两点往返（A/B 箭头）"),

	/** 按顺序依次走完 Waypoints 列表，循环 */
	Waypoints UMETA(DisplayName = "多点按顺序巡逻"),

	/** 在出生点周围随机游走 */
	Random UMETA(DisplayName = "随机游走")
};

/**
 * 在 A、B 两个巡逻点之间来回走动的 NPC。
 * 由 AWalkerAIController 自动接管（放置到场景中即生效），无需配置蓝图。
 */
UCLASS()
class BEIMINGSIM_API AWalkerNPC : public ACharacter
{
	GENERATED_BODY()

public:
	AWalkerNPC();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// ---------------- 外观（在详情面板中指定资产） ----------------

	/** 骨骼网格体，例如迁移过来的 SKM_Quinn / SKM_Manny */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|外观")
	TObjectPtr<USkeletalMesh> SkeletalMeshAsset;

	/** 走路动画，例如 MM_Walk_Fwd（动画资产需勾选 Looping） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|外观")
	TObjectPtr<UAnimationAsset> WalkAnim;

	/** 待机动画（可选；不填则在停留时暂停走路动画） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|外观")
	TObjectPtr<UAnimationAsset> IdleAnim;

	/** 走路动画播放速率，用来匹配实际移动速度（1 = 原始速度） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|外观", meta = (ClampMin = "0.05"))
	float WalkAnimPlayRate = 1.0f;

	// ---------------- 移动参数 ----------------

	/** 巡逻模式 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动")
	EWalkerPatrolMode PatrolMode = EWalkerPatrolMode::TwoPoints;

	/** 路径点列表（多点巡逻模式：按数组顺序循环走，可在 Place Actors 中搜 WalkerWaypoint 创建） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动",
		meta = (EditCondition = "PatrolMode == EWalkerPatrolMode::Waypoints", EditConditionHides))
	TArray<TObjectPtr<AWalkerWaypoint>> Waypoints;

	/** 随机游走半径（cm），以 NPC 放置位置为中心 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动",
		meta = (EditCondition = "PatrolMode == EWalkerPatrolMode::Random", EditConditionHides, ClampMin = "100"))
	float RandomWanderRadius = 800.0f;

	/** 走路速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动", meta = (ClampMin = "10"))
	float WalkSpeed = 160.0f;

	/** 到达端点后的停留时间（秒），停留期间播放待机（站立）动画 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动", meta = (ClampMin = "0"))
	float WaitTimeAtPoint = 2.0f;

	/** 转身速度（度/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动", meta = (ClampMin = "1"))
	float TurnSpeed = 240.0f;

	/** 巡逻点 A（场景中的绿色箭头，直接拖动即可） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动")
	TObjectPtr<UArrowComponent> PatrolPointA;

	/** 巡逻点 B（场景中的绿色箭头，直接拖动即可） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WalkerNPC|移动")
	TObjectPtr<UArrowComponent> PatrolPointB;

	/** 游戏开始时是否画出巡逻路径（调试用，绿色线段） */
	UPROPERTY(EditAnywhere, Category = "WalkerNPC|调试")
	bool bDrawDebugPath = false;

	// ---------------- 供 AI 控制器调用的接口 ----------------

	/** 切换走路 / 待机的动画表现 */
	void SetWalkingVisuals(bool bWalking);

	/** 获取第 Index 个巡逻点（0=A, 1=B）的世界坐标 */
	FVector GetPatrolPointLocation(int32 Index) const;

private:
	void ApplyAppearance();
	void CachePatrolPoints();

	/** 开局定格的巡逻点世界坐标（不随角色移动，保证路线固定不变） */
	FVector CachedPatrolPoints[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bPatrolPointsCached = false;
};
