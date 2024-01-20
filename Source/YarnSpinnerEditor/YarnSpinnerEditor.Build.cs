using System.IO;
using UnrealBuildTool;

public class YarnSpinnerEditor : ModuleRules
{
    public YarnSpinnerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;        
		CppStandard = CppStandardVersion.Latest;

        string yscPath;

        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            // The protobuf header files use '#if _MSC_VER', but this will
            // trigger -Wundef. Disable unidentified compiler directive warnings.
            bEnableUndefinedIdentifierWarnings = false;

            yscPath = ToolPath(Target, "ysc");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            yscPath = ToolPath(Target, "ysc.exe");
        }
        else
        {
            throw new System.PlatformNotSupportedException($"Platform {Target.Platform} is not currently supported.");
        }

        PublicDefinitions.Add($"YSC_PATH=TEXT(\"{yscPath}\")");

        DynamicallyLoadedModuleNames.AddRange(
            new[]
            {
                "AssetTools",
                "MainFrame",
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "ContentBrowser",
                "Core",
                "CoreUObject",
                "DesktopWidgets",
                "EditorStyle",
                "Engine",
                "InputCore",
                "Projects",
                "Slate",
                "SlateCore",
                "UnrealEd",

                "YarnSpinner",
                "YSProtobuf",
                "Json",
                "JsonUtilities",
                "Localization",
                "LocalizationCommandletExecution",
                "EditorSubsystem",
                "Kismet",
                "BlueprintGraph",
            });
    }

    public string ToolPath(ReadOnlyTargetRules Target, string Executable)
    {
        return Path.Combine(PluginDirectory, "Tools", Target.Platform.ToString(), Executable)
            .Replace(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
    }
}