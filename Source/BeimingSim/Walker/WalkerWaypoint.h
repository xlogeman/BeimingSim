// WalkerWaypoint.h - 巡逻路径点标记

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WalkerWaypoint.generated.h"

class UArrowComponent;

/**
 * 路径点标记：拖进场景摆放位置，然后把它的引用按顺序加入
 * WalkerNPC 的 Waypoints 列表，即可实现多点巡逻。
 * 箭头仅作编辑器可视化，运行时隐藏。
 */
UCLASS()
class BEIMINGSIM_API AWalkerWaypoint : public AActor
{
	GENERATED_BODY()

public:
	AWalkerWaypoint();

	UPROPERTY(VisibleAnywhere, Category = "WalkerWaypoint")
	TObjectPtr<UArrowComponent> Arrow;
};
