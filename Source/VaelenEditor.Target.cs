// VAELEN - Editor target.
using UnrealBuildTool;
using System.Collections.Generic;

public class VaelenEditorTarget : TargetRules
{
	public VaelenEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		CppStandard = CppStandardVersion.Cpp20;

		ExtraModuleNames.AddRange(new string[] { "VaelenCore", "Vaelen" });
	}
}
