using UnrealBuildTool;

public class RoboDrive : ModuleRules
{
	public RoboDrive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "RoboSim", "RoboAutonomy" });

		// UMG hosts the operator view, which is written in Slate, the lower-level
		// UI layer UMG is built on. Json reads the scenario files it offers.
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "Json" });
	}
}
