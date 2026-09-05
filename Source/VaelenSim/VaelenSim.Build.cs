// VAELEN - VaelenSim module rules.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
//
// VaelenSim is the second engine-agnostic kernel module (entities, components,
// systems, clock, events, persistence). Same rules as VaelenCore: pure C++20,
// no Unreal header except VaelenSimModule.cpp, compiled both by UBT and by
// CMake (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenSim : ModuleRules
{
	public VaelenSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_SIM_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_SIM_IMPORTS=1");
		}
	}
}
