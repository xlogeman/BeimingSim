#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneModelSwitcher.generated.h"

class AFlyingPawn;
class UDroneSwitchWidget;
class UBlueprint;

/**
 * 机型热切换器：运行时按 Tab 呼出侧滑菜单，点击机型即可整体替换
 * 当前多旋翼 Pawn 的外观（机身 + 桨叶），飞控/物理/API 完全不受影响。
 * 由 BeimingSimGameMode 在 BeginPlay 自动生成，无需在关卡里摆放。
 *
 * 设计说明：
 * - 外观装配完全在内容浏览器的无人机蓝图里做（BP_X450 / BP_X300 / BP_X100 …），
 *   编辑器里随时打开蓝图即可看到装配效果；
 * - 切换时读取所选蓝图 Class Default Object 的组件配置（机身网格、桨叶网格、
 *   相对变换、可见性），原样套到正在飞的 Pawn 上；
 * - 菜单自动扫描 /Game/UAV 下所有 AFlyingPawn 子类蓝图，新增机型零代码。
 */
UCLASS()
class BEIMINGSIM_API ADroneModelSwitcher : public AActor
{
    GENERATED_BODY()

public:
    ADroneModelSwitcher();

    virtual void BeginPlay() override;

    /** 自动扫描的根目录（递归），其中所有 AFlyingPawn 子类蓝图都会出现在菜单里 */
    UPROPERTY(EditAnywhere, Category = "DroneSwitch")
    FString ScanPath = TEXT("/Game/UAV");

    /** AirSim 自带的默认四旋翼蓝图（插件内容），菜单固定排在第一项 */
    UPROPERTY(EditAnywhere, Category = "DroneSwitch")
    FString DefaultPawnBlueprintPath = TEXT("/AirSim/Blueprints/BP_FlyingPawn.BP_FlyingPawn");

    /** AirSim 默认机型在菜单里的显示名 */
    UPROPERTY(EditAnywhere, Category = "DroneSwitch")
    FString DefaultModelName = TEXT("AirSim 默认");

private:
    void CreateWidgetInternal();
    void BindTabKey();
    void ToggleMenu();
    void DiscoverModels();
    AFlyingPawn* FindDronePawn() const;
    void ApplyAppearanceFromBlueprint(AFlyingPawn* Pawn, UBlueprint* Blueprint);

    UFUNCTION()
    void ApplyModel(int32 Index);

    UPROPERTY()
    UDroneSwitchWidget* MenuWidget = nullptr;

    /** 扫描结果：可用的无人机蓝图（第 0 项固定为 AirSim 默认机型） */
    UPROPERTY()
    TArray<TSoftObjectPtr<UBlueprint>> DiscoveredBlueprints;

    /** 与 DiscoveredBlueprints 一一对应的菜单显示名 */
    UPROPERTY()
    TArray<FString> DisplayNames;

    FTimerHandle TabBindRetryTimer;
};
