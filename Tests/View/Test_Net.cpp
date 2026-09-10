// VAELEN - Tests/View
// Phase 13.08a: what the world has BUILT, as a renderer needs it.
//
// A frame says a region is touched by three routes. It does not say WHICH three,
// so a map drawn from a frame alone has towns on it and no lines between them.
// This file is about the claim that the view now carries the network itself -
// and, as in 13.01 and 13.07a, that it carries it as numbers with no way back
// into the world.
//
// STATUS: PROTOTYPE (Phase 13)

#include "Vaelen/View/Net.h"

#include "Vaelen/Colony/Mining.h"
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
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"
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
using namespace Vaelen::Colony;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::View;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogNet);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// A world with an economy and a colony in it: the smallest thing that has
	/// a network worth drawing.
	struct Built
	{
		explicit Built(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Norms_ = NormTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Pit = ColonyTypes::Declare(Instance);

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
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Rock =
				std::make_unique<MiningSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Pit, MiningRules{});
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
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
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Systems().Add(Rock.get());
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
			S.HasTrade = true;
			S.Trade = Trade;
			S.HasColony = true;
			S.Colony_ = Pit;
			return S;
		}
		/// The region with the most people: somewhere a colony can plausibly sit.
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
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		ColonyTypes Pit;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
		std::unique_ptr<MiningSystem> Rock;
	};
} // namespace

VAELEN_TEST(Net, TheNetworkCarriesNoWayBackIntoTheWorld)
{
	static_assert(std::is_trivially_copyable<RouteView>::value,
				  "a RouteView must be copyable by memcpy: no pointer, no handle, no vtable");
	static_assert(std::is_trivially_copyable<ColonyView>::value, "and so must a ColonyView");
	static_assert(std::is_standard_layout<RouteView>::value, "laid out plainly enough to hand to a GPU");
	VT_CHECK_MSG(sizeof(RouteView) == sizeof(uint64) + 6 * sizeof(uint32),
				 "and RouteView has no padding, because MeasureNetView hashes it");
	VT_CHECK_MSG(sizeof(ColonyView) == 4 * sizeof(uint32), "and neither has ColonyView");

	Built W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Built::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Pit, Where));
	W.Ages.Run(60);

	NetView V;
	TakeNetView(W.Instance, W.Sources(), V);
	const NetStats S = MeasureNetView(V);
	VAELEN_LOG_INFO(LogNet, "year %u: %u routes (%u open), %u colonies, %u hands, %u bytes", V.Year, S.Routes, S.Open,
					S.Colonies, S.Hands, S.Bytes);
	VT_CHECK_MSG(S.Routes > 0, "the world has built roads");
	VT_CHECK_MSG(S.Open > 0, "and some of them are open");
	VT_CHECK_EQ(S.Open, V.Open);
	VT_CHECK_EQ(S.Colonies, 1u);

	// The routes come out in index order, every time.
	bool Ordered = true;
	for (usize i = 1; i < V.Routes.size(); ++i)
	{
		Ordered = Ordered && V.Routes[i - 1].Index < V.Routes[i].Index;
	}
	VT_CHECK_MSG(Ordered, "the routes are in index order and never in pool order");

	// A route is found from either end, because the caller should not have to
	// know which region index the kernel happened to put first.
	VT_REQUIRE(!V.Routes.empty());
	const RouteView First = V.Routes.front();
	const RouteView* Forward = RouteBetween(V, First.From, First.To);
	const RouteView* Backward = RouteBetween(V, First.To, First.From);
	VT_CHECK_MSG(Forward != nullptr && Backward == Forward, "a road is the same road read from either end");
	VT_CHECK_MSG(RouteBetween(V, 0, 0) == nullptr, "and there is no road between nowhere and nowhere");

	const ColonyView* Pit = ColonyIn(V, Where);
	VT_REQUIRE(Pit != nullptr);
	VT_CHECK_EQ(Pit->Region, Where);
	VT_CHECK_MSG(ColonyIn(V, Where + 100000u) == nullptr, "and no colony where there is none");
}

VAELEN_TEST(Net, TheNetworkMatchesTheWorldItWasReadFrom)
{
	// Every route in the view against the route in the world, keyed on INDEX and
	// not on the pair of regions - because a pair does not identify a route. The
	// first draft of this test looked routes up by (From, To) and failed on 96 of
	// 254 of them, which is how ADR-0120 was found: 06.04 builds a second entity
	// when it reopens a road it closed earlier in the same tick.
	Built W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Built::Square(96), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Pit, Where));
	RequestDetail(W.Instance, W.Lod, Where);
	W.Ages.Run(40);

	NetView V;
	TakeNetView(W.Instance, W.Sources(), V);

	uint32 InWorld = 0;
	uint32 WrongRoutes = 0;
	W.Instance.Components()
		.GetPool(W.Trade.Route)
		.ForEach(
			[&](EntityHandle, const RouteInfo& R)
			{
				if (R.Index == 0)
				{
					return;
				}
				++InWorld;
				const RouteView* Seen = RouteOf(V, R.Index);
				if (Seen == nullptr || Seen->From != R.From || Seen->To != R.To || Seen->Carried != R.Carried ||
					Seen->Idle != R.Idle || Seen->Openings != R.Openings || Seen->Open != (R.Closed == 0 ? 1u : 0u))
				{
					++WrongRoutes;
				}
			});
	VT_CHECK_MSG(WrongRoutes == 0, "every route of the view is the route the world has");
	VT_CHECK_EQ(static_cast<uint32>(V.Routes.size()), InWorld);

	uint32 Colonies = 0;
	uint32 WrongColonies = 0;
	W.Instance.Components()
		.GetPool(W.Pit.Colony)
		.ForEach(
			[&](EntityHandle, const ColonyInfo& C)
			{
				if (C.Region == 0)
				{
					return;
				}
				++Colonies;
				const ColonyView* Seen = ColonyIn(V, C.Region);
				if (Seen == nullptr || Seen->Hands != C.Hands || Seen->Lifted != C.Lifted)
				{
					++WrongColonies;
				}
			});
	VT_CHECK_MSG(WrongColonies == 0, "and every colony is the colony the world has");
	VT_CHECK_EQ(static_cast<uint32>(V.Colonies.size()), Colonies);

	// The colony is worked, not merely founded: hands on the rock and ore out of
	// it. A view that said "one colony, nobody in it" would pass every check
	// above and describe nothing.
	const ColonyView* Pit = ColonyIn(V, Where);
	VT_REQUIRE(Pit != nullptr);
	VAELEN_LOG_INFO(LogNet, "colony on region %u: %u hands, %u units lifted", Pit->Region, Pit->Hands, Pit->Lifted);
	VT_CHECK_MSG(Pit->Hands > 0, "somebody is on the rock");
	VT_CHECK_MSG(Pit->Lifted > 0, "and ore has come out of it");
}

VAELEN_TEST(Net, TheViewReportsTheDuplicateRoadsRatherThanTidyingThemAway)
{
	// ADR-0120, held where it will be noticed if it is ever fixed. 06.04 can put
	// two route entities on one pair of regions, and the view's job is to say so
	// - a view that quietly kept one of them would make the defect invisible
	// again and would lie about what the world holds.
	Built W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Built::Square(96), 300));
	W.Ages.Run(40);

	NetView V;
	TakeNetView(W.Instance, W.Sources(), V);
	VT_REQUIRE(!V.Routes.empty());

	std::vector<RouteView> ByPair = V.Routes;
	std::sort(ByPair.begin(), ByPair.end(),
			  [](const RouteView& X, const RouteView& Y) { return X.From != Y.From ? X.From < Y.From : X.To < Y.To; });
	uint32 Doubled = 0;
	uint32 BothOpen = 0;
	for (usize i = 1; i < ByPair.size(); ++i)
	{
		if (ByPair[i - 1].From == ByPair[i].From && ByPair[i - 1].To == ByPair[i].To)
		{
			++Doubled;
			BothOpen += (ByPair[i - 1].Open != 0 && ByPair[i].Open != 0) ? 1u : 0u;
		}
	}
	VAELEN_LOG_INFO(LogNet, "%u of %u routes stand on a pair that already had one (%u with both open)", Doubled,
					static_cast<uint32>(V.Routes.size()), BothOpen);

	// Two OPEN roads on one pair would be a different and worse thing: goods
	// would cross twice. 06.04's own IsOpen prevents it, and this is where that
	// stays true. The kernel's RouteStats::Twice counts the same thing and has
	// never fired, which is exactly why the closed twins went unseen.
	VT_CHECK_MSG(BothOpen == 0, "no pair of regions carries two OPEN roads");

	// Every route is reachable by its own index, duplicates included: the index
	// is the identity, and the view must not lose one.
	uint32 Missing = 0;
	for (const RouteView& R : V.Routes)
	{
		Missing += RouteOf(V, R.Index) == nullptr ? 1u : 0u;
	}
	VT_CHECK_MSG(Missing == 0, "and every route, twin or not, is found by its index");

	// And RouteBetween prefers the road that is actually there.
	for (const RouteView& R : V.Routes)
	{
		if (R.Open == 0)
		{
			continue;
		}
		const RouteView* Best = RouteBetween(V, R.To, R.From);
		VT_REQUIRE(Best != nullptr);
		VT_CHECK_MSG(Best->Open != 0, "an open road is never shadowed by its closed twin");
	}
}

VAELEN_TEST(Net, TheNetworkOutlivesTheWorldItCameFrom)
{
	NetView V;
	Hash64 Before = 0;
	uint32 Routes = 0;
	{
		std::unique_ptr<Built> W = std::make_unique<Built>(AelvorSeed);
		VT_REQUIRE(W->Ages.Generate(Built::Square(96), 300));
		const uint32 Where = W->Busiest();
		VT_REQUIRE(Where != 0);
		VT_REQUIRE(FoundColony(W->Instance, W->Ages.Types(), W->Pit, Where));
		W->Ages.Run(40);
		TakeNetView(W->Instance, W->Sources(), V);
		Before = MeasureNetView(V).Digest;
		Routes = static_cast<uint32>(V.Routes.size());
		VT_REQUIRE(Routes > 0);
	} // the world is gone here

	VT_CHECK_EQ(MeasureNetView(V).Digest, Before);
	VT_CHECK_EQ(static_cast<uint32>(V.Routes.size()), Routes);
	VT_CHECK_MSG(RouteBetween(V, V.Routes.front().From, V.Routes.front().To) != nullptr,
				 "and the roads are still there with nothing left to have built them");
}

VAELEN_TEST(Net, TheSameSeedGivesTheSameNetwork)
{
	Built A(AelvorSeed);
	Built B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Built::Square(64), 300));
	VT_REQUIRE(B.Ages.Generate(Built::Square(64), 300));
	const uint32 WhereA = A.Busiest();
	const uint32 WhereB = B.Busiest();
	VT_CHECK_EQ(WhereA, WhereB);
	VT_REQUIRE(FoundColony(A.Instance, A.Ages.Types(), A.Pit, WhereA));
	VT_REQUIRE(FoundColony(B.Instance, B.Ages.Types(), B.Pit, WhereB));
	A.Ages.Run(30);
	B.Ages.Run(30);

	NetView Va;
	NetView Vb;
	TakeNetView(A.Instance, A.Sources(), Va);
	TakeNetView(B.Instance, B.Sources(), Vb);
	VT_CHECK_EQ(MeasureNetView(Va).Digest, MeasureNetView(Vb).Digest);

	// And taken twice from one world it does not move.
	NetView Again;
	TakeNetView(A.Instance, A.Sources(), Again);
	VT_CHECK_EQ(MeasureNetView(Again).Digest, MeasureNetView(Va).Digest);
}

VAELEN_TEST(Net, AWorldWithNoEconomySaysSoInsteadOfGuessing)
{
	// The sources decide. A caller that has no trade types to give gets an empty
	// network rather than a pool read through a component type nobody declared.
	Built W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Built::Square(64), 300));
	W.Ages.Run(20);

	ViewSources Bare;
	Bare.Types = W.Ages.Types();
	Bare.Persons = W.Persons;
	NetView V;
	TakeNetView(W.Instance, Bare, V);
	const NetStats S = MeasureNetView(V);
	VT_CHECK_MSG(V.Routes.empty(), "no trade in the sources, no roads in the view");
	VT_CHECK_MSG(V.Colonies.empty(), "and no colony either");
	VT_CHECK_EQ(S.Routes, 0u);
	VT_CHECK_EQ(S.Colonies, 0u);
	VT_CHECK_EQ(V.Open, 0u);
	VT_CHECK_MSG(RouteBetween(V, 1, 2) == nullptr, "and nothing is found between two regions");
	VT_CHECK_MSG(ColonyIn(V, 1) == nullptr, "nor a colony on one");

	// The year still comes through, because a world with no economy still has a
	// clock and a renderer still has to label the frame.
	VT_CHECK_MSG(V.Year > 0, "the frame is still dated");
}
