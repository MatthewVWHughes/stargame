using UnrealBuildTool;

public class Stargame : ModuleRules
{
	public Stargame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"UMG"
		});
	}
}
