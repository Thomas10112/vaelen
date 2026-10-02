// VAELEN - VaelenWalk
// Phase 19 task 19.11: the world drawn is the world built.
//
// What the scene invents (Vaelen/Scene/Layout.h) is laid out once per
// retaking by the subsystem, and this actor draws THAT layout and nothing
// else, as instanced meshes of the engine's own basic shapes - no asset of
// ours (ADR-0157): a house is a cube with a cone on it, a figure a cylinder
// with a sphere, the company's figures a size larger; a square a flat slab, a
// road a slab per tile, a pit a wide low cylinder. The sea is a plane at 0
// and every lake its invented surface (Terrain.h), as procedural-mesh
// sections without collision. Everything rebuilds on OnViewsTaken, never on
// a frame, and the instance counts equal the layout line's - Vaelen.Scene
// prints both, and Session.P19S3 holds them equal.
//
// STATUS: UNVERIFIED (engine) since 23.02 (2026-10-02): the wood (DrawFlora,
// four instanced components, per-instance custom data) is parsed against
// Tools/EngineShim and not yet built - sitting S5 builds it. Build b1001
// (d96a418) compiled the 19.11 scenery as it stood.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vaelen/Scene/Flora.h"
#include "Vaelen/Scene/Layout.h"
#include "Vaelen/Scene/Terrain.h"

#include "VaelenScenery.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMesh;

UCLASS()
class VAELENWALK_API AVaelenScenery : public AActor
{
	GENERATED_BODY()

public:
	AVaelenScenery();

	/// Draws the layout over the ground: every instance cleared and placed
	/// again. Returns false when a shape could not be found on this engine.
	bool Draw(const Vaelen::Scene::Ground& G, const Vaelen::Scene::SceneLayout& L);

	/// The water once: the sea plane over the whole map at 0, and a quad per
	/// lake tile at its surface. The map does not move, so neither does this.
	void DrawWater(const Vaelen::Scene::Ground& G);

	/// 23.02: the wood drawn over the ground, from what Flora.h planted - a
	/// trunk (cylinder) and a crown (a cone for a conifer, a sphere for the
	/// rest; a shrub is a low sphere alone) per tree, no collision, tinted
	/// per instance through custom data (three floats, the crown's colour
	/// by kind) for the material 23.01 brings; the engine's default until
	/// then. Redrawn only when the wood's digest moved.
	bool DrawFlora(const Vaelen::Scene::Flora& Wood);
	/// What stands drawn, for the line Vaelen.Scene prints beside the layout's.
	struct FDrawn
	{
		int32 Houses = 0;
		int32 Figures = 0;
		int32 Company = 0;
		int32 Squares = 0;
		int32 RoadTiles = 0;
		int32 Pits = 0;
		int32 Trees = 0;
	};
	FDrawn Drawn() const;

	/// Bound to the subsystem's OnViewsTaken by Vaelen.Walk: the layout of the day.
	void OnViewsTaken();

private:
	UInstancedStaticMeshComponent* Houses = nullptr;
	UInstancedStaticMeshComponent* Roofs = nullptr;
	UInstancedStaticMeshComponent* Figures = nullptr;
	UInstancedStaticMeshComponent* Heads = nullptr;
	UInstancedStaticMeshComponent* Company = nullptr;
	UInstancedStaticMeshComponent* CompanyHeads = nullptr;
	UInstancedStaticMeshComponent* Squares = nullptr;
	UInstancedStaticMeshComponent* Roads = nullptr;
	UInstancedStaticMeshComponent* Pits = nullptr;
	UProceduralMeshComponent* Water = nullptr;
	UInstancedStaticMeshComponent* Trunks = nullptr;
	UInstancedStaticMeshComponent* Conifers = nullptr;
	UInstancedStaticMeshComponent* Crowns = nullptr;
	UInstancedStaticMeshComponent* Shrubs = nullptr;
	bool bShapesFound = false;
	Vaelen::Hash64 WoodDrawn = 0; ///< the digest of the wood last drawn
};
