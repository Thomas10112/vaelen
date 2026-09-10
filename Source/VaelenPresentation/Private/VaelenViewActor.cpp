// VAELEN - VaelenPresentation. Phase 13 task 13.07c.
//
// STATUS: UNVERIFIED - never compiled by UnrealBuildTool, never run.
//
// This is the ONE file of the module that knows what a World is, and it knows
// it for exactly as long as it takes to take three views of one. Everything
// after the closing brace of that scope draws from numbers.
#include "VaelenViewActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

// The kernel. Nothing above this line knows about it, nothing below knows about
// Unreal, and the two meet only in this file - the same division the atlas
// actor keeps, for the same reason.
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
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include <memory>

DEFINE_LOG_CATEGORY_STATIC(LogVaelenView, Log, All);

namespace
{
	/// Everything the kernel needs to run a world, in the order the headless
	/// gates use it. Copied deliberately from AVaelenAtlasActor's KernelRun
	/// rather than reinvented: the ordering here is the one ADR-0055 settled
	/// after a Phase 06 gate went looking for it, and a second, subtly
	/// different wiring in the same project would be a bug waiting for a year
	/// when the two disagree.
	///
	/// It lives in an anonymous namespace inside the ONE file allowed to know
	/// about worlds. Nothing in VaelenViewDrawer.cpp can name any of it.
	struct FWorldRun
	{
		explicit FWorldRun(uint64 InSeed) : Instance(Config(InSeed)), Ages(Instance, Vaelen::History::PreHistoryRules{})
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
			Roads_ = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Trade,
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
			Instance.Systems().Add(Roads_.get());
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

		/// The region with the most people: the one worth simulating person by
		/// person. Ties go to the lower index so the choice is the same on
		/// every machine, which is the whole point of asking.
		Vaelen::uint32 Busiest() const
		{
			Vaelen::uint32 Best = 0;
			Vaelen::uint32 People = 0;
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

		/// What the view is to be told to look at. People and roads; the rest
		/// of the optional sources are left off because this world does not
		/// declare them, and 13.01 was built so that a view of a world without
		/// an economy says 0 roads rather than refusing to be taken.
		Vaelen::View::ViewSources Sources() const
		{
			Vaelen::View::ViewSources S;
			S.Types = Ages.Types();
			S.Persons = Persons;
			S.HasTrade = true;
			S.Trade = Trade;
			return S;
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
		std::unique_ptr<Vaelen::Economy::TradeSystem> Roads_;
		std::unique_ptr<Vaelen::Economy::WealthSystem> Purses;
		std::unique_ptr<Vaelen::Society::StandingSystem> Ranks;
		std::unique_ptr<Vaelen::Politics::PolitySystem> Rulers;
	};

	UHierarchicalInstancedStaticMeshComponent* MakeLayer(AActor* Owner, USceneComponent* Under, const TCHAR* Name)
	{
		UHierarchicalInstancedStaticMeshComponent* C =
			Owner->CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Under);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	}
} // namespace

AVaelenViewActor::AVaelenViewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Plate = CreateDefaultSubobject<USceneComponent>(TEXT("Plate"));
	RootComponent = Plate;
	Ground = MakeLayer(this, Plate, TEXT("Ground"));
	Towns = MakeLayer(this, Plate, TEXT("Towns"));
	Roads = MakeLayer(this, Plate, TEXT("Roads"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Ground->SetStaticMesh(Cube.Object);
		Towns->SetStaticMesh(Cube.Object);
		Roads->SetStaticMesh(Cube.Object);
	}
}

void AVaelenViewActor::BeginPlay()
{
	Super::BeginPlay();
}

void AVaelenViewActor::Clear()
{
	if (Ground != nullptr)
	{
		Ground->ClearInstances();
	}
	if (Towns != nullptr)
	{
		Towns->ClearInstances();
	}
	if (Roads != nullptr)
	{
		Roads->ClearInstances();
	}
	Report = TEXT("cleared");
}

void AVaelenViewActor::BuildFromView()
{
	Clear();

	// The three views, declared OUTSIDE the scope that owns the world. That
	// placement is the whole task: they are filled inside it and read after it
	// has closed.
	Vaelen::View::MapView Map;
	Vaelen::View::WorldView Frame;
	Vaelen::View::NetView Net;

	const double Started = FPlatformTime::Seconds();
	double Simulated = 0.0;
	{
		FWorldRun Run(static_cast<uint64>(Seed));

		Vaelen::WorldGenConfig Gen;
		Gen.Width = static_cast<Vaelen::uint32>(FMath::Clamp(WorldSize, 32, 512));
		Gen.Height = Gen.Width;
		if (!Run.Ages.Generate(Gen, static_cast<Vaelen::uint32>(FMath::Max(0, PreHistoryYears))))
		{
			Report = TEXT("generation failed");
			UE_LOG(LogVaelenView, Error, TEXT("AELVOR: generation failed"));
			return;
		}
		// One region simulated person by person, chosen after the pre-history
		// rather than before it: ADR-0122 found that asking at year zero gives
		// a DIFFERENT world, because nobody has spread yet and the busiest
		// region is not yet busy.
		Vaelen::Population::RequestDetail(Run.Instance, Run.Lod, Run.Busiest());
		Run.Ages.Run(static_cast<Vaelen::uint32>(FMath::Max(0, Years)));
		Simulated = FPlatformTime::Seconds() - Started;

		const Vaelen::View::ViewSources From = Run.Sources();
		Vaelen::View::TakeMapView(Run.Instance, From, Map);
		Vaelen::View::TakeView(Run.Instance, From, Frame);
		Vaelen::View::TakeNetView(Run.Instance, From, Net);
	}
	// THE WORLD IS GONE. Its entities, its components, its map and its event
	// log were destroyed by the brace above. Everything below draws from three
	// blocks of numbers that outlived it, and if any of them were secretly
	// holding a handle back into it, what follows would be garbage or a crash.
	// That is not a side effect of the design; it IS the design, and this is
	// the only place in the project where a person can watch it happen.

	FVaelenDrawSettings How;
	How.TileSize = TileSize;
	How.ReliefScale = ReliefScale;
	How.SlabHeight = SlabHeight;

	FVaelenDrawTally Tally;
	Tally.Tiles = VaelenViewDrawer::DrawGround(Map, How, Ground, Tally.Land);
	Tally.Towns = VaelenViewDrawer::DrawTowns(Frame, Map, How, Towns);
	Tally.Roads = VaelenViewDrawer::DrawRoads(Net, Frame, Map, How, Roads, Tally.SkippedRoads);

	const Vaelen::View::ViewStats Weighed = Vaelen::View::MeasureView(Frame);
	Report = FString::Printf(
		TEXT(
			"AELVOR %dx%d, year %u: %d tiles (%d land), %u regions (%u peopled), %u living, %d towns, %d roads of %d, ")
			TEXT("view %u bytes, simulated in %.2f s"),
		WorldSize, WorldSize, Frame.Year, Tally.Tiles, Tally.Land, Weighed.Regions, Weighed.Peopled, Frame.People,
		Tally.Towns, Tally.Roads, static_cast<int32>(Net.Routes.size()), Weighed.Bytes, Simulated);
	if (Tally.SkippedRoads > 0)
	{
		// Said out loud rather than swallowed: a road the frame could not place
		// is a gap between two views that are meant to agree.
		Report += FString::Printf(TEXT(" - %d roads not drawn, regions missing from the frame"), Tally.SkippedRoads);
	}
	UE_LOG(LogVaelenView, Display, TEXT("%s"), *Report);
}
