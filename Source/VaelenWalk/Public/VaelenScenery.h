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
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S3 builds it.
// That /Engine/BasicShapes/Cube, Cone, Cylinder and Sphere are 100 cm shapes
// found by those names is the belief here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	/// What stands drawn, for the line Vaelen.Scene prints beside the layout's.
	struct FDrawn
	{
		int32 Houses = 0;
		int32 Figures = 0;
		int32 Company = 0;
		int32 Squares = 0;
		int32 RoadTiles = 0;
		int32 Pits = 0;
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
	bool bShapesFound = false;
};
