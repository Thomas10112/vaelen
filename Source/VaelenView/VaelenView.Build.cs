// VAELEN - VaelenView module rules.
//
// STATUS: VALIDATED (UE 5.6, 2026-09-10) - compiled and linked by UnrealBuildTool in 13.06;
// not run in the editor, and not covered by the headless CI.
//
// VaelenView is the twelfth engine-agnostic kernel module: what a renderer needs
// to be told about the world, taken once per frame and never written back.
//
// It is the boundary the layering rule of this project has always claimed -
// PRESENTATION reads WORLD STATE and does not touch it - made structural. The
// view is a plain snapshot of numbers with no pointer into the world, so a
// renderer holding one cannot reach the simulation even by accident.
//
// Same rules as VaelenSim: pure C++20, no Unreal header except
// VaelenViewModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenView : ModuleRules
{
	public VaelenView(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics", "VaelenMilitary", "VaelenInfrastructure", "VaelenColony", "VaelenPlayer", "VaelenGameplay" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_VIEW_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_VIEW_IMPORTS=1");
		}
	}
}
