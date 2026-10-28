#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BeimingSimPlayerController.generated.h"

/**
 * 显示鼠标光标的 PlayerController。
 * 打包运行时游戏窗口内默认隐藏系统光标（bShowMouseCursor=False），
 * 此处强制显示，使鼠标在窗口内可见且不被捕获锁定。
 *
 * 额外提供自由视角切换：按 F9 在 载具操控 / 自由飞行观察相机 之间切换
 * （自由飞行相机为 AFreeFlyCameraPawn，编辑器视口风格操作）。
 */
UCLASS()
class BEIMINGSIM_API ABeimingSimPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ABeimingSimPlayerController();

    virtual void SetupInputComponent() override;

    /** 在载具操控与自由飞行观察相机之间切换 */
    UFUNCTION(BlueprintCallable, Category = "BeimingSim|FreeFly")
    void ToggleFreeFlyCamera();

protected:
    UPROPERTY(Transient)
    TObjectPtr<class AFreeFlyCameraPawn> FreeFlyPawn;

    UPROPERTY(Transient)
    TObjectPtr<APawn> SavedVehiclePawn;

    /** 进入自由视角前的视口鼠标捕获模式，退出时恢复 */
    EMouseCaptureMode SavedMouseCaptureMode = EMouseCaptureMode::NoCapture;

    bool bFreeFlyActive = false;
};
