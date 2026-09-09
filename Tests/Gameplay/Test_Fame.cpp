// VAELEN - Tests/Gameplay
// Phase 12.05: a name that travels.
//
// 12.02 named its own limit out loud: `MostThoughtOf` is eight, so a person is
// thought of by eight people at a time and the ninth evicts the oldest. That is
// a village's reputation, and this file exists to show what replaces it - not a
// bigger number, a different holder. A name that has travelled is held by the
// PLACE, and it moves on the roads of 06.04 rather than across the map.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Gameplay/Living.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Player/Doings.h"
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
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::Gameplay;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogFame);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// The whole of what a travelling name needs: people who act (12.01), people
	/// who think something of each other (12.02), and roads (06.04).
	struct Run
	{
		explicit Run(uint64 Seed, bool WithRoads = true, FameRules InFame = FameRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
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
			// 12.01 and 12.02, as they were built.
			Live = LivingTypes::Declare(Instance);
			Acts = std::make_unique<Player::Doings>(Ages.Types(), Persons, Families, Needs, Economy_,
													Player::DoingRules{});
			Lives_ = std::make_unique<LivingSystem>(Instance, Ages.Types(), Persons, Live, LivingRules{});
			Lives_->ObserveDoing(Acts.get());
			Names = ReputeTypes::Declare(Instance);
			Talk = std::make_unique<ReputeSystem>(Instance, Persons, Names, ReputeRules{});
			Talk->RunAfter("Living");
			// 12.05.
			Fame_ = FameTypes::Declare(Instance);
			Told = std::make_unique<FameSystem>(Instance, Ages.Types(), Persons, Names, Trade, Fame_, InFame);
			if (WithRoads)
			{
				Told->RunAfter("Trade"); // a name goes out after the year's carrying, not before it
			}
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			if (WithRoads)
			{
				Instance.Systems().Add(Roads.get());
			}
			Instance.Systems().Add(Lives_.get());
			Instance.Systems().Add(Talk.get());
			Instance.Systems().Add(Told.get());
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
		uint32 Alive(uint32 Region) const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach([&](EntityHandle, const PersonInfo& P)
						 { N += P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) ? 1u : 0u; });
			return N;
		}
		bool Promote(uint32 Region)
		{
			RequestDetail(Instance, Lod, Region);
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0 ||
				   Alive(Region) > 0;
		}
		FameStats Stats() const { return MeasureFame(Instance, Fame_); }

		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
		LivingTypes Live;
		std::unique_ptr<Player::Doings> Acts;
		std::unique_ptr<LivingSystem> Lives_;
		ReputeTypes Names;
		std::unique_ptr<ReputeSystem> Talk;
		FameTypes Fame_;
		std::unique_ptr<FameSystem> Told;
	};

	/// Six years of a lively region: one for the people to make names for each
	/// other, and the rest for those names to cross roads.
	uint32 Settle(Run& W)
	{
		const uint32 Where = W.Busiest();
		if (Where == 0 || !W.Promote(Where) || !MakeLively(W.Instance, W.Ages.Types(), W.Live, Where))
		{
			return 0;
		}
		W.Instance.TickMany(TicksPerYear * 6);
		return Where;
	}
} // namespace

VAELEN_TEST(Fame, ANameReachesGroundItsPersonHasNeverStoodOn)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);

	const FameStats S = W.Stats();
	const TradeStats T = MeasureTrade(W.Instance, W.Ages.Types(), W.Trade, TradeRules{});

	// Everybody alive anywhere, so "has never stood there" can be checked rather
	// than assumed: only one region is simulated person by person, so anybody
	// with a name is standing in Home.
	uint32 Elsewhere = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach([&](EntityHandle, const PersonInfo& P)
				 { Elsewhere += P.Region != Home && P.State == static_cast<uint8>(LifeState::Alive) ? 1u : 0u; });

	VAELEN_LOG_INFO(LogFame,
					"six years out of region %u: %u routes open, %u places carry %u names, %u of them from abroad, "
					"furthest %u road(s); %u people live anywhere else",
					Home, T.RoutesOpen, S.Places, S.Names, S.Abroad, S.Furthest, Elsewhere);
	VT_CHECK_MSG(T.RoutesOpen > 0, "there are roads to travel on at all");
	VT_CHECK_MSG(S.Names > 0, "somebody is spoken of somewhere");
	VT_CHECK_MSG(S.Places > 1, "and in more than the one place they live");
	VT_CHECK_MSG(S.Abroad > 0, "a name has crossed at least one road, which nothing before 12.05 could do");
	VT_CHECK_MSG(Elsewhere == 0, "and it did so without a single person leaving the region");
	VT_CHECK_MSG(S.Furthest <= FameRules{}.MostHops, "no name goes further than the world lets it");
}

VAELEN_TEST(Fame, ItIsThinnerFurtherFromHome)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);

	// For every name held in more than one place, compare what is said of them
	// at home with what is said of them a road away.
	uint32 Compared = 0;
	uint32 Thinner = 0;
	uint32 Louder = 0;
	W.Instance.Components()
		.GetPool(W.Fame_.Names)
		.ForEach(
			[&](EntityHandle, const RegionNames& N)
			{
				for (uint32 k = 0; k < N.Count && k < MostNames; ++k)
				{
					const Fame& Away = N.Who[k];
					if (Away.Hops == 0)
					{
						continue;
					}
					const Fame* AtHome = FameIn(W.Instance, W.Ages.Types(), W.Fame_, Home, Away.Person);
					if (AtHome == nullptr || AtHome->Hops != 0)
					{
						continue;
					}
					++Compared;
					const int32 Here = Away.Said < 0 ? -Away.Said : Away.Said;
					const int32 There = AtHome->Said < 0 ? -AtHome->Said : AtHome->Said;
					Thinner += Here < There ? 1u : 0u;
					Louder += Here > There ? 1u : 0u;
				}
			});
	VAELEN_LOG_INFO(LogFame, "%u name(s) held both at home and abroad: %u thinner away, %u louder away", Compared,
					Thinner, Louder);
	VT_CHECK_MSG(Compared > 0, "some name is held both at home and away, or there is nothing to compare");
	VT_CHECK_MSG(Louder == 0, "no name is louder in the next valley than where it was earned");
	VT_CHECK_MSG(Thinner == Compared, "every one of them is thinner for the distance");
}

VAELEN_TEST(Fame, TheRoadsCarryIt)
{
	// The same world twice, with the roads and without. Everything else is the
	// same map, the same people, the same acts and the same opinions - so what
	// separates the two is only whether there was a road.
	Run With(AelvorSeed);
	VT_REQUIRE(With.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(With) != 0);
	Run Without(AelvorSeed, false);
	VT_REQUIRE(Without.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(Without) != 0);

	const FameStats A = With.Stats();
	const FameStats B = Without.Stats();
	VAELEN_LOG_INFO(LogFame, "with roads: %u places, %u names, %u abroad. Without: %u places, %u names, %u abroad",
					A.Places, A.Names, A.Abroad, B.Places, B.Names, B.Abroad);
	VT_CHECK_MSG(B.Names > 0, "people are still spoken of where they live");
	VT_CHECK_MSG(B.Abroad == 0, "and nowhere else at all, because nothing connects the places");
	VT_CHECK_MSG(B.Places == 1, "one place knows one region's names, and it is the region itself");
	VT_CHECK_MSG(A.Abroad > 0, "the roads are the whole of the difference");
}

VAELEN_TEST(Fame, APlaceKeepsANameWhileItKeepsHearingIt)
{
	// What forgetting is here, and what it is not. There is no gradual fading -
	// ADR-0100 records the three ways of reaching one that all came back empty.
	// A place says the loudest handful it is told about; a name it stops being
	// told, it stops saying, that year.
	//
	// So the two halves of the claim, together: names PERSIST, which is what
	// makes them names rather than a yearly lottery, and names are DROPPED,
	// which is what keeps the handful a handful.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);
	const uint64 Middle = W.Instance.Now();
	W.Instance.TickMany(TicksPerYear * 6);

	uint32 Old = 0;	  ///< names a place has been saying since before the halfway mark
	uint32 Fresh = 0; ///< and names it has picked up since
	uint64 Longest = 0;
	W.Instance.Components()
		.GetPool(W.Fame_.Names)
		.ForEach(
			[&](EntityHandle, const RegionNames& N)
			{
				for (uint32 k = 0; k < N.Count && k < MostNames; ++k)
				{
					(N.Who[k].Since < Middle ? Old : Fresh) += 1u;
					const uint64 Held = W.Instance.Now() - N.Who[k].Since;
					Longest = Held > Longest ? Held : Longest;
				}
			});
	uint32 Reached = 0;
	uint32 Dropped = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Reached += E.Is(NameTravelledEvent) ? 1u : 0u;
		Dropped += E.Is(NameForgottenEvent) ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogFame,
					"twelve years: %u names held since before the sixth, %u picked up since; the longest has been "
					"said in one place for %llu years. %u names reached a place over the world's life, %u were "
					"dropped again",
					Old, Fresh, static_cast<unsigned long long>(Longest / TicksPerYear), Reached, Dropped);
	VT_CHECK_MSG(Old > 0, "some name has outlasted six years in the same place, which nothing built on 12.02's "
						  "eight-slot average ever could");
	VT_CHECK_MSG(Longest >= TicksPerYear * 6, "and the longest-held is at least that old");
	VT_CHECK_MSG(Dropped > 0, "names are dropped when nobody tells them any more");
	VT_CHECK_MSG(Reached > Dropped, "and more are still being said than have been let go");
}

VAELEN_TEST(Fame, TheSameWorldSaysTheSameThings)
{
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(A) != 0);
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(B) != 0);
	const FameStats X = A.Stats();
	const FameStats Y = B.Stats();
	VAELEN_LOG_INFO(LogFame, "two runs of one seed: %u/%u names, digests %016llx and %016llx", X.Names, Y.Names,
					static_cast<unsigned long long>(X.Digest), static_cast<unsigned long long>(Y.Digest));
	VT_CHECK_EQ(X.Names, Y.Names);
	VT_CHECK_EQ(X.Abroad, Y.Abroad);
	VT_CHECK_EQ(X.Digest, Y.Digest);
}
