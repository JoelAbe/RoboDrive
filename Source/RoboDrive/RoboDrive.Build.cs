// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RoboDrive : ModuleRules
{
	public RoboDrive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "RoboSim", "RoboAutonomy" });

		// UMG for the operator view. Slate and SlateCore are the lower-level UI for UMG
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "Json" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
