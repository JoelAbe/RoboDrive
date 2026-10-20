using UnrealBuildTool;

public class RoboDriveTarget : TargetRules
{
	public RoboDriveTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "RoboSim", "RoboApi", "RoboAutonomy", "RoboDrive" });
	}
}
