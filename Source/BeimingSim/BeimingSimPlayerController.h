#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BeimingSimPlayerController.generated.h"

/**
 * 显示鼠标光标的 PlayerController。
 * 打包运行时游戏窗口内默认隐藏系统光标（bShowMouseCursor=False），
 * 此处强制显示，使鼠标在窗口内可见且不被捕获锁定。
 */
UCLASS()
class BEIMINGSIM_API ABeimingSimPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ABeimingSimPlayerController();
};
