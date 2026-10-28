#include "BeimingSimPlayerController.h"
#include "FreeFlyCameraPawn.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "AirSimCameraDirector.h"
#include "PIPCamera.h"

ABeimingSimPlayerController::ABeimingSimPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void ABeimingSimPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // F9：随时切换自由视角（Controller 级绑定，不依赖当前 possessed 的 Pawn）
    InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ThisClass::ToggleFreeFlyCamera);
}

void ABeimingSimPlayerController::ToggleFreeFlyCamera()
{
    if (!bFreeFlyActive)
    {
        // 进入自由视角：记录当前载具，把自由相机放到载具当前位置
        APawn* CurrentPawn = GetPawn();
        const FVector SpawnLoc = CurrentPawn ? CurrentPawn->GetActorLocation() : GetFocalLocation();
        const FRotator SpawnRot = CurrentPawn ? CurrentPawn->GetActorRotation() : GetControlRotation();

        if (!IsValid(FreeFlyPawn))
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.ObjectFlags |= RF_Transient;
            FreeFlyPawn = GetWorld()->SpawnActor<AFreeFlyCameraPawn>(
                AFreeFlyCameraPawn::StaticClass(), SpawnLoc, SpawnRot, Params);
        }
        if (!FreeFlyPawn)
        {
            UE_LOG(LogTemp, Warning, TEXT("FreeFlyCamera: 生成自由飞行相机失败"));
            return;
        }

        FreeFlyPawn->SetActorLocationAndRotation(SpawnLoc, SpawnRot);
        SavedVehiclePawn = CurrentPawn;

        Possess(FreeFlyPawn);
        SetViewTarget(FreeFlyPawn);
        SetControlRotation(SpawnRot);

        // 自由视角空闲态：光标可见、仅右键可触发捕获（见 ApplyFreeFlyIdleInputMode）。
        // 按住右键期间由 AFreeFlyCameraPawn::SetLookActive 切到 GameOnly 模式
        // （高精度原始输入 + 光标锁定，位移不受屏幕边界限制），松开右键切回空闲态。
        ApplyFreeFlyIdleInputMode();

        bFreeFlyActive = true;
        UE_LOG(LogTemp, Log, TEXT("FreeFlyCamera: 已切换到自由视角（F9 切回载具）"));
    }
    else
    {
        // 切回载具
        if (IsValid(SavedVehiclePawn))
        {
            Possess(SavedVehiclePawn);

            // 恢复机载 FPV 相机视角。直接 SetViewTarget(载具) 会落到 Pawn 上
            // 默认激活的相机（AirSim 的后方追拍 ChaseCamera），这里找到跟随该载具的
            // CameraDirector 并触发其 FPV 模式（内部会 SetViewTarget 到机载相机）。
            bool bRestoredFpv = false;
            for (TActorIterator<AAirSimCameraDirector> It(GetWorld()); It; ++It)
            {
                APIPCamera* FpvCam = It->getFpvCamera();
                if (FpvCam && FpvCam->IsAttachedTo(SavedVehiclePawn))
                {
                    It->inputEventFpvView();
                    bRestoredFpv = true;
                    break;
                }
            }
            if (!bRestoredFpv)
            {
                SetViewTarget(SavedVehiclePawn);
            }
        }
        SavedVehiclePawn = nullptr;
        bFreeFlyActive = false;

        // 恢复项目默认输入行为：无捕获、光标可见。
        // GameAndUI 的 ApplyInputMode 内部会 ReleaseMouseCapture 并把锁定行为
        // 设为 DoNotLock，从而解除自由视角遗留的高精度/锁定状态。
        SetInputMode(FInputModeGameAndUI());
        bShowMouseCursor = true;
        if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
        {
            GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
        }
        UE_LOG(LogTemp, Log, TEXT("FreeFlyCamera: 已切回载具视角"));
    }
}

void ABeimingSimPlayerController::ApplyFreeFlyIdleInputMode()
{
    // 自由视角空闲态的输入模式：
    // 1. GameAndUI：解除 GameOnly 遗留的高精度鼠标/光标锁定/永久捕获
    //    （ApplyInputMode 内部 ReleaseMouseCapture、锁定行为 DoNotLock），光标恢复显示；
    // 2. 捕获模式改为"仅右键按下时捕获"：不能保持 NoCapture——引擎在无捕获且
    //    捕获模式不匹配时不会把鼠标按键事件下发给游戏输入
    //    （见 FSceneViewport::OnMouseButtonDown 的 bTemporaryCapture/bProcessInputPrimary），
    //    右键按下就永远到不了 Pawn 的 BindKey 回调。
    // 按住右键后由 SetLookActive(true) 切到 GameOnly（高精度+锁定，位移无边界）。
    SetInputMode(FInputModeGameAndUI());
    bShowMouseCursor = true;
    if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
    {
        GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
    }
}
