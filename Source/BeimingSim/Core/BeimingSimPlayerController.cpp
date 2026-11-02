#include "BeimingSimPlayerController.h"
#include "Camera/FreeFlyCameraPawn.h"
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
        // 记录进入前 CameraDirector 的视图模式（如飞机后方的 SpringArmChase 追拍），
        // 按 F9 退出时原样恢复，而不是固定切到机身 FPV。
        bSavedDirectorModeValid = false;
        for (TActorIterator<AAirSimCameraDirector> It(GetWorld()); It; ++It)
        {
            SavedDirectorMode = It->getMode();
            bSavedDirectorModeValid = true;
            break;
        }

        // 出生点用"当前屏幕上实际渲染的相机"位置：追拍视角下即飞机后方的相机处，
        // 自由视角从相机所在位置无缝开始，而不是跳到机身里。
        FVector SpawnLoc = FVector::ZeroVector;
        FRotator SpawnRot = FRotator::ZeroRotator;
        GetPlayerViewPoint(SpawnLoc, SpawnRot);

        APawn* CurrentPawn = GetPawn();
        if (SpawnLoc.IsNearlyZero())
        {
            SpawnLoc = CurrentPawn ? CurrentPawn->GetActorLocation() : GetFocalLocation();
            SpawnRot = CurrentPawn ? CurrentPawn->GetActorRotation() : GetControlRotation();
        }

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

        // 空闲态：光标可见、仅右键按下时捕获；按住右键期间由 SetLookActive 切 GameOnly。
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

            // 恢复进入自由视角前的视图模式（如飞机后方的 SpringArmChase 追拍视角）。
            // inputEventXxxView 内部会 setMode + showToScreen（SetViewTarget 到对应相机）。
            // 仅当找不到 Director 或没有记录时，才退回载具默认相机。
            bool bRestored = false;
            for (TActorIterator<AAirSimCameraDirector> It(GetWorld()); It; ++It)
            {
                if (bSavedDirectorModeValid)
                {
                    RestoreCameraDirectorView(*It);
                    bRestored = true;
                }
                else
                {
                    APIPCamera* FpvCam = It->getFpvCamera();
                    if (FpvCam && FpvCam->IsAttachedTo(SavedVehiclePawn))
                    {
                        It->inputEventFpvView();
                        bRestored = true;
                    }
                }
                break;
            }
            if (!bRestored)
            {
                SetViewTarget(SavedVehiclePawn);
            }
        }
        SavedVehiclePawn = nullptr;
        bFreeFlyActive = false;
        bSavedDirectorModeValid = false;

        // GameAndUI 解除自由视角遗留的高精度/锁定状态，恢复光标可见。
        SetInputMode(FInputModeGameAndUI());
        bShowMouseCursor = true;
        if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
        {
            GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
        }
        UE_LOG(LogTemp, Log, TEXT("FreeFlyCamera: 已切回载具视角"));
    }
}

void ABeimingSimPlayerController::RestoreCameraDirectorView(AAirSimCameraDirector* Director)
{
    switch (SavedDirectorMode)
    {
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FPV:              Director->inputEventFpvView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_GROUND_OBSERVER:  Director->inputEventGroundView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FLY_WITH_ME:      Director->inputEventFlyWithView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_MANUAL:           Director->inputEventManualView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE:  Director->inputEventSpringArmChaseView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_BACKUP:           Director->inputEventBackupView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY:        Director->inputEventNoDisplayView(); break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FRONT:            Director->inputEventFrontView(); break;
    }
}

void ABeimingSimPlayerController::ApplyFreeFlyIdleInputMode()
{
    // GameAndUI 清除 GameOnly 遗留的高精度/锁定/捕获状态并显示光标；
    // 不能用 NoCapture——无捕获时引擎不把鼠标按下事件下发给游戏输入，
    // 右键就永远触发不了 Pawn 的 BindKey 回调，故改为"仅右键按下时捕获"。
    SetInputMode(FInputModeGameAndUI());
    bShowMouseCursor = true;
    if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
    {
        GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
    }
}
