// VAELEN - primary game module rules.
//
// This module is the bridge between the engine-agnostic kernel (VaelenCore)
// and Unreal Engine: lifecycle, log routing, and later the presentation layer.
//
// STATUS: VALIDATED (UE 5.6) - built by every UBT build since the first, 2026-09-07.
// BUILD: b0921 - Tools/engine_builds.txt; its code is what that build compiled (19.01).
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
			"VaelenCore",
			"VaelenSim",
			"VaelenPopulation",
			"VaelenSociety",
			"VaelenEconomy",
			"VaelenPolitics"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
