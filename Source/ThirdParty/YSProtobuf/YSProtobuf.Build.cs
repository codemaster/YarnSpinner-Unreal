﻿using UnrealBuildTool;
using System.IO;

public class YSProtobuf : ModuleRules
{
	public YSProtobuf(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibPath(Target), "libprotobuf.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
			PublicAdditionalLibraries.Add(Path.Combine(LibPath(Target), "libprotobuf.a"));
        }

        PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
	}

	public string LibPath(ReadOnlyTargetRules Target)
	{
		return Path.Combine(ModuleDirectory, "lib", Target.Platform.ToString(), ConfigPath(Target.Configuration));
    }
	
	public string ConfigPath(UnrealTargetConfiguration Config)
	{
		return Config is UnrealTargetConfiguration.Debug or UnrealTargetConfiguration.DebugGame ? "Debug" : "Release";
	}
}
