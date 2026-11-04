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
并做了本地补丁（源码内搜索 "BEIMING patch"），**修改会直接提交在本仓库，请勿直接用原版覆盖**。补丁分两类：

- UE 5.8 兼容与功能增强：相机 `Enabled:false`、机型热切换重建 SimMode、同名 Actor 冲突加后缀、
  F10 天气菜单移除、`AnnotationComponent` 兼容、`SimHUD` 控制台变量优先级等
- **共轴双旋翼支持**（架构级，新增共轴机型无需再改代码）：
  - `RotorActuator/MultiRotorParams/MultiRotorPhysicsBody/RotorParams`：`PhysicsParams` 支持 `RotorCount:2` 共轴布局、
    倾转电机架（偶/奇桨反向倾转出俯仰/滚转力偶）、反扭矩固定沿堆叠轴（避免交叉耦合，见 `RotorActuator.hpp` 注释）
  - `MavLinkMultirotorApi`：HIL_ACTUATOR_CONTROLS 通道 2/3（raw 值）= 倾转舵机指令
  - `simple_flight` 固件同步适配（Mixer/Board/Firmware）

配对的 PX4 侧补丁与参数流程见下节，配套机型档案在 flylab 仓库
`ros2_ws/src/flylab_flight/configs/vehicles/`。

## 新增共轴双旋翼机型指南

控制链路（PX4 Custom 布局 → HIL 通道 0/1=上下桨油门、2/3=倾转舵机 → 共轴倾转物理）是架构级的，
**新增机型只改参数，不改任何代码**。但增益与每架机的物理特性绑定，须按公式起调、试飞微调。

### ① 纸面推导

| 量 | 公式 |
|---|---|
| 单桨最大推力 | `T_max = CT × 1.225 × (MaxRPM/60)² × PropDiameter⁴` [N] |
| 推重比 | `n = 2 × T_max / (Mass × 9.81)` |
| 悬停油门 | `MPC_THR_HOVER = 1/n`（Cosys-AirSim 推力与油门线性） |
| 反扭矩系数 | `KM = CP/CT × PropDiameter/(2π)` |
| 满舵倾转力矩 | `T_full = (Mass×9.81/2) × sin(MaxTiltAngle) × \|RotorZTop − RotorZBottom\|` |
| 舵机效力 | `TRQ = T_full / T_max`（与 KM 保持同量级，比例符合物理 roll:yaw 关系） |
| 最大水平加速度 | `≈ g × tan(MaxTiltAngle)`（决定 MPC 速度/加速度限制上界） |
| 滚转/俯仰转动惯量 | `I ≈ m/12 × (BodyBoxY²+BodyBoxZ²)`（X/Z 轴类推），用于增益缩放 |

### ② Windows 端

1. 新建 `Documents\AirSim\settings_<机型>.json`（复制 X050 版修改），`PhysicsParams` 各字段：

   | 字段 | 含义 |
   |---|---|
   | `Mass` | 起飞总重 kg（含电机桨） |
   | `RotorCount` | 固定 `2`（共轴） |
   | `RotorZTop` / `RotorZBottom` | 上/下桨相对质心高度，**NED 系必须为负值**（桨在质心上方）；两者之差决定倾转力矩 |
   | `MaxTiltAngle` | 倾转电机架最大倾角 rad（0 禁用；X050 用 0.25≈14°） |
   | `TiltFilterTc` | 倾转低通时间常数 s（X050 用 0.05） |
   | `BodyBoxX/Y/Z` | 机身包围盒（决定惯量） |
   | `CT / CP / MaxRPM / PropDiameter` | 桨参数，推力自动重算 |
   | `DragCoefficient` | 线性阻力系数 |

2. UE 中创建 `Content/UAV/BP_<机型>`（`AFlyingPawn` 子类）。
   **蓝图资产名必须与 settings 文件名一致**，否则 Tab 切换时静默回退到默认四旋翼参数。

### ③ WSL 端（flylab 仓库）

1. 机型档案 `configs/vehicles/<机型>.json`：存 ① 的全部推导值与 `_px4_params`，作为唯一事实来源
2. PX4 机型文件 `px4.1.15.2/ROMFS/px4fmu_common/init.d-posix/airframes/700XX_<机型名>`（复制 70005 修改）：

   | 参数 | 改成 |
   |---|---|
   | `CA_ROTOR0_KM` / `CA_ROTOR1_KM` | ±新 KM |
   | `CA_SV_CS0_TRQ_R` / `CA_SV_CS1_TRQ_P` | 新 TRQ |
   | `MPC_THR_HOVER` | 新悬停油门（改 Mass 必须联动，否则悬停漂移） |
   | `MPC_TILTMAX_AIR` | MaxTiltAngle（度） |
   | `MPC_XY_VEL_MAX / MPC_ACC_HOR_MAX` 等 | 按新最大水平加速度折算 |
   | `MC_*RATE_P / MC_*_P` 等 | 见 ④ 缩放公式 |

3. 注册三件套：`airframes/CMakeLists.txt`、`simulator_mavlink/CMakeLists.txt` 加
   `add_custom_target(none_<机型名>)`、然后 `make px4_sitl_default`
4. 启动 `bash scripts/run-px4.sh none_<机型名> --clean`

### ④ 增益缩放公式

角速度环开环增益 = `MC_*RATE_P × T_max / I`（与 TRQ 无关），故：

```
MC_ROLLRATE_P(新) ≈ 0.012 × (T_max / 22.96) × (0.013 / I)     # 22.96/0.013 为 X050 基准
```

- 姿态环 `MC_*_P`、位置/速度环增益跟随角速度环带宽等比缩放
- 微调口诀（详见 70005 机型文件 §5 注释与 X050 档案）：持续 ±1 满舵乒乓 → P 减半；
  响应迟钝 → P +50%；方向反 → 翻转 `CA_SV_CS0_TRQ_R`/`CS1_TRQ_P` 符号（免改代码）
- 悬停上下浮动 → 降 `MPC_Z_VEL_P_ACC/I_ACC` 并引入小 `D_ACC`

### ⑤ 避坑清单

- **PX4 机型文件名禁止含点号**（rcS 把带点文件当 `xxx.post` 忽略，如 `tw2.3` 要写成 `tw23`）
- **TRQ/KM 不能任意大**：PX4 弱权限轴阈值已补丁为 0.005（原 0.05 会把 KM=0.02/TRQ=0.013 的
  共轴三轴力矩整行清零，表现为俯仰/滚转/偏航全部无响应、HIL 通道 2/3 恒 0）；
  反过来取值过大（如 1.0）会造成各轴权限失配、分配饱和自旋
- **舵机指令 = 力矩 / TRQ 放大倍率很大**：增益偏高会 bang-bang 极限环（±1 乒乓），先降 P 再谈响应
- 共轴倾转架构权限天然低于常规四轴（X050 满倾角水平加速度 ≈4.4 m/s²），Position 模式手感温和是预期行为
- **平飞速度上不去先查 `MPC_VEL_MANUAL`**：POSCTL 手动满杆限速用的是它（默认 4 m/s），
  `MPC_XY_VEL_MAX` 只管自动航线任务；侧向/后退另有 `MPC_VEL_MAN_SIDE/BACK`（默认 -1=跟随 MANUAL）。
  提速 = 改 `MPC_VEL_MANUAL` + `MPC_TILTMAX_AIR`（≤ 仿真 `MaxTiltAngle`）+
  `CA_SV_CS0/1_TRQ`（∝ sin(MaxTiltAngle) 同比放大），增益无需重调（开环增益 = P×T_max/I，与倾角无关）；
  终端速度用二次阻力核算：`v = sqrt(m·g·tanθ / k)`，`k = 迎风面积 × DragCoefficient/2 × 1.225`
- `ControlIp` 变了先跑 `scripts/sync-airsim-ip.sh`，settings 改动要复制到 Windows `Documents\AirSim\` 后重启 UE

## 提交规范

提交信息请使用前缀，例如：

- `feat(camera): 新增 F9 自由飞行观察视角`
- `fix(camera): 修复自由视角卡死问题`
- `chore: 构建与工程配置调整`
