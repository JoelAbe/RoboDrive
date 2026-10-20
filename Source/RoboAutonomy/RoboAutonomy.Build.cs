using UnrealBuildTool;

public class RoboAutonomy : ModuleRules
{
    public RoboAutonomy(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RoboSim"
            }
        );
    }
}