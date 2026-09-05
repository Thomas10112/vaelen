// VAELEN - primary game module rules.
//
// This module is the bridge between the engine-agnostic kernel (VaelenCore)
// and Unreal Engine: lifecycle, log routing, and later the presentation layer.
using UnrealBuildTool;

public class Vaelen : ModuleRules
{
	public Vaelen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"VaelenCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
