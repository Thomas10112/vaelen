// VAELEN - VaelenWalk
// Phase 19 task 19.11: the world drawn is the world built. See VaelenScenery.h.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25 (reviewed and corrected 2026-09-26, 19.11b), not yet built by
// UnrealBuildTool nor run: sitting S3 builds it.
#include "VaelenScenery.h"

#include "Materials/MaterialInterface.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Vaelen/Scene/Sky.h"
#include "Vaelen/Core/Hash.h"
#include "Materials/Material.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "VaelenWorldSubsystem.h"

namespace
{
	/// A basic shape as an instanced component, no collision unless asked:
	/// figures and slabs are walked through, houses are not.
	/// The parent is handed in: AActor::RootComponent is protected in the
	/// engine, and a free function reading it through an AActor* is MSVC C2248
	/// (found by the review of 2026-09-26; the shim accepted it until then).
	UInstancedStaticMeshComponent* Shapes(AActor* Owner, USceneComponent* Under, const TCHAR* Name, UStaticMesh* Mesh,
										  bool bCollide, bool& bFound)
	{
		UInstancedStaticMeshComponent* Made = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Made->SetupAttachment(Under);
		if (Mesh != nullptr)
		{
			Made->SetStaticMesh(Mesh);
		}
		else
		{
			bFound = false;
		}
		Made->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		return Made;
	}

	FTransform At(double X, double Y, double Z, double SX, double SY, double SZ)
	{
		return FTransform(FRotator::ZeroRotator, FVector(X, Y, Z), FVector(SX, SY, SZ));
	}

	/// 23.05: a gabled roof from the cube we have - the cube turned 45 degrees
	/// about the ridge (roll for a ridge along X, pitch for one along Y),
	/// scaled long along it and square across, so its lower edges sink into
	/// the walls and its top edge is the ridge.
	FTransform Gable(double X, double Y, double Z, bool bAlongY, double Length, double Across)
	{
		return FTransform(bAlongY ? FRotator(45.0, 0.0, 0.0) : FRotator(0.0, 0.0, 45.0), FVector(X, Y, Z),
						  bAlongY ? FVector(Across, Length, Across) : FVector(Length, Across, Across));
	}

	/// The walls' plaster by culture (six tints, the culture's index modulo
	/// six), a coarse house's grey, and the roof's thatch.
	struct FWallTint
	{
		float R, G, B;
	};
	constexpr FWallTint WallOf(Vaelen::uint32 Culture)
	{
		constexpr FWallTint Plaster[6] = {{0.80f, 0.74f, 0.62f}, {0.72f, 0.60f, 0.46f}, {0.62f, 0.66f, 0.60f},
										  {0.78f, 0.66f, 0.52f}, {0.56f, 0.52f, 0.48f}, {0.70f, 0.70f, 0.66f}};
		return Culture == 0u ? FWallTint{0.60f, 0.58f, 0.54f} : Plaster[Culture % 6u];
	}
	constexpr FWallTint Thatch = {0.34f, 0.22f, 0.12f};
	constexpr FWallTint Stone = {0.46f, 0.45f, 0.43f};

	void Painted(UInstancedStaticMeshComponent* Into, const FTransform& Where, const FWallTint& Tint)
	{
		const int32 Index = Into->AddInstance(Where);
		Into->SetCustomDataValue(Index, 0, Tint.R);
		Into->SetCustomDataValue(Index, 1, Tint.G);
		Into->SetCustomDataValue(Index, 2, Tint.B);
	}
} // namespace

AVaelenScenery::AVaelenScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Water"));
	Water->bUseComplexAsSimpleCollision = true;
	Water->bUseAsyncCooking = false;
	RootComponent = Water;

	// The engine's own shapes, 100 cm each, found by name - the belief S3 settles.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	bShapesFound = Cube.Succeeded() && Cone.Succeeded() && Cylinder.Succeeded() && Sphere.Succeeded();
	bool bFound = bShapesFound;
	Houses = Shapes(this, Water, TEXT("Houses"), Cube.Object, true, bFound);
	Roofs = Shapes(this, Water, TEXT("Roofs"), Cone.Object, true, bFound);
	Figures = Shapes(this, Water, TEXT("Figures"), Cylinder.Object, false, bFound);
	Heads = Shapes(this, Water, TEXT("Heads"), Sphere.Object, false, bFound);
	Company = Shapes(this, Water, TEXT("Company"), Cylinder.Object, false, bFound);
	CompanyHeads = Shapes(this, Water, TEXT("CompanyHeads"), Sphere.Object, false, bFound);
	Squares = Shapes(this, Water, TEXT("Squares"), Cube.Object, false, bFound);
	Roads = Shapes(this, Water, TEXT("Roads"), Cube.Object, false, bFound);
	Pits = Shapes(this, Water, TEXT("Pits"), Cylinder.Object, false, bFound);
	Wells = Shapes(this, Water, TEXT("Wells"), Cylinder.Object, true, bFound);
	// 23.06: the grass - the engine's plane, a 100 cm square, stood up and
	// scaled to a blade; the shapes are found or not as one, and the grass
	// is not among them: a disk without the plane has the rest and no grass.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	bool bPlane = true;
	Blades = Shapes(this, Water, TEXT("Blades"), Plane.Object, false, bPlane);
	Blades->SetCastShadow(false);
	// 23.02: the wood. Three floats of custom data per instance: the tint
	// the material of 23.01 reads (PerInstanceCustomData 0-2); nothing reads
	// them until it lands, and they cost nothing to set.
	Trunks = Shapes(this, Water, TEXT("Trunks"), Cylinder.Object, false, bFound);
	Conifers = Shapes(this, Water, TEXT("Conifers"), Cone.Object, false, bFound);
	Crowns = Shapes(this, Water, TEXT("Crowns"), Sphere.Object, false, bFound);
	Shrubs = Shapes(this, Water, TEXT("Shrubs"), Sphere.Object, false, bFound);
	// 23.01: the materials the commandlet writes, when they are on this disk.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterial(
		TEXT("/Game/Vaelen/Materials/M_Water.M_Water"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlatMaterial(
		TEXT("/Game/Vaelen/Materials/M_Flat.M_Flat"));
	WaterPaint = WaterMaterial.Succeeded() ? WaterMaterial.Object : nullptr;
	FlatPaint = FlatMaterial.Succeeded() ? FlatMaterial.Object : nullptr;
	// 23.05: the houses, their roofs, the squares and the wells are tinted
	// through the same custom data (the walls by the family's culture).
	for (UInstancedStaticMeshComponent* Each : {Trunks, Conifers, Crowns, Shrubs, Houses, Roofs, Squares, Wells, Pits})
	{
		Each->NumCustomDataFloats = 3;
		if (FlatPaint != nullptr)
		{
			Each->SetMaterial(0, FlatPaint);
		}
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafMaterial(
		TEXT("/Game/Vaelen/Materials/M_Leaf.M_Leaf"));
	Blades->NumCustomDataFloats = 3;
	if (LeafMaterial.Succeeded())
	{
		Blades->SetMaterial(0, LeafMaterial.Object);
	}
	bShapesFound = bFound;
}

namespace
{
	/// The crown's tint by kind, and the trunk's: what the custom data carries.
	struct FTint
	{
		float R, G, B;
	};
	constexpr FTint TintOf(Vaelen::uint8 Kind)
	{
		switch (Kind)
		{
		case Vaelen::Scene::TreeKind::Conifer:
			return {0.10f, 0.28f, 0.14f};
		case Vaelen::Scene::TreeKind::Broadleaf:
			return {0.22f, 0.44f, 0.16f};
		case Vaelen::Scene::TreeKind::Palm:
			return {0.34f, 0.52f, 0.18f};
		default:
			return {0.40f, 0.42f, 0.20f};
		}
	}
	constexpr FTint Bark = {0.30f, 0.22f, 0.14f};

	void Tinted(UInstancedStaticMeshComponent* Into, const FTransform& Where, const FTint& Tint)
	{
		const int32 Index = Into->AddInstance(Where);
		Into->SetCustomDataValue(Index, 0, Tint.R);
		Into->SetCustomDataValue(Index, 1, Tint.G);
		Into->SetCustomDataValue(Index, 2, Tint.B);
	}
} // namespace

bool AVaelenScenery::DrawFlora(const Vaelen::Scene::Flora& Wood)
{
	const Vaelen::Hash64 Digest = Vaelen::Scene::MeasureFlora(Wood).Digest;
	if (Digest == WoodDrawn)
	{
		return bShapesFound;
	}
	for (UInstancedStaticMeshComponent* Each : {Trunks, Conifers, Crowns, Shrubs})
	{
		Each->ClearInstances();
	}
	WoodDrawn = Digest;
	if (!bShapesFound)
	{
		return false;
	}
	for (const Vaelen::Scene::Tree& T : Wood.Trees)
	{
		const double X = T.X, Y = T.Y, Z = T.Z;
		const double H = T.HeightCm;
		const FTint Crown = TintOf(T.Kind);
		switch (T.Kind)
		{
		case Vaelen::Scene::TreeKind::Conifer:
		{
			// A trunk of a third, a cone of the rest, as wide as a quarter of it.
			const double Trunk = H / 3.0, Cone = H - Trunk;
			Tinted(Trunks, At(X, Y, Z + Trunk / 2.0, 0.35, 0.35, Trunk / 100.0), Bark);
			Tinted(Conifers, At(X, Y, Z + Trunk + Cone / 2.0, Cone / 400.0, Cone / 400.0, Cone / 100.0), Crown);
			break;
		}
		case Vaelen::Scene::TreeKind::Palm:
		{
			// A thin trunk of three quarters, a flat crown on top.
			const double Trunk = H * 0.75;
			Tinted(Trunks, At(X, Y, Z + Trunk / 2.0, 0.25, 0.25, Trunk / 100.0), Bark);
			Tinted(Crowns, At(X, Y, Z + Trunk, H / 250.0, H / 250.0, H / 600.0), Crown);
			break;
		}
		case Vaelen::Scene::TreeKind::Shrub:
			Tinted(Shrubs, At(X, Y, Z + H / 2.0, H / 80.0, H / 80.0, H / 100.0), Crown);
			break;
		default:
		{
			// A trunk of two fifths, a round crown as wide as it is tall.
			const double Trunk = H * 0.4, Ball = H - Trunk;
			Tinted(Trunks, At(X, Y, Z + Trunk / 2.0, 0.4, 0.4, Trunk / 100.0), Bark);
			Tinted(Crowns, At(X, Y, Z + Trunk + Ball / 2.0, Ball / 100.0, Ball / 100.0, Ball / 100.0), Crown);
			break;
		}
		}
	}
	return true;
}

bool AVaelenScenery::Draw(const Vaelen::Scene::Ground& G, const Vaelen::Scene::SceneLayout& L,
						  const Vaelen::Scene::TownLook& T)
{
	for (UInstancedStaticMeshComponent* Each :
		 {Houses, Roofs, Figures, Heads, Company, CompanyHeads, Squares, Roads, Pits, Wells})
	{
		Each->ClearInstances();
	}
	if (!bShapesFound || G.Width == 0u)
	{
		return false;
	}
	// A house: 8 m square (HouseHalfCm), 4 m of wall in the family's plaster,
	// and (23.05) a gabled roof along the ridge Town.h chose - 8.6 m along
	// it, 3.2 m across, its lower edges sunk into the walls - thatch dark;
	// the layout's Z is the ground under its centre. A town look that does
	// not fit the layout (it always does: the subsystem makes both from the
	// same layout) falls back to a ridge along X and no culture.
	for (Vaelen::usize I = 0; I < L.Houses.size(); ++I)
	{
		const Vaelen::Scene::Placed& H = L.Houses[I];
		const Vaelen::Scene::HouseLook Look = I < T.Houses.size() ? T.Houses[I] : Vaelen::Scene::HouseLook{};
		const double X = H.X, Y = H.Y, Z = H.Z;
		Painted(Houses, At(X, Y, Z + 200.0, 8.0, 8.0, 4.0), WallOf(Look.Culture));
		Painted(Roofs, Gable(X, Y, Z + 460.0, Look.RidgeAlongY != 0u, 8.6, 3.2), Thatch);
	}
	// A figure: a 1.6 m cylinder and a head; the company's a size larger, so
	// that who can be spoken to is told from who cannot at a glance.
	for (const Vaelen::Scene::Placed& F : L.Figures)
	{
		const bool bCompany = (F.Flags & Vaelen::Scene::FigureFlag::Company) != 0u;
		const double X = F.X, Y = F.Y, Z = F.Z;
		if (bCompany)
		{
			Company->AddInstance(At(X, Y, Z + 90.0, 0.5, 0.5, 1.8));
			CompanyHeads->AddInstance(At(X, Y, Z + 205.0, 0.4, 0.4, 0.4));
		}
		else
		{
			Figures->AddInstance(At(X, Y, Z + 80.0, 0.4, 0.4, 1.6));
			Heads->AddInstance(At(X, Y, Z + 185.0, 0.3, 0.3, 0.3));
		}
	}
	// A square: a 12 m slab a hand above the ground; a pit: a wide low
	// cylinder sunk into it.
	for (const Vaelen::Scene::Placed& S : L.Squares)
	{
		Painted(Squares, At(S.X, S.Y, static_cast<double>(S.Z) + 10.0, 12.0, 12.0, 0.2), Stone);
		// 23.05: a well at its middle - a 1.2 m ring of stone, waist high.
		Painted(Wells, At(S.X, S.Y, static_cast<double>(S.Z) + 70.0, 1.2, 1.2, 1.0), Stone);
	}
	for (const Vaelen::Scene::Placed& P : L.Pits)
	{
		Painted(Pits, At(P.X, P.Y, static_cast<double>(P.Z) - 60.0, 6.0, 6.0, 1.0), Stone);
	}
	// A road: a slab per tile, on the ground at the tile's centre.
	for (const Vaelen::Scene::RoadPath& R : L.Roads)
	{
		for (const Vaelen::uint32 Tile : R.Tiles)
		{
			Vaelen::int64 X = 0, Y = 0;
			Vaelen::Scene::PointOfTile(G, Tile, X, Y);
			const double Z = static_cast<double>(Vaelen::Scene::HeightAt(G, X, Y)) + 5.0;
			Roads->AddInstance(At(static_cast<double>(X), static_cast<double>(Y), Z, 2.5, 2.5, 0.1));
		}
	}
	return true;
}

void AVaelenScenery::DrawWater(const Vaelen::Scene::Ground& G)
{
	Water->ClearAllMeshSections();
	if (G.Width == 0u)
	{
		return;
	}
	const double C = static_cast<double>(G.Scale.CmPerTile);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colours;
	TArray<FProcMeshTangent> Tangents;
	const auto Quad = [&](double X0, double Y0, double X1, double Y1, double Z, const FLinearColor& Colour)
	{
		const int32 First = Vertices.Num();
		Vertices.Add(FVector(X0, Y0, Z));
		Vertices.Add(FVector(X1, Y0, Z));
		Vertices.Add(FVector(X1, Y1, Z));
		Vertices.Add(FVector(X0, Y1, Z));
		for (int32 i = 0; i < 4; ++i)
		{
			Normals.Add(FVector(0.0, 0.0, 1.0));
			UV0.Add(FVector2D(i == 1 || i == 2 ? 1.0 : 0.0, i >= 2 ? 1.0 : 0.0));
			Colours.Add(Colour);
			Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
		}
		// The same winding as the ground's (Terrain.h): counter-clockwise from above.
		Triangles.Add(First);
		Triangles.Add(First + 1);
		Triangles.Add(First + 2);
		Triangles.Add(First);
		Triangles.Add(First + 2);
		Triangles.Add(First + 3);
	};
	// The sea: one plane at 0 over the whole map, half a tile past its edge.
	Quad(-C / 2.0, -C / 2.0, (G.Width - 0.5) * C, (G.Height - 0.5) * C, 0.0, FLinearColor(0.10f, 0.25f, 0.45f, 1.0f));
	Water->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colours, Tangents, false);
	// A material per section, or the section is drawn with the engine's default
	// and the vertex colour above shows nothing (19.11b, the review). 23.01:
	// the water's own when the commandlet has written it.
	if (WaterPaint != nullptr)
	{
		Water->SetMaterial(0, WaterPaint);
	}
	else if (GEngine != nullptr)
	{
		Water->SetMaterial(0, GEngine->VertexColorMaterial);
	}
	// The lakes: a quad per lake tile at the surface the ground invented for it.
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UV0.Empty();
	Colours.Empty();
	Tangents.Empty();
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		if (G.Kind[T] != Vaelen::Scene::GroundKind::Lake)
		{
			continue;
		}
		Vaelen::int64 X = 0, Y = 0;
		Vaelen::Scene::PointOfTile(G, T, X, Y);
		Quad(X - C / 2.0, Y - C / 2.0, X + C / 2.0, Y + C / 2.0, static_cast<double>(G.LakeSurfaceCm[T]),
			 FLinearColor(0.15f, 0.35f, 0.50f, 1.0f));
	}
	if (Vertices.Num() > 0)
	{
		Water->CreateMeshSection_LinearColor(1, Vertices, Triangles, Normals, UV0, Colours, Tangents, false);
		if (WaterPaint != nullptr)
		{
			Water->SetMaterial(1, WaterPaint);
		}
		else if (GEngine != nullptr)
		{
			Water->SetMaterial(1, GEngine->VertexColorMaterial);
		}
	}
}

AVaelenScenery::FDrawn AVaelenScenery::Drawn() const
{
	FDrawn Out;
	Out.Houses = Houses->GetInstanceCount();
	Out.Figures = Figures->GetInstanceCount() + Company->GetInstanceCount();
	Out.Company = Company->GetInstanceCount();
	Out.Squares = Squares->GetInstanceCount();
	Out.RoadTiles = Roads->GetInstanceCount();
	Out.Pits = Pits->GetInstanceCount();
	Out.Trees = Conifers->GetInstanceCount() + Crowns->GetInstanceCount() + Shrubs->GetInstanceCount();
	Out.Blades = Blades->GetInstanceCount();
	return Out;
}

void AVaelenScenery::OnViewsTaken()
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
	Draw(World->Scene(), World->Layout(), World->Town());
	DrawFlora(World->Wood());
}

void AVaelenScenery::BeginPlay()
{
	Super::BeginPlay();
	// 23.06: the one presentation clock of the walk. It reads where the
	// walker stands and replants the grass around them; it reads nothing of
	// the world and turns nothing (ADR-0138 is about the world's day).
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(GrassTimer, this, &AVaelenScenery::Regrow, 0.5f, true);
	}
}

void AVaelenScenery::SetGrass(bool bOn)
{
	bGrass = bOn;
	if (!bOn)
	{
		Blades->ClearInstances();
		bGrassPlanted = false;
	}
}

namespace
{
	/// Blades per lattice cell by biome, 0..4 of 4 (the cell's hash decides):
	/// Ocean, Ice, Tundra, BorealForest, ColdSteppe, TemperateForest,
	/// Grassland, Scrubland, TropicalForest, Savanna, Desert, Alpine.
	constexpr Vaelen::uint8 GrassDensity[12] = {0, 0, 1, 2, 2, 3, 4, 2, 3, 3, 0, 1};
	struct FGrassTint
	{
		float R, G, B;
	};
	constexpr FGrassTint GrassTintOf(Vaelen::uint8 Biome)
	{
		switch (Biome)
		{
		case 2:
			return {0.52f, 0.54f, 0.40f}; // tundra, dun
		case 3:
		case 5:
		case 8:
			return {0.24f, 0.46f, 0.18f}; // under the trees, deep
		case 4:
			return {0.58f, 0.56f, 0.34f}; // steppe, straw
		case 6:
			return {0.38f, 0.60f, 0.22f}; // grassland
		case 9:
			return {0.66f, 0.60f, 0.30f}; // savanna, gold
		default:
			return {0.50f, 0.52f, 0.36f};
		}
	}
	constexpr double GrassCellCm = 150.0;
	constexpr double GrassReachCm = 6000.0;
	constexpr double GrassReplantCm = 2000.0;
} // namespace

void AVaelenScenery::Regrow()
{
	if (!bGrass || GetWorld() == nullptr || GetWorld()->GetGameInstance() == nullptr)
	{
		return;
	}
	const APlayerController* Who = GetWorld()->GetFirstPlayerController();
	const APawn* Walker = Who != nullptr ? Who->GetPawn() : nullptr;
	const UVaelenWorldSubsystem* World = GetWorld()->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
	if (Walker == nullptr || World == nullptr || !World->Begun())
	{
		return;
	}
	const FVector Here = Walker->GetActorLocation();
	if (bGrassPlanted && (Here - GrassAt).Size() < GrassReplantCm)
	{
		return;
	}
	const Vaelen::Scene::Ground& G = World->Scene();
	const Vaelen::View::ClimateView& Climate = World->Climate();
	Blades->ClearInstances();
	GrassAt = Here;
	bGrassPlanted = true;
	if (G.Width == 0u)
	{
		return;
	}
	const bool bSnowKnown = Climate.Tiles.size() == static_cast<Vaelen::usize>(G.Width) * G.Height;
	int32 Planted = 0;
	const Vaelen::int64 First = static_cast<Vaelen::int64>((Here.X - GrassReachCm) / GrassCellCm);
	const Vaelen::int64 Last = static_cast<Vaelen::int64>((Here.X + GrassReachCm) / GrassCellCm);
	const Vaelen::int64 FirstY = static_cast<Vaelen::int64>((Here.Y - GrassReachCm) / GrassCellCm);
	const Vaelen::int64 LastY = static_cast<Vaelen::int64>((Here.Y + GrassReachCm) / GrassCellCm);
	for (Vaelen::int64 CY = FirstY; CY <= LastY && Planted < GrassCap; ++CY)
	{
		for (Vaelen::int64 CX = First; CX <= Last && Planted < GrassCap; ++CX)
		{
			// The cell's own hash: the same blade whoever walks by, and no
			// blade twice. Its point is the cell's corner plus hashed thirds.
			const Vaelen::Hash64 H =
				Vaelen::Mix64(Vaelen::HashCombine(Vaelen::HashUInt64(static_cast<Vaelen::uint64>(CX)),
												  Vaelen::HashUInt64(static_cast<Vaelen::uint64>(CY))));
			const double X = (static_cast<double>(CX) + static_cast<double>(H & 0xFFu) / 256.0) * GrassCellCm;
			const double Y = (static_cast<double>(CY) + static_cast<double>((H >> 8) & 0xFFu) / 256.0) * GrassCellCm;
			if ((X - Here.X) * (X - Here.X) + (Y - Here.Y) * (Y - Here.Y) > GrassReachCm * GrassReachCm)
			{
				continue;
			}
			Vaelen::uint32 Tile = 0;
			if (!Vaelen::Scene::TileOfPoint(G, static_cast<Vaelen::int64>(X), static_cast<Vaelen::int64>(Y), Tile) ||
				G.Kind[Tile] != Vaelen::Scene::GroundKind::Land)
			{
				continue;
			}
			const Vaelen::uint8 Biome = G.Biome[Tile] < 12u ? G.Biome[Tile] : 0u;
			if (((H >> 16) & 3u) >= GrassDensity[Biome])
			{
				continue;
			}
			const double Z = static_cast<double>(
				Vaelen::Scene::HeightAt(G, static_cast<Vaelen::int64>(X), static_cast<Vaelen::int64>(Y)));
			const double Height = 0.35 + static_cast<double>((H >> 24) & 0xFFu) / 256.0 * 0.45; // 35 to 80 cm
			const double Yaw = static_cast<double>((H >> 32) & 0xFFu) * 180.0 / 256.0;
			FGrassTint Tint = GrassTintOf(Biome);
			if (bSnowKnown)
			{
				const float Snow = static_cast<float>(Vaelen::Scene::SnowOf(Climate.Tiles[Tile])) / 255.0f;
				Tint.R += (0.92f - Tint.R) * Snow;
				Tint.G += (0.94f - Tint.G) * Snow;
				Tint.B += (0.96f - Tint.B) * Snow;
			}
			// Two crossed blades: the plane stood up (pitch 90), as tall as
			// Height, a quarter as wide; the second turned a right angle.
			for (int32 Cross = 0; Cross < 2; ++Cross)
			{
				const FTransform Where(FRotator(90.0, Yaw + 90.0 * Cross, 0.0), FVector(X, Y, Z + Height * 50.0),
									   FVector(Height, Height * 0.25, 1.0));
				const int32 Index = Blades->AddInstance(Where);
				Blades->SetCustomDataValue(Index, 0, Tint.R);
				Blades->SetCustomDataValue(Index, 1, Tint.G);
				Blades->SetCustomDataValue(Index, 2, Tint.B);
			}
			Planted += 2;
		}
	}
	Blades->MarkRenderStateDirty();
}
