// VAELEN - VaelenGameplay module rules.
//
// STATUS: UNVERIFIED - engine-side, newer than the first Unreal build.
//
// VaelenGameplay is the eleventh engine-agnostic kernel module: what a person
// nobody is playing does, what they know, and what the world hears of them. It
// invents no verb - 10.05 already owns the seven, and Doings::Do takes a person
// index rather than "the player" - so this module decides intent and nothing
// else. It sits above VaelenPlayer, which owns the verbs, and VaelenColony.
// Same rules as VaelenSim: pure C++20, no Unreal header except
// VaelenGameplayModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenGameplay : ModuleRules
{
	public VaelenGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics", "VaelenMilitary", "VaelenInfrastructure", "VaelenColony", "VaelenPlayer" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_GAMEPLAY_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_GAMEPLAY_IMPORTS=1");
		}
	}
}
