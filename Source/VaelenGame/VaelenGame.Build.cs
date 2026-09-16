// VAELEN - VaelenGame module rules. Phase 14 task 14.08.
//
// STATUS: BUILT - compiled and linked by UnrealBuildTool on 2026-09-16 (UE 5.6,
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
// surface. It does NOT depend on VaelenSim, VaelenPopulation or any other
// kernel module by name - VaelenRun's own public dependencies bring what the
// wiring needs, and naming them here would invite this module to reach past
// the Run into the world itself.
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
			"VaelenCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
