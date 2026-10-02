// VAELEN - VaelenWalk
// Phase 23 task 23.01: the materials, built by code and committed as assets.
//
// No material of this project is drawn by hand (ROADMAP section 29, decision
// 1): every one is the OUTPUT of this commandlet, so a lost asset is rebuilt
// from the repository and a change to the look is a change to a .cpp a
// reviewer can read. Run once on the machine that has the editor:
//
//   UnrealEditor-Cmd.exe <repo>\Vaelen.uproject -run=VaelenMaterials
//
// and it writes, under Content/Vaelen/Materials/, the three the walk looks
// for by name (VaelenLand, VaelenScenery): M_Ground, M_Water, M_Flat and
// (23.06) M_Leaf, the blade of grass. The
// code that USES them falls back to the engine's vertex-colour material and
// its default when they are not on the disk (Tools/check_cook.py counts a
// /Game path as optional for that reason), so a build without the assets
// still runs - greyer.
//
// EDITOR ONLY. The whole file is inside WITH_EDITOR: a material's graph is
// built and compiled by the editor's own library (UMaterialEditingLibrary,
// the MaterialEditor module), which a cooked game does not carry, and the
// Build.cs adds that module to the editor target alone. The headless parse
// (Tools/parse_engine_modules.py) sees nothing of it - the shim has no
// editor - so the owner's build is this file's FIRST compiler, and every
// name in it is a belief until then.
//
// STATUS: UNVERIFIED (engine) - 23.01 (2026-10-02), not parsed and not built:
// sitting S5 runs it.
#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
// clang-format off
#include "Commandlets/Commandlet.h"

#include "VaelenMaterialsCommandlet.generated.h"
// clang-format on

UCLASS()
class UVaelenMaterialsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVaelenMaterialsCommandlet();

	/// Builds and saves the four materials; prints one LogVaelenMaterials
	/// line per material (its expression count and the file written) and a
	/// last line with the count saved. Returns 0 when all four were saved.
	virtual int32 Main(const FString& Params) override;
};
#endif
