using UnrealBuildTool;

public class YarnSpinner : ModuleRules
{
    public YarnSpinner(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        
        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "Projects",
                "YSProtobuf",
                "CoreUObject",
                "AssetRegistry",
                "GameplayTasks",
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Json",
                "JsonUtilities",
            });
    }
}