// VAELEN - Game target (runtime).
using UnrealBuildTool;
using System.Collections.Generic;

public class VaelenTarget : TargetRules
{
	public VaelenTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		CppStandard = CppStandardVersion.Cpp20;

		ExtraModuleNames.AddRange(new string[] { "VaelenCore", "Vaelen" });
	}
}
