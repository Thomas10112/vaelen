// VAELEN - VaelenSociety module rules.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
//
// VaelenSociety is the fourth engine-agnostic kernel module (organisations,
// standing, norms and bondage over the Phase 04 persons). Same rules as
// VaelenSim: pure C++20, no Unreal header except VaelenSocietyModule.cpp,
// compiled both by UBT and by CMake (Tools/kernel_modules.txt lists it for the
// purity checker).
using UnrealBuildTool;

public class VaelenSociety : ModuleRules
{
	public VaelenSociety(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_SOCIETY_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_SOCIETY_IMPORTS=1");
		}
	}
}
