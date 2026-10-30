#include "BeimingSimGameMode.h"
#include "Core/BeimingSimPlayerController.h"
#include "Drone/DroneModelSwitcher.h"
#include "Engine/World.h"

ABeimingSimGameMode::ABeimingSimGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerControllerClass = ABeimingSimPlayerController::StaticClass();
}

void ABeimingSimGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 生成机型热切换器（含 Tab 侧滑菜单），无需在关卡中摆放
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    GetWorld()->SpawnActor<ADroneModelSwitcher>(
        ADroneModelSwitcher::StaticClass(), FTransform::Identity, Params);
}
