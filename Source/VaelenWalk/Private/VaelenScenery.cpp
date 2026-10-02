// VAELEN - VaelenWalk
// Phase 19 task 19.11: the world drawn is the world built. See VaelenScenery.h.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25 (reviewed and corrected 2026-09-26, 19.11b), not yet built by
// UnrealBuildTool nor run: sitting S3 builds it.
#include "VaelenScenery.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
	// 23.02: the wood. Three floats of custom data per instance: the tint
	// the material of 23.01 reads (PerInstanceCustomData 0-2); nothing reads
	// them until it lands, and they cost nothing to set.
	Trunks = Shapes(this, Water, TEXT("Trunks"), Cylinder.Object, false, bFound);
	Conifers = Shapes(this, Water, TEXT("Conifers"), Cone.Object, false, bFound);
	Crowns = Shapes(this, Water, TEXT("Crowns"), Sphere.Object, false, bFound);
	Shrubs = Shapes(this, Water, TEXT("Shrubs"), Sphere.Object, false, bFound);
	for (UInstancedStaticMeshComponent* Each : {Trunks, Conifers, Crowns, Shrubs})
	{
		Each->NumCustomDataFloats = 3;
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

bool AVaelenScenery::Draw(const Vaelen::Scene::Ground& G, const Vaelen::Scene::SceneLayout& L)
{
	for (UInstancedStaticMeshComponent* Each :
		 {Houses, Roofs, Figures, Heads, Company, CompanyHeads, Squares, Roads, Pits})
	{
		Each->ClearInstances();
	}
	if (!bShapesFound || G.Width == 0u)
	{
		return false;
	}
	// A house: 8 m square (HouseHalfCm), 4 m of wall, a 3 m roof on it; the
	// layout's Z is the ground under its centre.
	for (const Vaelen::Scene::Placed& H : L.Houses)
	{
		const double X = H.X, Y = H.Y, Z = H.Z;
		Houses->AddInstance(At(X, Y, Z + 200.0, 8.0, 8.0, 4.0));
		Roofs->AddInstance(At(X, Y, Z + 400.0, 8.0, 8.0, 3.0));
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
		Squares->AddInstance(At(S.X, S.Y, static_cast<double>(S.Z) + 10.0, 12.0, 12.0, 0.2));
	}
	for (const Vaelen::Scene::Placed& P : L.Pits)
	{
		Pits->AddInstance(At(P.X, P.Y, static_cast<double>(P.Z) - 60.0, 6.0, 6.0, 1.0));
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
	// and the vertex colour above shows nothing (19.11b, the review).
	if (GEngine != nullptr)
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
		if (GEngine != nullptr)
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
	Draw(World->Scene(), World->Layout());
	DrawFlora(World->Wood());
}
