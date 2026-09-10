// VAELEN - VaelenPresentation. Phase 13 task 13.07c.
//
// AELVOR drawn inside Unreal from the view of 13.01 and from nothing else.
//
// STATUS: PROTOTYPE - compiled and linked by UnrealBuildTool on UE 5.6.1 with
// MSVC 14.44 on 2026-09-10 (14 modules, 167 actions, Result: Succeeded), and
// NOT YET RUN. Nothing here has been dropped in a level or looked at, so every
// claim about what it DRAWS remains unmeasured. What compiling proves is only
// that it is the shape of a program.
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
//         TakeMapView (Run.Instance, Sources, Map);
//         TakeView    (Run.Instance, Sources, Frame);
//         TakeNetView (Run.Instance, Sources, Net);
//     }                                  // AND IT DIES HERE
//
//     Draw(Map, Frame, Net);             // every instance placed after this
//
// The world is destroyed before a single slab is placed. Not "not used" -
// destroyed, its memory returned, its entity registry gone. If a view were
// secretly holding a handle back into it, this actor would draw garbage or
// crash, and that is precisely the test.
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

	UPROPERTY(EditAnywhere, Category = "AELVOR|Scale", meta = (ClampMin = "0"))
	float ReliefScale = 700.0f;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Scale", meta = (ClampMin = "1"))
	float SlabHeight = 40.0f;

	/// What the last build put on the ground, in one line, and what the view
	/// weighed. An empty world and a failed one look identical in a viewport,
	/// so this exists to tell them apart without opening the log.
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
};
