// VAELEN - VaelenPresentation module rules. Phase 13 task 13.07c.
//
// STATUS: PROTOTYPE - compiled and linked by UnrealBuildTool on UE 5.6.1 with
// MSVC 14.44 on 2026-09-10 (14 modules, 167 actions, Result: Succeeded), and
// NOT YET RUN. Nothing here has been dropped in a level or looked at, so every
// claim about what it DRAWS remains unmeasured. What compiling proves is only
// that it is the shape of a program.
//
// It is still not covered by the headless CI, and cannot be: nothing here can
// be built without an engine.
//
// THIS MODULE IS NOT LIKE THE OTHER THIRTEEN.
//
// VaelenCore through VaelenView are engine-agnostic: pure C++20, compiled by
// UnrealBuildTool and by CMake both, listed in Tools/kernel_modules.txt so the
// purity checker fails if an Unreal header ever appears in one. That is what
// makes the headless CI meaningful.
//
// VaelenPresentation is the other side of that line. It includes Engine, it
// creates actors and components, and it cannot be built by CMake. It is
// deliberately NOT in kernel_modules.txt and deliberately has no
// CMakeLists.txt, because a module that needs an editor to exist has no
// business claiming a green light from a build that has none.
//
// What it may depend on is the interesting part. VaelenView, and nothing else
// of the kernel - not VaelenEconomy, not VaelenPopulation, not VaelenSim. The
// drawer in this module is handed three plain structs of numbers and has no
// name for a World in its whole vocabulary. That is 13.01's promise made
// structural on the engine side: the layering rule holds because the renderer
// has nothing in its hand to reach the simulation with.
//
// The actor is the exception that proves it, and it is quarantined for that
// reason: AVaelenViewActor builds a world in order to take a view OF it, so it
// needs the kernel modules a world is made of. Which is why building the world
// and drawing it are two files here, not one - see VaelenViewDrawer.h.
using UnrealBuildTool;

public class VaelenPresentation : ModuleRules
{
	public VaelenPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// The view, which is all the drawer needs.
			"VaelenView",
			// And the kernel a world is made of, which only the ACTOR needs, to
			// have something to take a view of. VaelenViewDrawer.cpp includes
			// none of these and the compiler is what enforces that.
			"VaelenCore",
			"VaelenSim",
			"VaelenPopulation",
			"VaelenSociety",
			"VaelenEconomy",
			"VaelenPolitics"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
