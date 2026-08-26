using UnrealBuildTool;

public class SightWeaveRender : ModuleRules
{
	public SightWeaveRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SightWeaveRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"RHI",
			"RenderCore",
			"Renderer"
		});
	}
}
