// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Darkwell : ModuleRules
{
	public Darkwell(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"AIModule",
			"NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"SightWeaveRuntime",
			"GeometryCore",
			"GeometryFramework",
			"Slate",
			"SlateCore",
			"UMG"
		});

		if (Target.Type != TargetType.Server)
		{
			PrivateDependencyModuleNames.Add("SightWeaveRender");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
