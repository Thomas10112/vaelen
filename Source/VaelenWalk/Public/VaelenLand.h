// VAELEN - VaelenWalk
// Phase 19 task 19.06: the ground one walks on, uploaded.
//
// VaelenScene cuts the ground in integers from the map leaf (Terrain.h); this
// actor turns those integers into floats and hands them to a procedural mesh
// component, one section per chunk. THE CHUNKS THE PLAYED REGION TOUCHES are
// uploaded at the full lattice (stride 1) WITH collision - the walker stands
// on them, and their digest is the one `VaelenAtlas --scene-terrain R`
// prints, so a sitting compares the engine's line with the Atlas's byte for
// byte; every other chunk is the far land, at stride Steps (one point per
// tile) and without collision. The colours are the tiles' with the day's
// snow and grass on them (19.09), repainted when the views are retaken.
//
// The frame is the scene's (Terrain.h): tile (x, y)'s centre at (x * C, y * C)
// in the actor's own space, and the actor stands at the level's origin - so a
// world point IS a map point, and RegionUnderGround / RegionAt read it as such.
//
// Nothing here ticks, and nothing here reaches the door: the actor reads the
// views the subsystem hands on and draws. Vaelen.Probe (VaelenWalkCommands)
// is the instrument that holds this upload to the builder: N line traces
// against HeightAt, and a bias that must show.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
// The winding of the triangles (counter-clockwise from above, Terrain.h) and
// GEngine->VertexColorMaterial are the beliefs S2 settles first.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Climate.h"

#include "VaelenLand.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

UCLASS()
class VAELENWALK_API AVaelenLand : public AActor
{
	GENERATED_BODY()

public:
	AVaelenLand();

	/// Uploads the whole ground: the chunks touching Region at the full
	/// lattice with collision, the rest at stride Steps without. False when
	/// the ground is empty. Keeps a copy of nothing but the section table:
	/// the ground itself is the subsystem's, held once (Scene()).
	bool Build(const Vaelen::Scene::Ground& G, const Vaelen::View::ClimateView& Climate, uint32 Region);

	/// The day's colours again, on the near chunks only (the far land keeps
	/// its biome colours: a tile of it is one vertex, and snow on it would be
	/// a fact the walker cannot reach). Nothing when Build never ran.
	void Repaint(const Vaelen::Scene::Ground& G, const Vaelen::View::ClimateView& Climate);

	/// The line the engine prints for the region it built: MeasureTerrain
	/// over the near chunks, in Terrain.h's words. 0 when Build never ran.
	uint32 TerrainLine(uint32 Size, uint64 Seed, char* Out, uint32 Bytes) const;

	/// N line traces from above at hashed points over the near chunks, each
	/// compared with the builder's HeightAt (Terrain.h): the widest gap in
	/// centimetres and how many traces hit nothing. BiasCm is added to every
	/// trace's answer, so `Vaelen.Probe 64 50` must print a gap of 5.0 - a
	/// probe that cannot fail is not a probe. Returns the traces made.
	int32 Probe(const Vaelen::Scene::Ground& G, int32 N, double BiasCm, double& OutMaxCm, int32& OutMisses) const;

	/// Which region the near chunks were built for, 0 before Build.
	uint32 BuiltFor() const { return Region_; }

	/// Bound to the subsystem's OnViewsTaken by Vaelen.Walk (AddUObject, so
	/// it unbinds with the actor): the day's snow on the near chunks and the
	/// sun of this level's sky at the life's hour. Reads and draws.
	void OnViewsTaken();

private:
	/// One section per chunk, in chunk order (CY * Across + CX): the near
	/// ones remember it so that Repaint touches them and only them.
	void Upload(const Vaelen::Scene::Ground& G, const Vaelen::View::ClimateView& Climate, uint32 CX, uint32 CY,
				bool Near);

	UProceduralMeshComponent* Mesh = nullptr;
	UMaterialInterface* Paint = nullptr;
	uint32 Region_ = 0;
	uint32 Across_ = 0;
	uint32 Down_ = 0;
	/// Per chunk: 1 when it is a near chunk (full lattice, collision).
	TArray<uint8> NearChunks;
	/// The near chunks' stats, measured as uploaded: what TerrainLine says.
	Vaelen::Scene::TerrainStats NearStats;
};
