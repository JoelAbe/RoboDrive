using UnrealBuildTool;

public class RoboApi : ModuleRules
{
    public RoboApi(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Sockets",
                "Networking",
                "Json",
                "RoboSim",
                "DeveloperSettings"
            }
        );
    }
}