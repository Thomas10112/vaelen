// VAELEN - VaelenPopulation module rules.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
//
// VaelenPopulation is the third engine-agnostic kernel module (persons,
// families, demographics over the Phase 03 pre-history). Same rules as
// VaelenSim: pure C++20, no Unreal header except VaelenPopulationModule.cpp,
// compiled both by UBT and by CMake (Tools/kernel_modules.txt lists it for the
// purity checker).
using UnrealBuildTool;

public class VaelenPopulation : ModuleRules
{
	public VaelenPopulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_POPULATION_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_POPULATION_IMPORTS=1");
		}
	}
}
