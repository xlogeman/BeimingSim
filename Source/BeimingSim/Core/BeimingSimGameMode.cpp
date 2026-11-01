#include "BeimingSimGameMode.h"
#include "Core/BeimingSimPlayerController.h"
#include "Drone/DroneModelSwitcher.h"
#include "Config/SimConfigManager.h"
#include "Engine/World.h"

ABeimingSimGameMode::ABeimingSimGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerControllerClass = ABeimingSimPlayerController::StaticClass();
}

void ABeimingSimGameMode::BeginPlay()
{
    Super::BeginPlay();

    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;

    // 生成机型热切换器（含 Tab 侧滑菜单），无需在关卡中摆放
    GetWorld()->SpawnActor<ADroneModelSwitcher>(
        ADroneModelSwitcher::StaticClass(), FTransform::Identity, Params);

    // 生成仿真配置管理器（O 呼出配置面板，即时应用/写回 settings.json 并重建 SimMode）
    GetWorld()->SpawnActor<ASimConfigManager>(
        ASimConfigManager::StaticClass(), FTransform::Identity, Params);
}
