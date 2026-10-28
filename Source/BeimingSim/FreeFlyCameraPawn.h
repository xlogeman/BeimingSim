#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FreeFlyCameraPawn.generated.h"

class UCapsuleComponent;
class UCameraComponent;
class UFloatingPawnMovement;

/**
 * 自由飞行观察相机（类似 UE 编辑器视口）：
 * - 按住鼠标右键拖动调整视角
 * - WASD 平移，Q/E（或 Space/C）升降
 * - Shift 加速，鼠标滚轮调节移动速度
 * - F9 与载具视角互相切换（由 ABeimingSimPlayerController 处理）
 * 无碰撞，可像编辑器相机一样穿过墙体。
 */
UCLASS()
class BEIMINGSIM_API AFreeFlyCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    AFreeFlyCameraPawn();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
    // ---- 输入回调（UE5.8 BindKey 不支持附加参数，按下/松开分别绑定） ----
    void OnForwardPress() { ForwardInput = 1.f; }
    void OnForwardRelease() { ForwardInput = 0.f; }
    void OnBackwardPress() { ForwardInput = -1.f; }
    void OnBackwardRelease() { ForwardInput = 0.f; }
    void OnStrafePress() { StrafeInput = -1.f; }
    void OnStrafeRelease() { StrafeInput = 0.f; }
    void OnStrafeRightPress() { StrafeInput = 1.f; }
    void OnStrafeRightRelease() { StrafeInput = 0.f; }
    void OnUpPress() { UpInput = 1.f; }
    void OnUpRelease() { UpInput = 0.f; }
    void OnDownPress() { UpInput = -1.f; }
    void OnDownRelease() { UpInput = 0.f; }
    void OnFastPress() { bFast = true; }
    void OnFastRelease() { bFast = false; }
    void OnLookPress() { SetLookActive(true); }
    void OnLookRelease() { SetLookActive(false); }
    void SetLookActive(bool bActive);
    void AdjustSpeed(float AxisValue);
    void RequestExit();

    // ---- 参数 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeFly")
    float LookSensitivity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeFly")
    float MoveSpeed = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeFly")
    float MinMoveSpeed = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeFly")
    float MaxMoveSpeed = 8000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeFly")
    float FastMultiplier = 4.f;

    // ---- 组件 ----
    UPROPERTY(VisibleAnywhere, Category = "FreeFly")
    TObjectPtr<UCapsuleComponent> Capsule;

    UPROPERTY(VisibleAnywhere, Category = "FreeFly")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, Category = "FreeFly")
    TObjectPtr<UFloatingPawnMovement> Movement;

    // ---- 运行时状态 ----
    float ForwardInput = 0.f;
    float StrafeInput = 0.f;
    float UpInput = 0.f;
    bool bFast = false;
    bool bLookActive = false;

    /** 当前视角（直接驱动相机，不依赖 ControlRotation 管线） */
    FRotator ViewRotation = FRotator::ZeroRotator;
    bool bViewInitialized = false;
    bool bSkipNextLookDelta = false;
};
