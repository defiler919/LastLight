using UnrealBuildTool;

public class SightWeaveEditor : ModuleRules
{
	public SightWeaveEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Settings",
			"UnrealEd",
			"SightWeaveRuntime",
			"SightWeaveRender"
		});
	}
}
