// VAELEN - the world in the viewport.
//
// Phase 15 belongs to the presentation layer; this is its first stone, and it
// is here early because a living world nobody can look at is a rumour. Drop
// one of these actors in a level, press Build AELVOR, and the kernel generates
// the world, runs its centuries headless inside the editor, and lays the
// result out as relief: one instanced cube per tile, coloured by biome and
// water, raised by elevation, with the towns of trade standing on it and the
// roads drawn between them.
//
// The rule of the layer holds: this actor only reads. It never writes world
// state, and nothing in the simulation knows it exists.
//
// STATUS: VALIDATED (UE 5.6, 2026-09-10) - compiled and linked by UnrealBuildTool in 13.06;
// not run in the editor, and not covered by the headless CI.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VaelenAtlasActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS(Blueprintable, ClassGroup = (VAELEN), meta = (DisplayName = "VAELEN Atlas"))
class VAELEN_API AVaelenAtlasActor : public AActor
{
	GENERATED_BODY()

public:
	AVaelenAtlasActor();

	/// Tiles a side. 128 is the size the kernel's own tests use.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World", meta = (ClampMin = "32", ClampMax = "512"))
	int32 WorldSize = 128;

	/// Years of pre-history: settlement, cultures, the first regions peopled.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 PreHistoryYears = 300;

	/// Years run afterwards with every system: lives, houses, councils, the
	/// economy, the polities.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 Years = 120;

	/// The world's seed. The same seed always gives the same world.
	UPROPERTY(EditAnywhere, Category = "AELVOR|World")
	int64 Seed = 0x41454c564f52;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Relief", meta = (ClampMin = "10"))
	float TileSize = 100.0f;

	/// Centimetres of relief per unit of elevation.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Relief", meta = (ClampMin = "0"))
	float ReliefScale = 700.0f;

	/// Thickness of a tile's slab; the sea is drawn flat at zero.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Relief", meta = (ClampMin = "1"))
	float SlabHeight = 40.0f;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Marks")
	bool bShowTowns = true;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Marks")
	bool bShowRoads = true;

	UPROPERTY(EditAnywhere, Category = "AELVOR|Marks")
	bool bShowSeats = true;

	UPROPERTY(EditAnywhere, Category = "AELVOR|World")
	bool bBuildOnBeginPlay = true;

	/// What the last build found, for the details panel and the log.
	UPROPERTY(VisibleAnywhere, Category = "AELVOR|Report")
	FString Report;

	/// Generate the world and lay it out. Safe to press again.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "AELVOR")
	void BuildAelvor();

	/// Remove every instance and mark left by the last build.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "AELVOR")
	void ClearAelvor();

	virtual void BeginPlay() override;

protected:
	UPROPERTY()
	TObjectPtr<USceneComponent> Plate;

	/// One component per colour of the plate; the palette is fixed in the .cpp.
	/// Not "Layers": AActor already has a member of that name, and UHT refuses
	/// a property that shadows one.
	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PaintLayers;

private:
	void EnsurePaintLayers();
};
