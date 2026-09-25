// VAELEN - VaelenWalk module rules. Phase 19 task 19.06.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
//
// THE MODULE THAT WALKS: actors only. The ground one walks on, cut by
// VaelenScene from the map leaf (ADR-0156) and uploaded as procedural mesh
// sections; the sky, lit from the world's hours (19.09); the walker, a
// character the controller of VaelenUI possesses; the game mode that names
// the three; and two console commands, Vaelen.Walk and Vaelen.Probe.
//
// It holds no world. Like VaelenUI it reads the views the subsystem hands
// on and never writes through the door: Tools/check_ui_fence.py reads this
// module with the UI's rules AND four more words it may not say - Mean,
// Watch, AdvanceDay, TakeSomebodyElse, Save, Load - because a walk that
// could turn the day or mean a verb on its own would be a second host.
//
// Dependencies: Unreal's, the two plugins (Enhanced Input for the
// controller's mapping context, ProceduralMeshComponent for the ground), the
// view leaves, the scene, the module that holds the world, and VaelenUI for
// the controller and the HUD the game mode names. VaelenUI does NOT depend
// on this module: the walk's controller lives there and moves a pawn, any
// pawn, which is why the game mode is here and not there (ROADMAP 19.06 AS
// BUILT says so out loud).
using UnrealBuildTool;

public class VaelenWalk : ModuleRules
{
	public VaelenWalk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"VaelenCore",
			"VaelenView",
			"VaelenScene",
			"VaelenGame",
			"VaelenUI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
