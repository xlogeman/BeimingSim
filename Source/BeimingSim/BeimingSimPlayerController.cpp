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

        // 项目配置为 NoCapture（光标不被锁定），但引擎在无捕获时
        // 既不下发鼠标按键事件、也不累积 MouseX/Y 位移。
        // 自由视角期间临时启用"按住右键捕获"，退出时恢复原模式。
        if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
        {
            SavedMouseCaptureMode = GameViewport->GetMouseCaptureMode();
            GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
        }

        Possess(FreeFlyPawn);
        SetViewTarget(FreeFlyPawn);
        SetControlRotation(SpawnRot);
        bShowMouseCursor = true;
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
        bShowMouseCursor = true;
        bFreeFlyActive = false;

        // 恢复进入前的鼠标捕获模式（NoCapture）
        if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
        {
            GameViewport->SetMouseCaptureMode(SavedMouseCaptureMode);
        }
        UE_LOG(LogTemp, Log, TEXT("FreeFlyCamera: 已切回载具视角"));
    }
}
