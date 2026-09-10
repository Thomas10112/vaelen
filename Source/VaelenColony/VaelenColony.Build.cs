// VAELEN - VaelenColony module rules.
//
// STATUS: VALIDATED (UE 5.6, 2026-09-10) - compiled and linked by UnrealBuildTool in 13.06;
// not run in the editor, and not covered by the headless CI.
//
// VaelenColony is the tenth engine-agnostic kernel module: the mining colony as
// a place, built out of the systems the phases below already own. It invents no
// mechanic of its own - it puts hands on a deposit of 02.07, credits the ore
// through the stocks of 06.01, and spends what the colony eats but does not
// grow. It sits above VaelenInfrastructure and below VaelenPlayer, because the
// player's start (10.02) stands on its ground.
// Same rules as VaelenSim: pure C++20, no Unreal header except
// VaelenColonyModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenColony : ModuleRules
{
	public VaelenColony(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics", "VaelenMilitary", "VaelenInfrastructure" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_COLONY_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_COLONY_IMPORTS=1");
		}
	}
}
