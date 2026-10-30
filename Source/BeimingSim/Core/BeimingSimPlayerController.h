#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AirSimCameraDirector.h"
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

    /** 自由视角空闲态输入模式：光标可见、仅右键按下可触发捕获（供 FreeFlyCameraPawn 在松开右键时调用） */
    void ApplyFreeFlyIdleInputMode();

protected:
    UPROPERTY(Transient)
    TObjectPtr<class AFreeFlyCameraPawn> FreeFlyPawn;

    UPROPERTY(Transient)
    TObjectPtr<APawn> SavedVehiclePawn;

    bool bFreeFlyActive = false;

    /** 进入自由视角前 CameraDirector 所处的视图模式（如 SpringArmChase 追拍），退出时恢复 */
    ECameraDirectorMode SavedDirectorMode = ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FPV;
    bool bSavedDirectorModeValid = false;

    /** 按记录的视图模式恢复 CameraDirector（调用对应的 inputEventXxxView） */
    void RestoreCameraDirectorView(class AAirSimCameraDirector* Director);
};
