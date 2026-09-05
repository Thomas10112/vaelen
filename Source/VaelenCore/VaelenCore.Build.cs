// VAELEN - VaelenCore module rules.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
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

		PublicDefinitions.Add("VAELEN_UNREAL_BUILD=1");

		// Export macro (CoreTypes.h): the kernel never uses UBT's DLLEXPORT
		// token, which only exists after HAL/Platform.h. In modular (editor)
		// builds the module exports and its dependants import; in monolithic
		// builds the macro is empty.
		if (Target.LinkType == TargetLinkType.Modular)
		{
			PrivateDefinitions.Add("VAELEN_CORE_EXPORTS=1");
			PublicDefinitions.Add("VAELEN_CORE_IMPORTS=1");
		}

		// Kernel assertions mirror DO_CHECK: on except in Shipping and Test.
		// (UBT defines NDEBUG in every non-debug-CRT configuration, so the
		// NDEBUG convention must not be used here.)
		bool bKernelAsserts = Target.Configuration != UnrealTargetConfiguration.Shipping
			&& Target.Configuration != UnrealTargetConfiguration.Test;
		PublicDefinitions.Add("VAELEN_ASSERTS_ENABLED=" + (bKernelAsserts ? "1" : "0"));

		// Compile-time log floor: Trace everywhere except Shipping (Info).
		PublicDefinitions.Add("VAELEN_LOG_COMPILED_MIN_LEVEL="
			+ (Target.Configuration == UnrealTargetConfiguration.Shipping ? "2" : "0"));
	}
}
