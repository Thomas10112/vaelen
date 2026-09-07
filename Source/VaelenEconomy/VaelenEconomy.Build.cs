// VAELEN - VaelenEconomy module rules.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
//
// VaelenEconomy is the fifth engine-agnostic kernel module (goods, stocks,
// production, markets, trade and wealth over the Phase 04 persons and the
// Phase 05 society). Same rules as VaelenSim: pure C++20, no Unreal header
// except VaelenEconomyModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenEconomy : ModuleRules
{
	public VaelenEconomy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_ECONOMY_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_ECONOMY_IMPORTS=1");
		}
	}
}
