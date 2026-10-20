using UnrealBuildTool;

public class RoboSim : ModuleRules
{
    public RoboSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );

        // Json reads scenarios, which arrive as parsed JSON objects. Private,
        // because RoboSim's public headers only forward-declare FJsonObject.
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Json"
            }
        );

        /*PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );*/
    }
}