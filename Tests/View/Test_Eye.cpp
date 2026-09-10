// VAELEN - Tests/View
// Phase 13.03: level of detail for the eye.
//
// The claim: how finely the world THINKS and how finely it is SHOWN are two
// different questions, and this file proves they give different answers on the
// same world at the same moment. A region simulated person by person can be
// off-screen and drawn not at all; a region the simulation touches once a year
// can be directly under the eye. If those ever agreed, 01.03's SimLod would
// have been enough and this task would not exist.
//
// STATUS: PROTOTYPE (Phase 13)

#include "Vaelen/View/Eye.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogEye);

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

VAELEN_TEST(Eye, WhatTheWorldThinksAboutAndWhatIsDrawnAreDifferentQuestions)
{
	// The whole point of the task, shown on one world at one moment: a region
	// the simulation runs person by person, left out of the frame because the
	// eye is elsewhere; and a region the simulation only counts, drawn because
	// the eye is on it.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Busy = W.Busiest();
	VT_REQUIRE(Busy != 0);
	VT_REQUIRE(RequestDetail(W.Instance, W.Lod, Busy));
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Busy, W.Instance.Now()) > 0);
	W.Instance.TickMany(TicksPerYear);

	WorldGen::RegionGraphCache Ways;
	WorldView Whole;
	TakeView(W.Instance, W.Sources(), Whole);
	const RegionView* Simulated = RegionIn(Whole, Busy);
	VT_REQUIRE(Simulated != nullptr);
	VT_CHECK_MSG(Simulated->Detailed == 1u, "the world is simulating that region person by person");

	// Somewhere far from it to look from: the furthest region the borders reach.
	uint32 Elsewhere = 0;
	uint32 Furthest = 0;
	for (const RegionView& R : Whole.Regions)
	{
		const uint32 D = BordersBetween(W.Instance, W.Ages.Types(), Ways, Busy, R.Index);
		if (D != Unreached && D > Furthest)
		{
			Furthest = D;
			Elsewhere = R.Index;
		}
	}
	VT_REQUIRE(Elsewhere != 0);
	VT_REQUIRE(Furthest >= 2);

	Eye Looking;
	Looking.Region = Elsewhere;
	Looking.Reach = 1;
	WorldView Framed;
	TakeViewFor(W.Instance, W.Sources(), Looking, Ways, Framed);
	const EyeStats S = MeasureEye(Framed, static_cast<uint32>(Whole.Regions.size()));

	const RegionView* Drawn = RegionIn(Framed, Elsewhere);
	VAELEN_LOG_INFO(LogEye,
					"looking from region %u (%u borders from the simulated one): %u regions drawn of %u, %u near, "
					"%u far, %u left out; %u bytes against %u",
					Elsewhere, Furthest, S.Seen, static_cast<uint32>(Whole.Regions.size()), S.Near_, S.Far_, S.Unseen,
					S.Bytes, static_cast<uint32>(sizeof(WorldView) + Whole.Regions.size() * sizeof(RegionView)));
	VT_REQUIRE(Drawn != nullptr);
	VT_CHECK_MSG(Drawn->Grain_ == static_cast<uint32>(Grain::Near), "the region under the eye is drawn finely");
	VT_CHECK_MSG(Drawn->Detailed == 0u,
				 "and the world is NOT simulating it person by person - shown finely, thought about coarsely");
	VT_CHECK_MSG(RegionIn(Framed, Busy) == nullptr,
				 "while the region the world simulates person by person is not in the frame at all");
	VT_CHECK_MSG(S.Unseen > 0, "most of the world is not being looked at");
	VT_CHECK_MSG(S.Bytes < sizeof(WorldView) + Whole.Regions.size() * sizeof(RegionView),
				 "and a frame of what is on screen is smaller than a frame of everything");
}

VAELEN_TEST(Eye, ReachDecidesHowMuchIsSeen)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Middle = W.Busiest();
	VT_REQUIRE(Middle != 0);
	WorldGen::RegionGraphCache Ways;
	WorldView Whole;
	TakeView(W.Instance, W.Sources(), Whole);
	const uint32 All = static_cast<uint32>(Whole.Regions.size());

	uint32 Last = 0;
	for (uint32 Reach = 0; Reach <= 4; ++Reach)
	{
		Eye At;
		At.Region = Middle;
		At.Reach = Reach;
		WorldView V;
		TakeViewFor(W.Instance, W.Sources(), At, Ways, V);
		const EyeStats S = MeasureEye(V, All);
		VAELEN_LOG_INFO(LogEye, "reach %u: %u regions (%u near, %u far), %u people in the frame", Reach, S.Seen,
						S.Near_, S.Far_, V.People);
		VT_CHECK_MSG(S.Near_ == 1u, "exactly one region is the one being looked at, at any reach");
		VT_CHECK_MSG(S.Seen >= Last, "and reaching further never shows less");
		Last = S.Seen;
	}
	Eye Nowhere;
	WorldView Everything;
	TakeViewFor(W.Instance, W.Sources(), Nowhere, Ways, Everything);
	VT_CHECK_MSG(Everything.Regions.size() == Whole.Regions.size(),
				 "and an eye looking nowhere in particular gets the whole world, as 13.01 gave it");
	VT_CHECK_EQ(Everything.People, Whole.People);
}

VAELEN_TEST(Eye, ABudgetKeepsTheNearestGround)
{
	// A frame has a size a renderer can afford. What survives the budget must be
	// the ground nearest the eye, and it must be the SAME ground every frame -
	// a budget that cut differently between two frames would flicker.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Middle = W.Busiest();
	VT_REQUIRE(Middle != 0);
	WorldGen::RegionGraphCache Ways;

	Eye At;
	At.Region = Middle;
	At.Reach = 6;
	At.Most = 5;
	WorldView V;
	TakeViewFor(W.Instance, W.Sources(), At, Ways, V);
	WorldView Again;
	TakeViewFor(W.Instance, W.Sources(), At, Ways, Again);

	uint32 Worst = 0;
	for (const RegionView& R : V.Regions)
	{
		const uint32 D = BordersBetween(W.Instance, W.Ages.Types(), Ways, Middle, R.Index);
		Worst = D != Unreached && D > Worst ? D : Worst;
	}
	// Nothing outside the budget may be nearer than the worst thing inside it.
	Eye Unbudgeted = At;
	Unbudgeted.Most = 0;
	WorldView Wide;
	TakeViewFor(W.Instance, W.Sources(), Unbudgeted, Ways, Wide);
	uint32 NearerAndDropped = 0;
	for (const RegionView& R : Wide.Regions)
	{
		if (RegionIn(V, R.Index) != nullptr)
		{
			continue;
		}
		const uint32 D = BordersBetween(W.Instance, W.Ages.Types(), Ways, Middle, R.Index);
		NearerAndDropped += D != Unreached && D < Worst ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogEye, "a budget of 5 out of %zu in reach: furthest kept is %u borders, %u nearer ones dropped",
					Wide.Regions.size(), Worst, NearerAndDropped);
	VT_CHECK_EQ(V.Regions.size(), static_cast<usize>(5));
	VT_CHECK_MSG(NearerAndDropped == 0, "nothing nearer than what was kept was thrown away");
	VT_CHECK_MSG(MeasureEye(V, 0).Digest == MeasureEye(Again, 0).Digest,
				 "and the same eye on the same world cuts the same way twice");
	bool Ordered = true;
	for (usize i = 1; i < V.Regions.size(); ++i)
	{
		Ordered = Ordered && V.Regions[i - 1].Index < V.Regions[i].Index;
	}
	VT_CHECK_MSG(Ordered, "a budgeted frame is still in index order");
}

VAELEN_TEST(Eye, LookingChangesNothing)
{
	// Two people looking from two places get two frames, and the world is still
	// one world. The eye is an input to the view and never a property of the
	// world, and this is where that is checked rather than asserted.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Instance.TickMany(TicksPerYear);
	const Hash64 Before = ComputeStateDigest(W.Instance);
	const Hash64 LogBefore = W.Instance.Log().Digest();

	WorldGen::RegionGraphCache Ways;
	WorldView Whole;
	TakeView(W.Instance, W.Sources(), Whole);
	VT_REQUIRE(Whole.Regions.size() > 3);

	Eye Here;
	Here.Region = Whole.Regions.front().Index;
	Here.Reach = 2;
	Eye There;
	There.Region = Whole.Regions.back().Index;
	There.Reach = 2;
	WorldView A;
	WorldView B;
	TakeViewFor(W.Instance, W.Sources(), Here, Ways, A);
	TakeViewFor(W.Instance, W.Sources(), There, Ways, B);

	VAELEN_LOG_INFO(LogEye, "two eyes on one world: %zu regions from %u, %zu from %u", A.Regions.size(), Here.Region,
					B.Regions.size(), There.Region);
	VT_CHECK_MSG(MeasureEye(A, 0).Digest != MeasureEye(B, 0).Digest, "two places to look give two different frames");
	VT_CHECK_MSG(ComputeStateDigest(W.Instance) == Before, "and the world is exactly the world it was");
	VT_CHECK_MSG(W.Instance.Log().Digest() == LogBefore, "with nothing written into its history");
}
