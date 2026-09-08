// VAELEN - VaelenInfrastructure module rules.
//
// STATUS: UNVERIFIED - engine-side, newer than the first Unreal build.
//
// VaelenInfrastructure is the eighth engine-agnostic kernel module (what people
// build and keep: granaries, mills, smithies and walls over the Phase 06 goods,
// the Phase 07 polities and the Phase 08 war).
// Same rules as VaelenSim: pure C++20, no Unreal header
// except VaelenInfrastructureModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenInfrastructure : ModuleRules
{
	public VaelenInfrastructure(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics", "VaelenMilitary" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_INFRASTRUCTURE_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_INFRASTRUCTURE_IMPORTS=1");
		}
	}
}
