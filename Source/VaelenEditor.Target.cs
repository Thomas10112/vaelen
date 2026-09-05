// VAELEN - Editor target.
using UnrealBuildTool;

public class VaelenEditorTarget : TargetRules
{
	public VaelenEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		// C++20 is the engine default since 5.3 and is set per module in the
		// Build.cs files; a target-level override would need
		// bOverrideBuildEnvironment on installed engines.

		ExtraModuleNames.AddRange(new string[] { "VaelenCore", "VaelenSim", "Vaelen" });
	}
}
