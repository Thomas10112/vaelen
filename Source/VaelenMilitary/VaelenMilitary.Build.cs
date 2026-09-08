// VAELEN - VaelenMilitary module rules.
//
// STATUS: UNVERIFIED - engine-side, newer than the first Unreal build.
//
// VaelenMilitary is the seventh engine-agnostic kernel module (levies, armies, battle,
// siege and war over the Phase 07 politics and the Phase 06 economy).
// Same rules as VaelenSim: pure C++20, no Unreal header
// except VaelenMilitaryModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenMilitary : ModuleRules
{
	public VaelenMilitary(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_MILITARY_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_MILITARY_IMPORTS=1");
		}
	}
}
