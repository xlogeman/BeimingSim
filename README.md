# BeimingSim

基于 Unreal Engine 5.8 与 AirSim 的仿真项目。

## 环境要求

- Windows 10/11
- Unreal Engine 5.8
- Visual Studio 2022（含"使用 C++ 的游戏开发"工作负载）
- [Git](https://git-scm.com/) 与 [Git LFS](https://git-lfs.com/)

## 克隆仓库

本仓库使用 Git LFS 存储二进制资产，克隆前请先安装并初始化 LFS：

```powershell
git lfs install
git clone https://github.com/<你的用户名>/BeimingSim.git
```

## 打开与编译

1. 将 `Content/` 同步到项目根目录
2. 双击 `BeimingSim.uproject` 打开，首次会提示编译，选择"是"；
   或右键 `BeimingSim.uproject` → Generate Visual Studio project files，
   然后在 VS 中打开 `BeimingSim.sln`，选择 **Development Editor / Win64** 构建
3. 中间产物（`Binaries/`、`Intermediate/`、`DerivedDataCache/` 等）会自动生成，首次打开耗时较长属正常现象

## 目录结构

```
BeimingSim/
├── BeimingSim.uproject    # 项目入口
├── Source/                # 游戏 C++ 源码
│   ├── BeimingSim.Target.cs
│   ├── BeimingSimEditor.Target.cs
│   └── BeimingSim/        # 游戏 C++ 模块（按功能划分目录）
│       ├── BeimingSim.h/.cpp          # 模块入口
│       ├── Core/                      # 框架层：GameMode、PlayerController（F9 视角切换）
│       ├── Camera/                    # 自由飞行观察相机（FreeFlyCameraPawn）
│       ├── Drone/                     # 机型热切换：Tab 侧滑菜单 + 外观替换
│       └── Walker/                    # 行人巡逻 NPC：WalkerNPC / AIController / Waypoint
├── Config/                # 项目配置
├── Content/               # 游戏资产（不入库，另行同步）
└── Plugins/
    └── AirSim/            # 基于 Cosys-AirSim 修改的插件（源码已入库）
```

## 项目功能概览

- **自由飞行观察视角**：按 `F9` 在载具操控与自由相机（编辑器视口风格）之间切换，
  退出时自动恢复进入前的相机视角；WASD 平移、右键拖视角、Shift 加速、滚轮调速。
- **机型热切换**：按 `Tab` 呼出侧滑菜单，运行时整体替换无人机外观（机身 + 桨叶），
  飞控/物理/API 不受影响；自动扫描 `/Game/UAV` 下所有 `AFlyingPawn` 子类蓝图，新增机型零代码。
- **行人巡逻 NPC**：放置 `WalkerNPC` 即自动巡逻，支持两点往返 / 多点顺序 / 随机游走三种模式，
  有 NavMesh 时自动寻路绕障，无则退化为直线移动。
- **仿真核心**：Cosys-AirSim 多旋翼仿真，支持 settings.json 配置 `PhysicsParams` 物理参数、
  GPULidar 传感器、ArUco 标记识别等。

## 关于 AirSim 插件

`Plugins/AirSim/` 基于 [Cosys-AirSim](https://github.com/Cosys-Lab/Cosys-AirSim)（AirSim 的 Cosys-Lab 分支，v3.5.0）
并针对 UE 5.8 做了本地兼容补丁（如 `Annotation/AnnotationComponent.cpp` 中的 "BEIMING patch"），
**修改会直接提交在本仓库**，请勿直接用原版覆盖。

## 提交规范

提交信息请使用前缀，例如：

- `feat(camera): 新增 F9 自由飞行观察视角`
- `fix(camera): 修复自由视角卡死问题`
- `chore: 构建与工程配置调整`
