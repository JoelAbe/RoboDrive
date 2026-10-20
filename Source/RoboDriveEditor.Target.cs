using UnrealBuildTool;

public class RoboDriveEditorTarget : TargetRules
{
	public RoboDriveEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "RoboSim", "RoboApi", "RoboAutonomy", "RoboDrive" });
	}
}
