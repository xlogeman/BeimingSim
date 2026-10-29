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
├── Config/                # 项目配置
├── Content/               # 游戏资产（不入库，另行同步）
└── Plugins/
    └── AirSim/            # 基于 Microsoft AirSim 修改的插件（源码已入库）
```

## 关于 AirSim 插件

`Plugins/AirSim/` 在原版 [AirSim](https://github.com/microsoft/AirSim) 基础上做了定制修改，
**修改会直接提交在本仓库**，请勿直接用原版覆盖。

## 提交规范

提交信息请使用前缀，例如：

- `feat(camera): 新增 F9 自由飞行观察视角`
- `fix(camera): 修复自由视角卡死问题`
- `chore: 构建与工程配置调整`
