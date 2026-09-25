// VAELEN - VaelenWalk
// Phase 19 task 19.06: the ground one walks on, uploaded. See VaelenLand.h.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#include "VaelenLand.h"

#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Scene/Sky.h"
#include "VaelenSky.h"
#include "VaelenWorldSubsystem.h"

AVaelenLand::AVaelenLand()
{
	// No Tick: the ground moves when the views are retaken, and then only its
	// colours (ADR-0138: nothing of the world moves on a frame).
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ground"));
	// Complex as simple: the walker's capsule meets the triangles themselves,
	// which is what Vaelen.Probe measures against the builder.
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->bUseAsyncCooking = false;
	RootComponent = Mesh;
	// The project's tile material when it exists on this disk (VaelenViewActor
	// takes it the same way); the engine's own vertex-colour material
	// otherwise, which is the asset-free default ADR-0157 asks for and a
	// BELIEF until S2 sees a coloured hill.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Tile(TEXT("/Game/M_VaelenTile.M_VaelenTile"));
	Paint = Tile.Succeeded() ? Tile.Object : nullptr;
}

bool AVaelenLand::Build(const Vaelen::Scene::Ground& G, const Vaelen::View::ClimateView& Climate, uint32 Region)
{
	if (G.Width == 0u || G.Height == 0u)
	{
		return false;
	}
	Region_ = Region;
	Across_ = Vaelen::Scene::ChunksAcross(G);
	Down_ = Vaelen::Scene::ChunksDown(G);
	NearChunks.Empty();
	NearStats = Vaelen::Scene::TerrainStats{};
	Mesh->ClearAllMeshSections();
	for (uint32 CY = 0; CY < Down_; ++CY)
	{
		for (uint32 CX = 0; CX < Across_; ++CX)
		{
			// Near: a chunk holding a tile of the played region - exactly the
			// chunks `VaelenAtlas --scene-terrain R` measures, so the two lines
			// can be compared byte for byte.
			bool Near = false;
			for (uint32 Y = CY * Vaelen::Scene::ChunkTiles;
				 !Near && Y < (CY + 1u) * Vaelen::Scene::ChunkTiles && Y < G.Height; ++Y)
			{
				for (uint32 X = CX * Vaelen::Scene::ChunkTiles;
					 X < (CX + 1u) * Vaelen::Scene::ChunkTiles && X < G.Width; ++X)
				{
					if (G.Region[Y * G.Width + X] == Region)
					{
						Near = true;
						break;
					}
				}
			}
			NearChunks.Add(Near ? 1u : 0u);
			Upload(G, Climate, CX, CY, Near);
		}
	}
	if (Paint != nullptr)
	{
		Mesh->SetMaterial(0, Paint);
	}
	else if (GEngine != nullptr)
	{
		Mesh->SetMaterial(0, GEngine->VertexColorMaterial);
	}
	return true;
}

void AVaelenLand::Upload(const Vaelen::Scene::Ground& G, const Vaelen::View::ClimateView& Climate, uint32 CX, uint32 CY,
						 bool Near)
{
	Vaelen::Scene::TerrainMesh Cut;
	const uint32 Stride = Near ? 1u : static_cast<uint32>(G.Scale.Steps);
	if (!Vaelen::Scene::BuildChunk(G, CX, CY, Stride, Cut))
	{
		return;
	}
	if (Near)
	{
		// Measured BEFORE the snow: the terrain line is the ground's, and the
		// same one whatever the day (Atlas.SceneTerrain128's rule). The snow
		// goes on the copy that is uploaded.
		Vaelen::Scene::MeasureTerrain(Cut, NearStats);
		Vaelen::Scene::ApplyClimate(G, Climate, Cut);
	}
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colours;
	TArray<FProcMeshTangent> Tangents;
	const double Tile = static_cast<double>(G.Scale.CmPerTile);
	for (const Vaelen::Scene::TerrainVertex& V : Cut.Vertices)
	{
		Vertices.Add(FVector(static_cast<double>(V.X), static_cast<double>(V.Y), static_cast<double>(V.Z)));
		Normals.Add(FVector(static_cast<double>(V.NX) / 32767.0, static_cast<double>(V.NY) / 32767.0,
							static_cast<double>(V.NZ) / 32767.0));
		UV0.Add(FVector2D(static_cast<double>(V.X) / Tile, static_cast<double>(V.Y) / Tile));
		Colours.Add(FLinearColor(static_cast<float>(V.R) / 255.0f, static_cast<float>(V.G) / 255.0f,
								 static_cast<float>(V.B) / 255.0f, 1.0f));
		Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
	}
	for (const Vaelen::uint32 Index : Cut.Triangles)
	{
		Triangles.Add(static_cast<int32>(Index));
	}
	const int32 Section = static_cast<int32>(CY * Across_ + CX);
	Mesh->CreateMeshSection_LinearColor(Section, Vertices, Triangles, Normals, UV0, Colours, Tangents, Near);
}

void AVaelenLand::Repaint(const Vaelen::Scene::Ground& G, const Vaelen::View::ClimateView& Climate)
{
	if (Region_ == 0u || NearChunks.Num() == 0)
	{
		return;
	}
	// The near chunks again, snow and all. A section is replaced whole: the
	// shim knows no UpdateMeshSection, and once a day is not a frame.
	const Vaelen::Scene::TerrainStats Keep = NearStats;
	for (uint32 CY = 0; CY < Down_; ++CY)
	{
		for (uint32 CX = 0; CX < Across_; ++CX)
		{
			if (NearChunks[static_cast<int32>(CY * Across_ + CX)] != 0u)
			{
				Mesh->ClearMeshSection(static_cast<int32>(CY * Across_ + CX));
				Upload(G, Climate, CX, CY, true);
			}
		}
	}
	NearStats = Keep; // the same ground, measured once
}

uint32 AVaelenLand::TerrainLine(uint32 Size, uint64 Seed, char* Out, uint32 Bytes) const
{
	if (Region_ == 0u)
	{
		return 0u;
	}
	return Vaelen::Scene::TerrainLine(Size, Seed, Region_, NearStats, Out, Bytes);
}

int32 AVaelenLand::Probe(const Vaelen::Scene::Ground& G, int32 N, double BiasCm, double& OutMaxCm,
						 int32& OutMisses) const
{
	OutMaxCm = 0.0;
	OutMisses = 0;
	if (Region_ == 0u || N <= 0 || GetWorld() == nullptr)
	{
		return 0;
	}
	// The near chunks' tiles, as a list to pick from by hash: the same N
	// points for the same world, so two sittings probe the same ground.
	TArray<uint32> Tiles;
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		if (G.Region[T] == Region_)
		{
			Tiles.Add(T);
		}
	}
	if (Tiles.Num() == 0)
	{
		return 0;
	}
	const int64 C = G.Scale.CmPerTile;
	int32 Made = 0;
	for (int32 i = 0; i < N; ++i)
	{
		const Vaelen::Hash64 H =
			Vaelen::HashCombine(Vaelen::HashUInt64(0x19060001ull), Vaelen::HashUInt64(static_cast<uint64>(i)));
		const uint32 T = Tiles[static_cast<int32>(H % static_cast<Vaelen::Hash64>(Tiles.Num()))];
		int64 X = 0, Y = 0;
		Vaelen::Scene::PointOfTile(G, T, X, Y);
		// Somewhere in the tile, off its centre: the relief between centres is
		// the invented part, and the part worth probing.
		X += static_cast<int64>((H >> 20) % static_cast<Vaelen::Hash64>(C)) - C / 2;
		Y += static_cast<int64>((H >> 40) % static_cast<Vaelen::Hash64>(C)) - C / 2;
		const double Built = static_cast<double>(Vaelen::Scene::HeightAt(G, X, Y));
		FHitResult Hit;
		const FVector From(static_cast<double>(X), static_cast<double>(Y), Built + 100000.0);
		const FVector To(static_cast<double>(X), static_cast<double>(Y), Built - 100000.0);
		++Made;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility) || !Hit.bBlockingHit)
		{
			++OutMisses;
			continue;
		}
		const double Traced = static_cast<double>(Hit.ImpactPoint.Z) + BiasCm;
		const double Gap = Traced > Built ? Traced - Built : Built - Traced;
		OutMaxCm = Gap > OutMaxCm ? Gap : OutMaxCm;
	}
	return Made;
}

void AVaelenLand::OnViewsTaken()
{
	UWorld* Level = GetWorld();
	if (Level == nullptr || Level->GetGameInstance() == nullptr)
	{
		return;
	}
	const UVaelenWorldSubsystem* World = Level->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
	if (World == nullptr || !World->Begun())
	{
		return;
	}
	Repaint(World->Scene(), World->Climate());
	// The sun over the played region's centroid row, at the life's hour; the
	// equator is +Y of a row above the map's middle.
	const Vaelen::Scene::Ground& G = World->Scene();
	const Vaelen::View::LifeView& Life = World->Life();
	uint32 Row = G.Height / 2u;
	for (const Vaelen::View::RegionView& R : World->World().Regions)
	{
		if (R.Index == (Life.Region != 0u ? Life.Region : 1u) && G.Width != 0u)
		{
			Row = R.CentroidTile / G.Width;
			break;
		}
	}
	for (TActorIterator<AVaelenSky> It(Level); It; ++It)
	{
		It->Aim(Vaelen::Scene::SunOf(Row, G.Height, World->Climate().Day, Life.Spent, Life.Awake), 2u * Row < G.Height);
		It->Recapture();
	}
}
