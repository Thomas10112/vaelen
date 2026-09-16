// VAELEN - VaelenRun module rules.
//
// STATUS: BUILT (engine-only) - compiled and linked by UnrealBuildTool on
// 2026-09-16 with 14.08's first build, on the machine that has the engine.
// Compiled and tested headless by CMake on every CI leg since 14.03.
//
// VaelenRun is the thirteenth engine-agnostic kernel module: one wiring of a
// played AELVOR (Run::Aelvor), the door its host's inputs come through
// (Run::Door), and the replay of what came through it. The Atlas wiring
// verbatim behind options that add and never reorder - ADR-0135 is what a
// second wiring cost, and this is the wiring a host uses instead of copying
// one.
//
// Same rules as VaelenSim: pure C++20, no Unreal header except
// VaelenRunModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenRun : ModuleRules
{
	public VaelenRun(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics", "VaelenMilitary", "VaelenInfrastructure", "VaelenColony", "VaelenPlayer", "VaelenGameplay", "VaelenView" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_RUN_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_RUN_IMPORTS=1");
		}
	}
}
