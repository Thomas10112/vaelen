// VAELEN - Tests/View
// Phase 13.01: a read-only view of the world for a frame.
//
// The layering rule has said since Phase 00 that PRESENTATION reads WORLD STATE
// and does not touch it, and for thirteen phases that was a promise everybody
// remembered. This file is about the claim that it is now structural: a
// WorldView is a flat block of numbers with no handle, no component type and no
// pointer back into the world, so a renderer holding one has nothing to reach
// with.
//
// STATUS: PROTOTYPE (Phase 13)

#include "Vaelen/View/Frame.h"

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::View;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogView);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Norms_ = NormTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms_, NormRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms_, Standing, Bondage,
													BondageRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		static WorldGenConfig Square(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			return Gen;
		}
		ViewSources Sources() const
		{
			ViewSources S;
			S.Types = Ages.Types();
			S.Persons = Persons;
			S.HasBondage = true;
			S.Bondage = Bondage;
			S.HasTrade = true;
			S.Trade = Trade;
			return S;
		}
		uint32 Busiest() const
		{
			uint32 Best = 0;
			uint32 People = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && (P->Total > People || (P->Total == People && R.Index < Best)))
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		NormTypes Norms_;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		BondageTypes Bondage;
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
	};
} // namespace

VAELEN_TEST(Frame, AViewCarriesNoWayBackIntoTheWorld)
{
	// The claim the module exists for, made where the compiler can check it. If
	// a RegionView ever grows a pointer, a handle or a component type, this stops
	// compiling - which is the point of putting it here rather than in a comment.
	static_assert(std::is_trivially_copyable<RegionView>::value,
				  "a RegionView must be copyable by memcpy: no pointer, no handle, no vtable");
	static_assert(std::is_standard_layout<RegionView>::value, "and laid out plainly enough to hand to a GPU");
	VT_CHECK_MSG(sizeof(RegionView) == 56, "and it weighs what the header says it does");

	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView V;
	TakeView(W.Instance, W.Sources(), V);
	const ViewStats S = MeasureView(V);
	VAELEN_LOG_INFO(LogView, "year %u: %u regions (%u peopled, %u detailed), %u people, %u bytes for the frame", V.Year,
					S.Regions, S.Peopled, S.Detailed, V.People, S.Bytes);
	VT_CHECK_MSG(S.Regions > 0, "the view has the world's ground in it");
	VT_CHECK_MSG(S.Peopled > 0, "and the people on it");
	VT_CHECK_MSG(V.Width == 128 && V.Height == 128, "and the map it is drawn on");
}

VAELEN_TEST(Frame, TheSameWorldGivesTheSameFrame)
{
	// Taken twice from a world that has not ticked. A view that differed between
	// two takings would flicker on screen without anything having happened.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView A;
	WorldView B;
	TakeView(W.Instance, W.Sources(), A);
	TakeView(W.Instance, W.Sources(), B);
	VT_CHECK_EQ(MeasureView(A).Digest, MeasureView(B).Digest);
	VT_CHECK_EQ(A.Regions.size(), B.Regions.size());

	// And the regions come out in index order, every time, so a renderer can
	// keep its own array in step with the view's.
	bool Ordered = true;
	for (usize i = 1; i < A.Regions.size(); ++i)
	{
		Ordered = Ordered && A.Regions[i - 1].Index < A.Regions[i].Index;
	}
	VT_CHECK_MSG(Ordered, "the regions are in index order and never in pool order");
	VT_REQUIRE(!A.Regions.empty());
	const RegionView* Found = RegionIn(A, A.Regions.back().Index);
	VT_REQUIRE(Found != nullptr);
	VT_CHECK_EQ(Found->Index, A.Regions.back().Index);
	VT_CHECK_MSG(RegionIn(A, 0u) == nullptr, "and a region the view does not have comes back as nothing");
}

VAELEN_TEST(Frame, TakingAFrameChangesNothing)
{
	// The other half of read-only, and the half a const signature cannot prove
	// on its own: a world that has had a view taken from it is the same world,
	// down to its state digest and its event log.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Instance.TickMany(TicksPerYear * 3);
	const Hash64 Before = ComputeStateDigest(W.Instance);
	const Hash64 LogBefore = W.Instance.Log().Digest();
	const usize EventsBefore = W.Instance.Log().All().size();

	WorldView V;
	for (uint32 Frame = 0; Frame < 60; ++Frame)
	{
		TakeView(W.Instance, W.Sources(), V);
	}
	VAELEN_LOG_INFO(LogView, "sixty frames taken: %zu events before, %zu after", EventsBefore,
					W.Instance.Log().All().size());
	VT_CHECK_MSG(ComputeStateDigest(W.Instance) == Before, "a second of frames left the world exactly as it was");
	VT_CHECK_MSG(W.Instance.Log().Digest() == LogBefore, "and wrote nothing into its history");
	VT_CHECK_EQ(W.Instance.Log().All().size(), EventsBefore);
}

VAELEN_TEST(Frame, AViewFollowsTheWorldThroughAYear)
{
	// A frame is only worth taking if it changes when the world does.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView Early;
	TakeView(W.Instance, W.Sources(), Early);
	const ViewStats First = MeasureView(Early);
	W.Instance.TickMany(TicksPerYear * 5);
	WorldView Late;
	TakeView(W.Instance, W.Sources(), Late);
	const ViewStats Then = MeasureView(Late);

	VAELEN_LOG_INFO(LogView, "year %u: %u people, %u peopled regions, digest %016llx", Early.Year, Early.People,
					First.Peopled, static_cast<unsigned long long>(First.Digest));
	VAELEN_LOG_INFO(LogView, "year %u: %u people, %u peopled regions, digest %016llx", Late.Year, Late.People,
					Then.Peopled, static_cast<unsigned long long>(Then.Digest));
	VT_CHECK_MSG(Late.Tick > Early.Tick, "the frame knows when it was taken");
	VT_CHECK_MSG(Late.Year > Early.Year, "and what year the world had reached");
	VT_CHECK_MSG(Then.Digest != First.Digest, "and five years of a world moving show up in it");
	VT_CHECK_EQ(First.Regions, Then.Regions);
}

VAELEN_TEST(Frame, AViewOfAWorldWithoutAnEconomyIsStillAView)
{
	// Every source past the map and the people is optional, and a world that has
	// no trade should give a view saying nothing about roads rather than refusing
	// to be looked at. A renderer must work against a half-built world, because
	// for most of this project's life that is what there was.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	ViewSources Bare;
	Bare.Types = W.Ages.Types();
	Bare.Persons = W.Persons;
	WorldView V;
	TakeView(W.Instance, Bare, V);
	const ViewStats S = MeasureView(V);
	uint32 Roads = 0;
	uint32 Bound = 0;
	for (const RegionView& R : V.Regions)
	{
		Roads += R.Roads;
		Bound += R.Bound;
	}
	VAELEN_LOG_INFO(LogView, "told only about the map and the people: %u regions, %u people, %u roads, %u bound",
					S.Regions, V.People, Roads, Bound);
	VT_CHECK_MSG(S.Regions > 0, "the ground is there");
	VT_CHECK_MSG(V.People > 0, "and the people are");
	VT_CHECK_MSG(Roads == 0, "and it says nothing about roads rather than guessing");
	VT_CHECK_MSG(Bound == 0, "nor about who is free");
}
