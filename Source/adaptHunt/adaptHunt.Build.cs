using UnrealBuildTool;

public class adaptHunt : ModuleRules
{
    public adaptHunt(ReadOnlyTargetRules Target)
        : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "EnhancedInput",
                "InputCore",
                "AIModule",
                "NavigationSystem",
                "DeveloperSettings"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
            }
        );
    }
}
