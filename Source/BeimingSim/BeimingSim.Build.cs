// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BeimingSim : ModuleRules
{
	public BeimingSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 本模块包含 AirSim 头文件（如 AirLib 的 FileSystem.hpp 使用 try/catch），
		// 打包的游戏目标默认禁用 C++ 异常，需显式开启（与 AirSim 模块一致）。
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AirSim", "UMG", "Json" });

		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "NavigationSystem", "GameplayTasks", "Slate", "SlateCore" });

		// Source 按功能分子目录（Core/Camera/Drone/Walker），
		// 代码中以 "Core/xxx.h" 这种模块根相对路径 include，需把模块根目录加入搜索路径。
		PrivateIncludePaths.Add(ModuleDirectory);
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
