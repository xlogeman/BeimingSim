#include "BeimingSimGameMode.h"
#include "BeimingSimPlayerController.h"

ABeimingSimGameMode::ABeimingSimGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerControllerClass = ABeimingSimPlayerController::StaticClass();
}
