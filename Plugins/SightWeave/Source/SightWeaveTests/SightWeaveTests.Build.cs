using UnrealBuildTool;

public class SightWeaveTests : ModuleRules
{
	public SightWeaveTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"Projects",
			"RHI",
			"RenderCore",
			"TraceAnalysis",
			"TraceLog",
			"SightWeaveRuntime",
			"SightWeaveRender"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[]
			{
				"Advapi32.lib",
				"Psapi.lib",
				"PowrProf.lib"
			});
		}
	}
}
