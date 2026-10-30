// WalkerWaypoint.cpp

#include "WalkerWaypoint.h"

#include "Components/ArrowComponent.h"

AWalkerWaypoint::AWalkerWaypoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	SetRootComponent(Arrow);
	Arrow->ArrowSize = 2.0f;
	Arrow->SetHiddenInGame(true);
}
