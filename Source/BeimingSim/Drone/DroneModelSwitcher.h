#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneSwitchWidget.h"
#include "DroneModelSwitcher.generated.h"

class AFlyingPawn;
class UDroneSwitchWidget;
class UTexture;
class UTextureRenderTarget2D;

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
    FString DefaultModelName = TEXT("AirSim");

    /**
     * 各机型的介绍与参数（悬停菜单条目时弹出大图预览展示）。
     * 键为菜单显示名（如 "AirSim"、"X450"，即蓝图资产名去掉 BP_ 前缀）。
     * 未配置的机型显示兜底文案。
     */
    UPROPERTY(EditAnywhere, Category = "DroneSwitch")
    TMap<FString, FDroneModelInfo> ModelInfos;

private:
    void CreateWidgetInternal();
    void BindTabKey();
    void ToggleMenu();
    /** 同步初始选中态；无人机 Pawn 由 AirSim SimMode 延迟生成，未就绪时定时重试 */
    void SyncInitialSelection();
    void DiscoverModels();
    /** 为每个机型生成 3D 预览缩略图并更新菜单（BeginPlay 后延迟调用） */
    void GenerateThumbnails();
    /** 缩略图阶段 2：等一拍后逐个拍照并销毁临时实例 */
    void CapturePendingThumbnails();
    /** 把临时生成的机型实例拍成一张预览图（深色背景，只渲染该 Actor） */
    UTextureRenderTarget2D* CaptureThumbnail(AActor* Source) const;
    AFlyingPawn* FindDronePawn() const;
    /** 获取场景中当前无人机对应的菜单下标；无法识别时返回 -1（无选中项） */
    int32 GetCurrentModelIndex() const;
    /** 通过 AirSim SimMode 重置仿真：无人机瞬移回初始起飞坐标并重置飞控/物理 */
    void ResetSimulation();
    void ApplyAppearanceFromBlueprint(AFlyingPawn* Pawn, UClass* SourceClass);

    UFUNCTION()
    void ApplyModel(int32 Index);

    /**
     * 某机型条目行尾"设置"按钮：收起机型菜单并唤出居中的仿真配置面板，
     * 面板自动选中该机型对应的载具 settings 条目（按菜单显示名匹配）
     */
    UFUNCTION()
    void OpenSettingsPanel(int32 ModelIndex);

    /** 菜单收起时联动关闭 settings.json 配置面板（面板仅随菜单存在） */
    UFUNCTION()
    void OnMenuToggled(bool bOpen);

    UPROPERTY()
    UDroneSwitchWidget* MenuWidget = nullptr;

    /**
     * 扫描结果：可用的无人机蓝图"生成的类"（第 0 项固定为 AirSim 默认机型）。
     * 注意存的是 *_C 类而非 UBlueprint——UBlueprint 是编辑器专用类型，
     * 打包版蓝图资产里只有生成的类，直接存类才能在打包版正常加载与切换。
     */
    UPROPERTY()
    TArray<TSoftClassPtr<AFlyingPawn>> DiscoveredClasses;

    /** 与 DiscoveredClasses 一一对应的菜单显示名 */
    UPROPERTY()
    TArray<FString> DisplayNames;

    /** 与 DiscoveredClasses 一一对应的 3D 预览缩略图（可能为空） */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture>> ModelThumbnails;

    /** 缩略图阶段 1 生成的临时机型实例，阶段 2 拍照后销毁 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> TempInstances;

    FTimerHandle TabBindRetryTimer;
    FTimerHandle ThumbnailTimer;
    FTimerHandle SyncSelectionTimer;
    /** 初始选中态是否已同步完成（完成后不再重试/兜底） */
    bool bInitialSelectionSynced = false;
    int32 SyncSelectionAttempts = 0;
    /** 同步重试上限（0.5s 一次，共 10s） */
    static constexpr int32 MaxSyncAttempts = 20;
};
