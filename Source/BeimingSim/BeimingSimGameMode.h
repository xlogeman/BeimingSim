#pragma once

#include "CoreMinimal.h"
#include "AirSimGameMode.h"
#include "BeimingSimGameMode.generated.h"

/**
 * 基于 AirSimGameMode 的项目 GameMode，仅替换 PlayerController 为显示光标的版本。
 * 其余行为（SimMode、SimHUD、载具生成）完全继承 AirSim。
 */
UCLASS()
class BEIMINGSIM_API ABeimingSimGameMode : public AAirSimGameMode
{
    GENERATED_BODY()

public:
    ABeimingSimGameMode(const FObjectInitializer& ObjectInitializer);
};
