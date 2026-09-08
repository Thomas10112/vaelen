// VAELEN - the world in the viewport.
//
// STATUS: UNVERIFIED - engine-side, compiled by UBT only.

#include "VaelenAtlasActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/ConstructorHelpers.h"

// The kernel. Nothing above this line knows about it, nothing below it knows
// about Unreal: the two meet only inside this file.
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/TileGrid.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Sim/WorldMap.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include <memory>
#include <vector>

DEFINE_LOG_CATEGORY_STATIC(LogVaelenAtlas, Log, All);

namespace
{
	/// The plate's palette. One instanced-mesh component per entry, so the
	/// whole world is drawn in as many draw calls as there are colours here.
	enum class Paint : uint8
	{
		Ocean = 0,
		Shore,
		Lake,
		River,
		Ice,
		Tundra,
		BorealForest,
		ColdSteppe,
		TemperateForest,
		Grassland,
		Scrubland,
		TropicalForest,
		Savanna,
		Desert,
		Alpine,
		Town,
		Seat,
		Count
	};

	FColor PaintColour(Paint P)
	{
		switch (P)
		{
		case Paint::Ocean:
			return FColor(26, 50, 90);
		case Paint::Shore:
			return FColor(50, 86, 132);
		case Paint::Lake:
			return FColor(58, 106, 152);
		case Paint::River:
			return FColor(70, 124, 170);
		case Paint::Ice:
			return FColor(233, 238, 244);
		case Paint::Tundra:
			return FColor(159, 169, 157);
		case Paint::BorealForest:
			return FColor(55, 80, 59);
		case Paint::ColdSteppe:
			return FColor(147, 149, 109);
		case Paint::TemperateForest:
			return FColor(67, 106, 57);
		case Paint::Grassland:
			return FColor(135, 157, 85);
		case Paint::Scrubland:
			return FColor(157, 143, 93);
		case Paint::TropicalForest:
			return FColor(38, 93, 50);
		case Paint::Savanna:
			return FColor(175, 159, 85);
		case Paint::Desert:
			return FColor(211, 193, 137);
		case Paint::Alpine:
			return FColor(167, 165, 169);
		case Paint::Town:
			return FColor(196, 62, 40);
		case Paint::Seat:
			return FColor(232, 190, 92);
		default:
			return FColor::White;
		}
	}

	Paint PaintOfBiome(Vaelen::WorldGen::Biome B)
	{
		using Vaelen::WorldGen::Biome;
		switch (B)
		{
		case Biome::Ice:
			return Paint::Ice;
		case Biome::Tundra:
			return Paint::Tundra;
		case Biome::BorealForest:
			return Paint::BorealForest;
		case Biome::ColdSteppe:
			return Paint::ColdSteppe;
		case Biome::TemperateForest:
			return Paint::TemperateForest;
		case Biome::Grassland:
			return Paint::Grassland;
		case Biome::Scrubland:
			return Paint::Scrubland;
		case Biome::TropicalForest:
			return Paint::TropicalForest;
		case Biome::Savanna:
			return Paint::Savanna;
		case Biome::Desert:
			return Paint::Desert;
		case Biome::Alpine:
			return Paint::Alpine;
		default:
			return Paint::Ocean;
		}
	}

	/// Everything the kernel needs to run a world, assembled once. The same
	/// wiring the headless gates use, so the editor sees the world the tests
	/// see - including the one ordering the Phase 06 gate settled (ADR-0055).
	struct KernelRun
	{
		explicit KernelRun(uint64 InSeed) : Instance(Config(InSeed)), Ages(Instance, Vaelen::History::PreHistoryRules{})
		{
			using namespace Vaelen::Economy;
			using namespace Vaelen::Politics;
			using namespace Vaelen::Population;
			using namespace Vaelen::Society;

			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Economy = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Wealth = WealthTypes::Declare(Instance);
			Polities = PolityTypes::Declare(Instance);

			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy, Production,
														 ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Purses = std::make_unique<WealthSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Norms,
													Wealth, WealthRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Rulers =
				std::make_unique<PolitySystem>(Instance, Ages.Types(), Persons, Organizations, Polities, PolityRules{});

			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Orgs->RunAfter("Needs");
			Ranks->RunAfter("Wealth");
			Ranks->ObserveWealth(Wealth.Wealth);
			Stocks->RunAfter("Lod");
			Stocks->ObserveHeirs(Wealth.Heir);
			Harvest->ObserveTraits(Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Rulers->RunAfter("Lod");

			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Systems().Add(Purses.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Rulers.get());
			Instance.Build();
		}

		static Vaelen::WorldConfig Config(uint64 InSeed)
		{
			Vaelen::WorldConfig C;
			C.Seed = InSeed;
			return C;
		}

		/// The region with the most people: the one worth simulating person by person.
		uint32 Busiest() const
		{
			uint32 Best = 0;
			uint32 People = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](Vaelen::EntityHandle H, const Vaelen::WorldGen::RegionInfo& R)
					{
						const Vaelen::History::RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && (P->Total > People || (P->Total == People && R.Index < Best)))
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}

		Vaelen::World Instance;
		Vaelen::History::PreHistory Ages;
		Vaelen::Population::PersonTypes Persons;
		Vaelen::Population::FamilyTypes Families;
		Vaelen::Population::NeedTypes Needs;
		Vaelen::Population::TraitTypes Traits;
		Vaelen::Population::LodTypes Lod;
		Vaelen::Society::OrganizationTypes Organizations;
		Vaelen::Society::StandingTypes Standing;
		Vaelen::Society::NormTypes Norms;
		Vaelen::Economy::EconomyTypes Economy;
		Vaelen::Economy::ProductionTypes Production;
		Vaelen::Economy::MarketTypes Markets;
		Vaelen::Economy::TradeTypes Trade;
		Vaelen::Economy::WealthTypes Wealth;
		Vaelen::Politics::PolityTypes Polities;
		std::unique_ptr<Vaelen::Population::LifeSystem> Lives;
		std::unique_ptr<Vaelen::Population::FamilySystem> Houses;
		std::unique_ptr<Vaelen::Population::NeedSystem> Body;
		std::unique_ptr<Vaelen::Population::TraitSystem> Minds;
		std::unique_ptr<Vaelen::Population::LodSystem> Bridge;
		std::unique_ptr<Vaelen::Society::OrganizationSystem> Orgs;
		std::unique_ptr<Vaelen::Society::NormSystem> Customs;
		std::unique_ptr<Vaelen::Economy::StockSystem> Stocks;
		std::unique_ptr<Vaelen::Economy::ProductionSystem> Harvest;
		std::unique_ptr<Vaelen::Economy::MarketSystem> Fair;
		std::unique_ptr<Vaelen::Economy::TradeSystem> Roads;
		std::unique_ptr<Vaelen::Economy::WealthSystem> Purses;
		std::unique_ptr<Vaelen::Society::StandingSystem> Ranks;
		std::unique_ptr<Vaelen::Politics::PolitySystem> Rulers;
	};
} // namespace

AVaelenAtlasActor::AVaelenAtlasActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Plate = CreateDefaultSubobject<USceneComponent>(TEXT("Plate"));
	RootComponent = Plate;
}

void AVaelenAtlasActor::EnsurePaintLayers()
{
	if (PaintLayers.Num() == static_cast<int32>(Paint::Count))
	{
		return;
	}
	for (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& Old : PaintLayers)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	PaintLayers.Reset();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Guessing a parameter name is how the plate came out unpainted the first
	// time: BasicShapeMaterial's colour is not called what one would expect.
	// Ask the engine instead. Take the first stock material that loads AND
	// carries a vector parameter, and set every vector parameter it has to the
	// layer's colour - whatever they are called.
	static const TCHAR* const Candidates[] = {
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"),
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"),
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"),
		TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"),
	};
	UMaterialInterface* Plain = nullptr;
	TArray<FMaterialParameterInfo> Colours;
	for (const TCHAR* const Path : Candidates)
	{
		UMaterialInterface* Candidate = LoadObject<UMaterialInterface>(nullptr, Path);
		if (Candidate == nullptr)
		{
			continue;
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Candidate->GetAllVectorParameterInfo(Infos, Ids);
		FString Names;
		for (const FMaterialParameterInfo& Info : Infos)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", "));
			Names += Info.Name.ToString();
		}
		UE_LOG(LogVaelenAtlas, Display, TEXT("material %s: %d vector parameter(s)%s%s"), Path, Infos.Num(),
			   Infos.Num() > 0 ? TEXT(" — ") : TEXT(""), Infos.Num() > 0 ? *Names : TEXT(""));
		if (Plain == nullptr && Infos.Num() > 0)
		{
			Plain = Candidate;
			Colours = MoveTemp(Infos);
		}
	}
	if (Plain == nullptr)
	{
		UE_LOG(LogVaelenAtlas, Warning,
			   TEXT("no stock material carries a colour parameter; the plate will be drawn in one tone"));
		Plain = LoadObject<UMaterialInterface>(nullptr, Candidates[0]);
	}

	for (int32 Index = 0; Index < static_cast<int32>(Paint::Count); ++Index)
	{
		const FName Name(*FString::Printf(TEXT("Paint_%d"), Index));
		UHierarchicalInstancedStaticMeshComponent* Layer =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		Layer->SetupAttachment(Plate);
		Layer->RegisterComponent();
		Layer->SetMobility(EComponentMobility::Movable);
		Layer->SetCastShadow(false);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cube != nullptr)
		{
			Layer->SetStaticMesh(Cube);
		}
		if (Plain != nullptr)
		{
			UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Plain, Layer);
			if (Tint != nullptr)
			{
				const FLinearColor C = FLinearColor(PaintColour(static_cast<Paint>(Index)));
				for (const FMaterialParameterInfo& Info : Colours)
				{
					Tint->SetVectorParameterValue(Info.Name, C);
				}
				Layer->SetMaterial(0, Tint);
			}
		}
		AddInstanceComponent(Layer);
		PaintLayers.Add(Layer);
	}
}

void AVaelenAtlasActor::ClearAelvor()
{
	for (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& Layer : PaintLayers)
	{
		if (Layer != nullptr)
		{
			Layer->ClearInstances();
		}
	}
	if (UWorld* Here = GetWorld())
	{
		FlushPersistentDebugLines(Here);
	}
	Report = TEXT("cleared");
}

void AVaelenAtlasActor::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildOnBeginPlay)
	{
		BuildAelvor();
	}
}

void AVaelenAtlasActor::BuildAelvor()
{
	using namespace Vaelen::Economy;
	using namespace Vaelen::Politics;
	using namespace Vaelen::WorldGen;
	// Named one by one rather than by directive: the engine spells uint32 and
	// int64 too, and only the names actually used are wanted here.
	using Vaelen::EntityHandle;
	using Vaelen::TileCoord;
	using Vaelen::WorldGenConfig;
	using Vaelen::WorldGrid;
	using Vaelen::WorldMap;

	EnsurePaintLayers();
	ClearAelvor();

	const double Started = FPlatformTime::Seconds();
	KernelRun Run(static_cast<uint64>(Seed));

	WorldGenConfig Gen;
	Gen.Width = static_cast<uint32>(FMath::Clamp(WorldSize, 32, 512));
	Gen.Height = Gen.Width;
	if (!Run.Ages.Generate(Gen, static_cast<uint32>(FMath::Max(0, PreHistoryYears))))
	{
		Report = TEXT("generation failed");
		UE_LOG(LogVaelenAtlas, Error, TEXT("AELVOR: generation failed"));
		return;
	}
	const Vaelen::uint32 Detail = Run.Busiest();
	Vaelen::Population::RequestDetail(Run.Instance, Run.Lod, Detail);
	Run.Ages.Run(static_cast<uint32>(FMath::Max(0, Years)));
	const double Simulated = FPlatformTime::Seconds() - Started;

	const WorldMap& Map = Run.Instance.Map();
	const WorldGrid Grid = Map.Grid();
	const Vaelen::History::PreHistoryTypes& T = Run.Ages.Types();
	const auto& Biomes = Map.GetLayer(T.World.Layers.Biome);
	const auto& Terrain = Map.GetLayer(T.World.Layers.Terrain);
	const auto& Height = Map.GetLayer(T.World.Layers.Elevation);
	const auto& Rivers = Map.GetLayer(T.World.Hydro.RiverIndex);
	const auto& Lakes = Map.GetLayer(T.World.Hydro.LakeIndex);

	// One pass over the tiles: a slab per tile, in the bucket of its colour.
	TArray<TArray<FTransform>> Batches;
	Batches.SetNum(static_cast<int32>(Paint::Count));
	const float Half = TileSize * 0.5f;
	const float Span = TileSize * static_cast<float>(Grid.Width) * 0.5f;
	auto WorldXY = [&](uint32 X, uint32 Y) -> FVector2D
	{ return FVector2D(static_cast<float>(X) * TileSize - Span, static_cast<float>(Y) * TileSize - Span); };

	int32 LandTiles = 0;
	for (uint32 Y = 0; Y < static_cast<uint32>(Grid.Height); ++Y)
	{
		for (uint32 X = 0; X < static_cast<uint32>(Grid.Width); ++X)
		{
			const uint32 I = Grid.IndexOf(TileCoord{static_cast<int32>(X), static_cast<int32>(Y)});
			const bool Land = (Terrain[I] & TerrainFlag::Land) != 0;
			Paint P = Paint::Ocean;
			if (!Land)
			{
				P = (Terrain[I] & TerrainFlag::Shore) != 0 ? Paint::Shore : Paint::Ocean;
			}
			else
			{
				P = PaintOfBiome(static_cast<Biome>(Biomes[I]));
				++LandTiles;
			}
			if (Lakes[I] != 0)
			{
				P = Paint::Lake;
			}
			if (Rivers[I] != 0)
			{
				P = Paint::River;
			}
			// Fix64 is Q32.32: the raw value over 2^32 is the value in units.
			const double Units = static_cast<double>(Height[I]) / 4294967296.0;
			const float Z = Land ? static_cast<float>(Units) * ReliefScale : 0.0f;
			const FVector2D At = WorldXY(X, Y);
			// The engine's cube is 100 cm a side and centred on its origin.
			FTransform Slab;
			Slab.SetScale3D(FVector(TileSize / 100.0f, TileSize / 100.0f, SlabHeight / 100.0f));
			Slab.SetLocation(FVector(At.X + Half, At.Y + Half, Z));
			Batches[static_cast<int32>(P)].Add(Slab);
		}
	}

	// The centroid of every region, for the towns, the roads and the seats.
	TArray<FVector> Centre;
	TArray<int32> People;
	Centre.Init(FVector::ZeroVector, 1);
	People.Init(0, 1);
	Run.Instance.Components()
		.GetPool(T.World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const int32 Index = static_cast<int32>(R.Index);
				if (Index >= Centre.Num())
				{
					Centre.SetNum(Index + 1);
					People.SetNumZeroed(Index + 1);
				}
				const TileCoord C = Grid.CoordOf(R.CentroidTile);
				const uint32 I = Grid.IndexOf(C);
				const double Units = static_cast<double>(Height[I]) / 4294967296.0;
				const FVector2D At = WorldXY(static_cast<uint32>(C.X), static_cast<uint32>(C.Y));
				Centre[Index] = FVector(At.X + Half, At.Y + Half, static_cast<float>(Units) * ReliefScale);
				const Vaelen::History::RegionPopulation* Counts =
					Run.Instance.Components().GetPool(T.Population.Population).TryGet(H);
				People[Index] = Counts != nullptr ? static_cast<int32>(Counts->Total) : 0;
			});

	int32 Towns = 0;
	if (bShowTowns)
	{
		Run.Instance.Components()
			.GetPool(Run.Trade.Settlement)
			.ForEach(
				[&](EntityHandle, const SettlementInfo& S)
				{
					const int32 Index = static_cast<int32>(S.Region);
					if (S.Abandoned != 0 || Index <= 0 || Index >= Centre.Num())
					{
						return;
					}
					FTransform Mark;
					Mark.SetScale3D(FVector(TileSize / 150.0f, TileSize / 150.0f, TileSize / 55.0f));
					Mark.SetLocation(Centre[Index] + FVector(0, 0, TileSize * 0.9f));
					Batches[static_cast<int32>(Paint::Town)].Add(Mark);
					++Towns;
				});
	}

	int32 Seats = 0;
	if (bShowSeats)
	{
		Run.Instance.Components()
			.GetPool(Run.Polities.Polity)
			.ForEach(
				[&](EntityHandle, const PolityInfo& P)
				{
					const int32 Index = static_cast<int32>(P.Seat);
					if (P.Dissolved != 0 || Index <= 0 || Index >= Centre.Num())
					{
						return;
					}
					FTransform Mark;
					Mark.SetScale3D(FVector(TileSize / 90.0f, TileSize / 90.0f, TileSize / 30.0f));
					Mark.SetLocation(Centre[Index] + FVector(0, 0, TileSize * 1.6f));
					Batches[static_cast<int32>(Paint::Seat)].Add(Mark);
					++Seats;
				});
	}

	for (int32 Index = 0; Index < Batches.Num(); ++Index)
	{
		if (PaintLayers.IsValidIndex(Index) && PaintLayers[Index] != nullptr && Batches[Index].Num() > 0)
		{
			PaintLayers[Index]->AddInstances(Batches[Index], false, false);
		}
	}

	int32 RoadCount = 0;
	if (bShowRoads)
	{
		if (UWorld* Here = GetWorld())
		{
			Run.Instance.Components()
				.GetPool(Run.Trade.Route)
				.ForEach(
					[&](EntityHandle, const RouteInfo& R)
					{
						const int32 From = static_cast<int32>(R.From);
						const int32 To = static_cast<int32>(R.To);
						if (R.Closed != 0 || From <= 0 || To <= 0 || From >= Centre.Num() || To >= Centre.Num())
						{
							return;
						}
						const FVector Lift(0, 0, TileSize * 0.5f);
						const FTransform Where = GetActorTransform();
						DrawDebugLine(Here, Where.TransformPosition(Centre[From] + Lift),
									  Where.TransformPosition(Centre[To] + Lift), FColor(246, 206, 112), true, -1.0f, 0,
									  TileSize * 0.06f);
						++RoadCount;
					});
		}
	}

	int32 Peopled = 0;
	int64 Living = 0;
	for (int32 Index = 1; Index < People.Num(); ++Index)
	{
		Peopled += People[Index] > 0 ? 1 : 0;
		Living += People[Index];
	}
	const PolityStats Rule = MeasurePolities(Run.Instance, T, Run.Persons, Run.Organizations, Run.Polities);

	Report = FString::Printf(
		TEXT("AELVOR %dx%d, seed 0x%llx: year %d, %d land tiles, %d regions peopled, %lld living, %d towns, %d roads, ")
			TEXT("%u polities standing (region %u simulated person by person). Simulated in %.2f s."),
		static_cast<int32>(Grid.Width), static_cast<int32>(Grid.Height), static_cast<unsigned long long>(Seed),
		PreHistoryYears + Years, LandTiles, Peopled, static_cast<long long>(Living), Towns, RoadCount, Rule.Standing,
		Detail, Simulated);
	UE_LOG(LogVaelenAtlas, Display, TEXT("%s"), *Report);
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor(232, 190, 92), Report);
	}
}

namespace
{
	/// A console command, so that the atlas can be raised without touching the
	/// editor's viewport: type "Vaelen.Atlas" (optionally with a size and a
	/// number of years) and the world is generated and laid out in place. An
	/// actor already in the level is reused; otherwise one is spawned.
	void RaiseAtlas(const TArray<FString>& Args, UWorld* World_)
	{
		if (World_ == nullptr)
		{
			UE_LOG(LogVaelenAtlas, Error, TEXT("Vaelen.Atlas: no world"));
			return;
		}
		AVaelenAtlasActor* Atlas = nullptr;
		for (TActorIterator<AVaelenAtlasActor> It(World_); It; ++It)
		{
			Atlas = *It;
			break;
		}
		if (Atlas == nullptr)
		{
			Atlas = World_->SpawnActor<AVaelenAtlasActor>();
		}
		if (Atlas == nullptr)
		{
			UE_LOG(LogVaelenAtlas, Error, TEXT("Vaelen.Atlas: could not spawn the actor"));
			return;
		}
		if (Args.Num() > 0)
		{
			Atlas->WorldSize = FCString::Atoi(*Args[0]);
		}
		if (Args.Num() > 1)
		{
			Atlas->Years = FCString::Atoi(*Args[1]);
		}
		Atlas->BuildAelvor();
	}

	FAutoConsoleCommandWithWorldAndArgs
		GRaiseAtlas(TEXT("Vaelen.Atlas"),
					TEXT("Generate AELVOR and lay it out in the level. Vaelen.Atlas [size] [years]"),
					FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RaiseAtlas));
} // namespace
