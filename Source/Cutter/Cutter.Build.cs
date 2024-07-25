// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Cutter : ModuleRules
{
	public Cutter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "ProceduralMeshComponent" });
	}
}
