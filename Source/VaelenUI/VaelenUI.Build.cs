// VAELEN - VaelenUI module rules. Phase 14 task 14.09.
//
// STATUS: UNVERIFIED - parsed against Tools/EngineShim under the RESTRICTED
// include set of 14.07 and fenced by Tools/check_ui_fence.py, and not yet
// compiled by UnrealBuildTool.
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
			// The views, the command surface, and the module that holds the world.
			"VaelenCore",
			"VaelenView",
			"VaelenPlayer",
			"VaelenGame"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
