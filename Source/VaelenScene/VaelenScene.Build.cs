// VAELEN - VaelenScene module rules. Phase 19 task 19.05, ADR-0156.
//
// STATUS: UNVERIFIED (engine) - a kernel module: built and tested headless by CMake on every CI leg,
// never yet by UnrealBuildTool (its first UBT build is sitting S2, 19.06).
//
// The scene is built in integers from the view leaves; the engine converts and uploads. Its public
// dependencies are Core and the view module, nothing of the simulation: what it reads is a leaf.
using UnrealBuildTool;

public class VaelenScene : ModuleRules
{
	public VaelenScene(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenView" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_SCENE_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_SCENE_IMPORTS=1");
		}
	}
}
