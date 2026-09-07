// VAELEN - VaelenPolitics module rules.
//
// STATUS: VALIDATED (UE 5.6, 2026-09-07) - compiled and run in the editor; not covered by the headless CI.
//
// VaelenPolitics is the sixth engine-agnostic kernel module (polities, law,
// authority, succession and diplomacy over the Phase 05 society and the Phase
// 06 economy). Same rules as VaelenSim: pure C++20, no Unreal header
// except VaelenPoliticsModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenPolitics : ModuleRules
{
	public VaelenPolitics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_POLITICS_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_POLITICS_IMPORTS=1");
		}
	}
}
