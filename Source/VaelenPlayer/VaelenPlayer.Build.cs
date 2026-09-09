// VAELEN - VaelenPlayer module rules.
//
// STATUS: UNVERIFIED - engine-side, newer than the first Unreal build.
//
// VaelenPlayer is the ninth engine-agnostic kernel module (the player as one
// simulated person, and their intent as commands into the simulation).
// Same rules as VaelenSim: pure C++20, no Unreal header
// except VaelenPlayerModule.cpp, compiled both by UBT and by CMake
// (Tools/kernel_modules.txt lists it for the purity checker).
using UnrealBuildTool;

public class VaelenPlayer : ModuleRules
{
	public VaelenPlayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "VaelenCore", "VaelenSim", "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics", "VaelenMilitary", "VaelenInfrastructure" });

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro owned by the kernel (see VaelenCore.Build.cs).
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_PLAYER_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_PLAYER_IMPORTS=1");
		}
	}
}
