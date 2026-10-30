#include "FreeFlyCameraPawn.h"
#include "Core/BeimingSimPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

AFreeFlyCameraPawn::AFreeFlyCameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    Capsule->InitCapsuleSize(30.f, 60.f);
    // 无碰撞：像编辑器视口相机一样自由穿行，避免飞行时卡在建筑/地面里
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetRootComponent(Capsule);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Capsule);
    Camera->FieldOfView = 90.f;

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->UpdatedComponent = Capsule;

    // 旋转由本类直接驱动相机（ViewRotation），不经过 ControlRotation 管线
}

void AFreeFlyCameraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    // 首次 Tick：以相机当前朝向（出生朝向）作为初始视角
    if (!bViewInitialized)
    {
        ViewRotation = Camera->GetComponentRotation();
        bViewInitialized = true;
    }

    // 右键按住时，用鼠标位移直接旋转相机
    if (bLookActive)
    {
        float DeltaX = 0.f, DeltaY = 0.f;

        UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
        const bool bViewportCaptured =
            ViewportClient && ViewportClient->Viewport && ViewportClient->Viewport->HasMouseCapture();

        if (bViewportCaptured)
        {
            // 视口已捕获鼠标：用引擎高精度位移。此时 Slate 已锁定光标，
            // 绝不能再 SetMouseLocation，否则两边抢光标会导致视角闪跳。
            PC->GetInputMouseDelta(DeltaX, DeltaY);
            // MouseY 键值是原始位移取反后的值，翻回"屏幕坐标"约定（向下为正）
            DeltaY = -DeltaY;
        }
        else
        {
            // 兜底：捕获未生效时（右键按下事件未触发 Slate 自动捕获），位移来自
            // 光标位置差。改用光标归中法：把光标拉回视口中心，用偏移量作为旋转量。
            // 正常情况下不会走到这里——捕获生效时引擎已锁定光标并锁定到位移累积。
            if (ViewportClient && ViewportClient->Viewport)
            {
                const FIntPoint Size = ViewportClient->Viewport->GetSizeXY();
                float MouseX = 0.f, MouseY = 0.f;
                if (PC->GetMousePosition(MouseX, MouseY))
                {
                    DeltaX = MouseX - Size.X * 0.5f;
                    DeltaY = MouseY - Size.Y * 0.5f;
                    PC->SetMouseLocation(Size.X / 2, Size.Y / 2);
                }
            }
        }

        // 按下右键的第一帧光标可能离中心很远，跳过避免视角跳变
        if (bSkipNextLookDelta)
        {
            DeltaX = DeltaY = 0.f;
            bSkipNextLookDelta = false;
        }

        ViewRotation.Yaw += DeltaX * LookSensitivity;
        // 鼠标上移（DY<0）抬头，下移（DY>0）低头；限制在 ±89.9° 防止翻转
        ViewRotation.Pitch = FMath::Clamp(ViewRotation.Pitch - DeltaY * LookSensitivity, -89.9f, 89.9f);
        ViewRotation.Roll = 0.f;
        Camera->SetWorldRotation(ViewRotation);
    }

    // WASD 沿相机实际朝向移动（与编辑器一致：W 朝视线方向飞，包括俯仰分量）
    Movement->MaxSpeed = MoveSpeed * (bFast ? FastMultiplier : 1.f);
    Movement->Acceleration = Movement->MaxSpeed * 8.f;
    Movement->Deceleration = Movement->MaxSpeed * 8.f;

    if (!FMath::IsNearlyZero(ForwardInput))
    {
        AddMovementInput(Camera->GetForwardVector(), ForwardInput);
    }
    if (!FMath::IsNearlyZero(StrafeInput))
    {
        AddMovementInput(Camera->GetRightVector(), StrafeInput);
    }
    if (!FMath::IsNearlyZero(UpInput))
    {
        AddMovementInput(FVector::UpVector, UpInput);
    }
}

void AFreeFlyCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 传统按键绑定（Enhanced Input 启用时依然兼容）

    // WASD 平移
    PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ThisClass::OnForwardPress);
    PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ThisClass::OnForwardRelease);
    PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ThisClass::OnBackwardPress);
    PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ThisClass::OnBackwardRelease);
    PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ThisClass::OnStrafePress);
    PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ThisClass::OnStrafeRelease);
    PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ThisClass::OnStrafeRightPress);
    PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ThisClass::OnStrafeRightRelease);

    // Q/E 升降，Space/C 辅助升降
    PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::OnUpPress);
    PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &ThisClass::OnUpRelease);
    PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ThisClass::OnDownPress);
    PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &ThisClass::OnDownRelease);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ThisClass::OnUpPress);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ThisClass::OnUpRelease);
    PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::OnDownPress);
    PlayerInputComponent->BindKey(EKeys::C, IE_Released, this, &ThisClass::OnDownRelease);

    // Shift 加速
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ThisClass::OnFastPress);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ThisClass::OnFastRelease);

    // 右键按住 = 调整视角
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::OnLookPress);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ThisClass::OnLookRelease);

    // 滚轮调速度
    PlayerInputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ThisClass::AdjustSpeed);

    // F9 退回载具视角
    PlayerInputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ThisClass::RequestExit);
}

void AFreeFlyCameraPawn::SetLookActive(bool bActive)
{
    bLookActive = bActive;

    if (bActive)
    {
        bSkipNextLookDelta = true;
    }

    // 编辑器风格：按住右键期间切换输入模式。
    // 按下：GameOnly（UseHighPrecisionMouseMovement + LockMouseToWidget +
    // CapturePermanently，见 FInputModeGameOnly::ApplyInputMode）——
    // 光标隐藏锁定，位移为原始输入，不受屏幕边界限制。
    // 这些操作写入 LocalPlayer 待处理 Slate 队列，由主循环每帧通过
    // ProcessExternalReply 应用，不依赖输入事件，同一帧内即可生效。
    // 松开：切回空闲态（光标可见、仅右键可触发捕获）。
    // 注意：不要在这里自己设置 bShowMouseCursor——按下时光标可见才能让
    // 引擎的点击捕获启用光标锁定（bCursorHiddenDueToCapture），光标的
    // 隐藏/恢复由引擎捕获机制与 GameOnly 锁定共同完成。
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    if (bActive)
    {
        PC->SetInputMode(FInputModeGameOnly());
    }
    else
    {
        if (ABeimingSimPlayerController* BeimingPC = Cast<ABeimingSimPlayerController>(PC))
        {
            BeimingPC->ApplyFreeFlyIdleInputMode();
        }
    }
}

void AFreeFlyCameraPawn::AdjustSpeed(float AxisValue)
{
    if (FMath::IsNearlyZero(AxisValue))
    {
        return;
    }
    const float Factor = AxisValue > 0.f ? 1.15f : 1.f / 1.15f;
    MoveSpeed = FMath::Clamp(MoveSpeed * Factor, MinMoveSpeed, MaxMoveSpeed);
}

void AFreeFlyCameraPawn::RequestExit()
{
    if (ABeimingSimPlayerController* PC = Cast<ABeimingSimPlayerController>(GetController()))
    {
        PC->ToggleFreeFlyCamera();
    }
}
