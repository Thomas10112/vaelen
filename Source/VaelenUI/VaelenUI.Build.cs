// VAELEN - VaelenUI module rules. Phase 14 task 14.09.
//
// STATUS: UNVERIFIED (engine) since 19.06 - its code has changed after the last build that
// compiled it (b0921, 15.10, 867a129): EnhancedInput and VaelenScene joined its dependencies
// for the walk's controller. Parsed, never compiled; sitting S2 builds it.
// BUILD: b0921 - Tools/engine_builds.txt
//
// UNTIL 19.06: BUILT - parsed against Tools/EngineShim under the RESTRICTED include
// set of 14.07, fenced by Tools/check_ui_fence.py, and compiled and linked by
// UnrealBuildTool on 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor).
// RUN on 2026-09-16: the eighty-three-day month of Tests/Run/Streams was played
// at this module's keys and read off its page ("NOT yet run" here until 19.01).
//
// THE MODULE THAT DRAWS AND PRESSES KEYS, AND HOLDS NOTHING.
//
// Every dependency below is either Unreal or a module of views. VaelenGame
// holds the world; this one asks it for a page and hands it back intents. It
// does not depend on VaelenRun, VaelenSim or anything a World is made of, and
// the fence checks what the dependency list cannot: under UnrealBuildTool a
// PUBLIC dependency's include paths are transitive, so VaelenView's own
// dependencies (VaelenSim, VaelenPlayer, ten more) are reachable from here
// whatever this file says. Tools/check_ui_fence.py is what actually stops
// them, and it says so in its own docstring.
//
// Not VaelenPresentation: that module draws the WORLD (13.07c's actor and
// drawer) and this one draws a PAGE. They share nothing but the view headers.
using UnrealBuildTool;

public class VaelenUI : ModuleRules
{
	public VaelenUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			// 19.06: the walk's keys, mapped at run time.
			"EnhancedInput",
			// The views, the command surface, and the module that holds the world.
			"VaelenCore",
			"VaelenView",
			"VaelenPlayer",
			"VaelenGame",
			// 19.06: the fence the walker is held to (Vaelen/Scene/Fence.h), a leaf.
			"VaelenScene"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
