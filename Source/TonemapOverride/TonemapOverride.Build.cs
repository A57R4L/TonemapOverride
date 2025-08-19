// Copyright 2024 Ossi Luoto

using UnrealBuildTool;
using System.IO;

public class TonemapOverride : ModuleRules
{
	public TonemapOverride(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(EngineDir, "Source/Runtime/Renderer/Private")
			}
			);		
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(EngineDir, "Source/Runtime/Renderer/Internal")
			}
		);		
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"RHI",
				"RenderCore",
				"Renderer"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"NetcodeUnitTest",
				"Renderer",
				"Slate",
				"SlateCore",
				
			}
			);

        // If using custom version of the Unreal Engine with modified SceneViewExtension and PostprocessCombineLUT
		PublicDefinitions.Add("ENGINE_VERSION_CUSTOM=false");

    }
}
