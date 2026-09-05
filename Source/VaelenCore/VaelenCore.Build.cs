// VAELEN - VaelenCore module rules.
//
// VaelenCore is the ENGINE-AGNOSTIC simulation kernel. Everything under
// Public/ and Private/ (except VaelenCoreModule.cpp) must compile without any
// Unreal header, so that the same sources can be built headless with CMake
// (see /CMakeLists.txt) for unit tests, determinism tests and stress tests.
//
// Rules enforced by Tools/check_kernel_purity.py (CTest entry Kernel.Purity):
//   R0 at most one *Module.cpp per module   R1 includes: "Vaelen/..." or std only
//   R2 no exceptions                        R3 no RTTI (dynamic_cast / typeid)
//   R4 no non-deterministic randomness      R5 #pragma once + STATUS line
//   R6 VALIDATED files carry no TODO/FIXME  R7 no bare long / long long
using UnrealBuildTool;

public class VaelenCore : ModuleRules
{
	public VaelenCore(ReadOnlyTargetRules Target) : base(Target)
	{
		// The kernel has no engine PCH; keep the translation units independent.
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;
		CppStandard = CppStandardVersion.Cpp20;

		// Only the module-registration file depends on Core.
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		// Headless build defines this to empty; UBT defines VAELENCORE_API itself.
		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");
	}
}
