// VAELEN - VaelenPresentation. Phase 13 tasks 13.07c and 13.08b.
//
// AELVOR drawn inside Unreal from the view of 13.01 and from nothing else.
//
// STATUS: VALIDATED - built by UnrealBuildTool on UE 5.6.1 with MSVC 14.44,
// run in the editor and LOOKED AT on 2026-09-14: AELVOR at 128 and at 256,
// twelve biomes, rivers, towns, roads, and 1467 people standing on 117 tiles of
// one region (6996 on 639 at 256), after the world they came from was
// destroyed. 100 fps on a T400 4GB with 72649 instances.
//
// And measured, not just seen: every figure the engine prints matches
// Tools/Atlas at the same seed and settings, INCLUDING the frame digest
// (abc5a5767c6cf9dd at 128/120) and the ground digest (8f7f4948f49b6e86) since
// ADR-0135 put the same systems in both. Two halves of one project, one world.
//
// WHAT THIS IS FOR, and why it is not simply the atlas actor again.
//
// AVaelenAtlasActor (in the Vaelen game module) already draws AELVOR, and does
// it well - relief, biomes, rivers, towns, roads. It reads the WorldMap, the
// region pool, the person pool, the trade routes: it reaches into the
// simulation for each of them, and 700 lines later the picture is right.
//
// That is the thing 13.01 was built to stop needing. This actor draws the same
// world and touches none of it:
//
//     {
//         FWorldRun Run(Seed);          // the world is born here
//         ...generate, run the centuries...
//         TakeMapView   (Run.Instance, Sources, Map);
//         TakeView      (Run.Instance, Sources, Frame);
//         TakeNetView   (Run.Instance, Sources, Net);
//         TakePeopleView(Run.Instance, Sources, People);
//     }                                  // AND IT DIES HERE
//
//     Draw(Map, Frame, Net, People);     // every instance placed after this
//
// The world is destroyed before a single slab is placed. Not "not used" -
// destroyed, its memory returned, its entity registry gone. If a view were
// secretly holding a handle back into it, this actor would draw garbage or
// crash, and that is precisely the test.
//
// 13.08b makes that test sharper than it was. The people are the first view in
// this project of things that are ENTITIES - a region is a number, a road is a
// pair of numbers, but a person is somebody the world had a handle to. Nine
// hundred figures standing on AELVOR after their world has been destroyed is
// the strongest form of 13.01's promise the project can show anyone.
//
// Tests/View/Test_Land.cpp proves the same thing headless in a suite called
// "the ground outlives the world". This is that suite with a viewport, which is
// the only part of it a person can actually look at.
//
// The rule of the layer holds twice over: this actor only reads, and after the
// brace it has nothing left to read from.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VaelenViewDrawer.h"

#include "VaelenViewActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;

UCLASS(Blueprintable, ClassGroup = (VAELEN), meta = (DisplayName = "VAELEN View"))
class VAELENPRESENTATION_API AVaelenViewActor : public AActor
{
	GENERATED_BODY()

public:
	AVaelenViewActor();

	/// Tiles a side. 128 is the size the kernel's own tests use; 256 is the one
	/// the gates use and the one the headless atlas reports.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World", meta = (ClampMin = "32", ClampMax = "512"))
	int32 WorldSize = 128;

	/// Years of pre-history: settlement, cultures, the first regions peopled.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 PreHistoryYears = 300;

	/// Years run afterwards with every system of Phases 04 to 07.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 Years = 120;

	/// The world's seed. The same seed always gives the same world - that is
	/// the promise the whole project is built on, and this actor is one more
	/// place to check it.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World")
	int64 Seed = 0x41454c564f52;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Scale", meta = (ClampMin = "10"))
	float TileSize = 100.0f;

	/// How tall the highest land stands, as a fraction of the map's width.
	/// See FVaelenDrawSettings::ReliefFraction for why this is a fraction and
	/// not centimetres per elevation unit.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Scale", meta = (ClampMin = "0", ClampMax = "1"))
	float ReliefFraction = 0.08f;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Scale", meta = (ClampMin = "1"))
	float SlabHeight = 40.0f;

	/// Draw the living, one figure each, on the ground of the region they live
	/// in. Only regions the simulation thinks about person by person have any:
	/// everywhere else has a population and no people, which is the truth about
	/// that region and not a gap. See Vaelen/View/Folk.h.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World")
	bool bDrawPeople = true;

	/// What the last build put on the ground, in one line, and what the view
	/// weighed. An empty world and a failed one look identical in a viewport,
	/// so this exists to tell them apart without opening the log.
	/// The material every tile, town and road is drawn with.
	///
	/// The drawer writes a colour per instance as three floats of per-instance
	/// custom data. A material that does not read PerInstanceCustomData draws
	/// the whole world in one flat colour - which is what the engine default
	/// does, and what you see when this is left unset. To see the biomes, the
	/// sea, the towns and the roads, point this at an Unlit material whose
	/// Emissive Color is MakeFloat3(PerInstanceCustomData[0], [1], [2]).
	///
	/// Left unset on purpose rather than defaulted: a material asset belongs to
	/// the project content, not to this module, and the world should still draw
	/// - colourless but correct - in a project that has none.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Look")
	TObjectPtr<UMaterialInterface> TileMaterial;

	UPROPERTY(VisibleAnywhere, Category = "AELVOR|Report")
	FString Report;

	/// Build AELVOR, take the view, DROP THE WORLD, then draw what is left.
	UFUNCTION(CallInEditor, Category = "AELVOR")
	void BuildFromView();

	/// Remove every instance without touching the settings.
	UFUNCTION(CallInEditor, Category = "AELVOR")
	void Clear();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Plate;

	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Ground;

	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Towns;

	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Roads;

	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Folk;
};
