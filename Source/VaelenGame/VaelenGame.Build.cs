// VAELEN - VaelenGame module rules. Phase 14 task 14.08.
//
// STATUS: BUILT (engine) - compiled and linked by UnrealBuildTool on 2026-10-01 (b1001, d96a418), after
// the build of 3ee975a stopped on the two kernel modules this file now names (ROADMAP "Found on the
// owner's build of 3ee975a"). The world it holds was begun and played that day (sitting S4).
// BUILD: b1001 - Tools/engine_builds.txt
//
// UNTIL 2026-10-01: UNVERIFIED (engine) since 19.06 - VaelenScene joined its dependencies (the ground one
// walks on is cut once by the subsystem), parsed and never compiled.
//
// UNTIL 19.06: BUILT - compiled and linked by UnrealBuildTool on 2026-09-16 (UE 5.6,
// MSVC 19.51, Win64 Development Editor), after three defects nothing headless
// could see: UnrealHeaderTool's generated destructor on an incomplete pimpl,
// DEFINE_VTABLE_PTR_HELPER_CTOR instantiating the same one whatever the class
// declares, and RegionGraphCache exported whole while emitting nothing. NOT yet
// run: nothing here is VALIDATED until 14.10's two lines come out of a log.
//
// THE MODULE THAT HOLDS THE WORLD. VaelenUI (14.09) draws and presses keys and
// has no name for a World in its whole vocabulary; this is where the World
// lives, so that there is exactly one such place and it can be pointed at.
//
// What it depends on is the point: VaelenRun, which is the wiring of AELVOR
// (14.03), and VaelenView and VaelenPlayer for the views and the command
// surface. It did NOT depend on VaelenSim, VaelenPopulation or any other
// kernel module by name - VaelenRun's own public dependencies bring what the
// wiring needs, and naming them here would invite this module to reach past
// the Run into the world itself. Two are named since 2026-09-30 all the same:
// 19.04's ClimateLine() calls MeasureNeeds (VaelenPopulation) and
// MeasureWinters (VaelenEconomy) itself, and a modular MSVC build imports a
// symbol only from a module the .Build.cs names - the owner's build of
// 3ee975a stopped on those two (LNK2019), where clang's parse and the
// monolithic headless link had both passed. The fence stands where it stood:
// this module composes lines from what the Run holds, and does not tick it.
//
// Tools/check_ui_fence.py reads this module's PUBLIC directory with the UI's,
// and deliberately not its Private: the header may not name a World, and the
// .cpp must.
using UnrealBuildTool;

public class VaelenGame : ModuleRules
{
	public VaelenGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// The views a host hands on, and the command surface a key becomes.
			"VaelenView",
			"VaelenPlayer",
			// The wiring of AELVOR: one world, one door, one graph cache.
			"VaelenRun",
			// 19.06: the ground one walks on, cut once from the map leaf (ADR-0156).
			"VaelenScene",
			// 2026-09-30: what ClimateLine() measures directly (see the header comment).
			"VaelenPopulation",
			"VaelenEconomy",
			"VaelenCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
