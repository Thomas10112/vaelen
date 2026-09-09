// VAELEN - Tests/Gameplay
// Phase 12.06: what the world does about a name.
//
// Twelve phases have produced a reputation that costs nobody anything. This
// file exists to show that it now does - and to show the two halves of that
// separately, because each is a control pair and neither is worth anything
// asserted.
//
// One: somebody earns a bad name. Until 12.06 nobody in AELVOR had ever wronged
// anybody, because 12.01 issued only Speak and Give. Hunger is what makes a
// thief, and hunger is unequal where a die roll is not.
//
// Two: a place acts on the name it carries. The worst-named standing in it are
// bound; the best-named among the bound are let go.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Gameplay/Judgement.h"
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
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/BondState.h"
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
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogJudge);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// The whole of what a travelling name needs: people who act (12.01), people
	/// who think something of each other (12.02), and roads (06.04).
	struct Run
	{
		explicit Run(uint64 Seed, bool Bold = true, JudgementRules InJudge = JudgementRules{},
					 LivingRules InLive = LivingRules{}, ProductionRules InLand = ProductionRules{})
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
			Bondage = BondageTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, InLand);
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, InLand,
												  MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Trade,
												  InLand, MarketRules{}, TradeRules{});
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			// 12.01 and 12.02, as they were built.
			Live = LivingTypes::Declare(Instance);
			Acts = std::make_unique<Player::Doings>(Ages.Types(), Persons, Families, Needs, Economy_,
													Player::DoingRules{});
			Lives_ = std::make_unique<LivingSystem>(Instance, Ages.Types(), Persons, Live, InLive);
			Lives_->ObserveDoing(Acts.get());
			// The one difference between the two worlds of the first test: with
			// this, a bold person sometimes takes; without it, nobody has the
			// nerve and 12.01 is exactly what it shipped as.
			if (Bold)
			{
				Lives_->ObserveTraits(Traits.Traits);
			}
			Names = ReputeTypes::Declare(Instance);
			Talk = std::make_unique<ReputeSystem>(Instance, Persons, Names, ReputeRules{});
			Talk->RunAfter("Living");
			// 12.05 and 12.06.
			Fame_ = FameTypes::Declare(Instance);
			Told = std::make_unique<FameSystem>(Instance, Ages.Types(), Persons, Names, Trade, Fame_, FameRules{});
			Told->RunAfter("Trade"); // a name goes out after the year's carrying, not before it
			Judge = std::make_unique<JudgementSystem>(Instance, Ages.Types(), Persons, Bondage, Fame_, InJudge);
			Judge->RunAfter("Fame");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Systems().Add(Lives_.get());
			Instance.Systems().Add(Talk.get());
			Instance.Systems().Add(Told.get());
			Instance.Systems().Add(Judge.get());
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
		FameStats Fame() const { return MeasureFame(Instance, Fame_); }
		JudgementStats Judged() const { return MeasureJudgement(Instance, Persons, Bondage); }
		ReputeStats Reputes() const { return MeasureRepute(Instance, Persons, Names); }
		Player::DoingStats Verbs() const { return Player::MeasureDoings(Instance); }

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
		BondageTypes Bondage;
		std::unique_ptr<JudgementSystem> Judge;
	};

	/// Six years of a lively region.
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

VAELEN_TEST(Judgement, CharacterMakesAThief)
{
	// The control pair. Two worlds, the same seed, the same map, the same
	// people, the same acts - except that in one of them the acting system can
	// see 04.05's characters. Everything that differs between them is nerve.
	//
	// Why character rather than want, which was tried first and measured out.
	// 06.02 works out ONE ration for a whole region and 04.04 moves every
	// person's Food by it, so everybody in a place is exactly as fed as
	// everybody else: all 1428 people of the best-fed region sit at 255 of 255,
	// and a land lean enough to push them under the hunger line leaves 5 of them
	// alive. There is no window where some are hungry and others are not.
	// Boldness has one: 59 of the 1428 are over the line, and it is heritable.
	auto Live = [&](bool Bold)
	{
		Run W(AelvorSeed, Bold);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		VT_CHECK(Settle(W) != 0);
		return std::make_pair(W.Verbs(), W.Reputes());
	};
	const std::pair<Player::DoingStats, ReputeStats> Meek = Live(false);
	const std::pair<Player::DoingStats, ReputeStats> Bold = Live(true);

	VAELEN_LOG_INFO(LogJudge,
					"blind to character: %u acts, %u given, %u taken; best %d worst %d. Seeing it: %u acts, %u given, "
					"%u taken; best %d worst %d",
					Meek.first.Acts, Meek.first.Gave, Meek.first.Took, Meek.second.Best, Meek.second.Worst,
					Bold.first.Acts, Bold.first.Gave, Bold.first.Took, Bold.second.Best, Bold.second.Worst);
	VT_CHECK_MSG(Meek.first.Took == 0, "nobody had ever taken anything from anybody in twelve phases");
	VT_CHECK_MSG(Meek.second.Worst == 0, "so nobody had ever thought ill of anybody either");
	VT_CHECK_MSG(Bold.first.Took > 0, "the bold take");
	VT_CHECK_MSG(Bold.second.Worst < 0, "and are thought ill of for it, which is the phase's whole missing half");
	VT_CHECK_MSG(Bold.second.Best > 0, "while the world still thinks well of the people who give");
}

VAELEN_TEST(Judgement, AReputeCostsSomebodySomething)
{
	// The second control pair, and the one the phase was for. Same world twice;
	// in one of them no name is ever bad enough to answer for. The people are
	// just as badly named in both.
	auto Live = [&](int32 BindUnder)
	{
		JudgementRules R;
		R.BindUnder = BindUnder;
		Run W(AelvorSeed, true, R);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		VT_CHECK(Settle(W) != 0);
		return std::make_pair(W.Judged(), W.Fame());
	};
	const std::pair<JudgementStats, FameStats> Never = Live(-1000000);
	const std::pair<JudgementStats, FameStats> Answered = Live(JudgementRules{}.BindUnder);

	VAELEN_LOG_INFO(
		LogJudge,
		"a world that forgives everything: %u condemned, %u pardoned, %u bound now, over %u names. "
		"A world that does not: %u condemned, %u pardoned, %u bound now, worst name bound %d, over %u names",
		Never.first.Condemned, Never.first.Pardoned, Never.first.BoundNow, Never.second.Names, Answered.first.Condemned,
		Answered.first.Pardoned, Answered.first.BoundNow, Answered.first.WorstBound, Answered.second.Names);
	VT_CHECK_MSG(Never.first.Condemned == 0, "nobody answers for a name in a world with no line to cross");
	VT_CHECK_MSG(Never.first.BoundNow == 0, "and nobody is bound by judgement, because there was none");
	VT_CHECK_MSG(Answered.first.Condemned > 0, "somebody answers for theirs in a world that has one");
	VT_CHECK_MSG(Answered.first.BoundNow > 0, "and is still bound for it");
	VT_CHECK_MSG(Answered.first.WorstBound <= JudgementRules{}.BindUnder,
				 "everybody bound was at or under the line, which is what the line means");
	VT_CHECK_MSG(Never.second.Names == Answered.second.Names,
				 "the same names are said in both worlds - what differs is only what was done about them");
}

VAELEN_TEST(Judgement, OnlyThePeopleAPlaceSpeaksOfAreJudged)
{
	// The consequence of 12.05's shape rather than a rule invented here. A place
	// carries eight names. Somebody whose name is not among them is not spared
	// by any clause - they are simply never spoken of, and nothing happens to
	// them however badly they have behaved.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);

	uint32 Condemned = 0;
	uint32 NamedWhenCondemned = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(CondemnedEvent))
		{
			continue;
		}
		++Condemned;
		// The event carries what the place said of them, which only a place that
		// was saying something could have produced.
		NamedWhenCondemned += E.Get<FamePayload>().Said != 0 ? 1u : 0u;
	}

	// And the ones who wronged people but were never carried: badly-named by
	// their own record, untouched by anybody.
	uint32 Wrongdoers = 0;
	uint32 UnjudgedWrongdoers = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				const PersonRepute* R = W.Instance.Components().GetPool(W.Names.Repute).TryGet(H);
				if (R == nullptr || R->Wrongs == 0 || P.State != static_cast<uint8>(LifeState::Alive))
				{
					return;
				}
				++Wrongdoers;
				const BondState* B = W.Instance.Components().GetPool(W.Bondage.Bond).TryGet(H);
				UnjudgedWrongdoers += B == nullptr || B->Entry != static_cast<uint8>(BondEntry::Judgement) ? 1u : 0u;
			});

	VAELEN_LOG_INFO(LogJudge,
					"%u condemned, %u of them on a name the place was actually saying. %u people have wronged "
					"somebody and %u of those were never judged at all",
					Condemned, NamedWhenCondemned, Wrongdoers, UnjudgedWrongdoers);
	VT_CHECK_MSG(Condemned > 0, "somebody was condemned");
	VT_CHECK_MSG(NamedWhenCondemned == Condemned, "every one of them on a name their place was carrying");
	VT_CHECK_MSG(Wrongdoers > Condemned, "far more people have wronged somebody than were ever answered for it");
	VT_CHECK_MSG(UnjudgedWrongdoers > 0, "and the difference is people nobody speaks of, not people the rules spared");
}

VAELEN_TEST(Judgement, APlaceCannotBindSomebodyStandingElsewhere)
{
	// A name travels; a body does not. Every place but one in this world has
	// heard names and has nobody in it at all, so if a place could act on a name
	// rather than on a person, the condemnations would be scattered over the map.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);
	const FameStats F = W.Fame();
	VT_REQUIRE(F.Places > 1);

	uint32 AtHome = 0;
	uint32 Abroad = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(CondemnedEvent) && !E.Is(PardonedEvent))
		{
			continue;
		}
		(E.Get<FamePayload>().Region == Home ? AtHome : Abroad) += 1u;
	}
	VAELEN_LOG_INFO(LogJudge, "%u places carry names, %u of them with nobody in them; %u judgements at home, %u away",
					F.Places, F.Places - 1u, AtHome, Abroad);
	VT_CHECK_MSG(AtHome > 0, "the place the people stand in judges them");
	VT_CHECK_MSG(Abroad == 0, "and the seven that have only heard of them do nothing at all");
}

VAELEN_TEST(Judgement, TheSameWorldJudgesTheSamePeople)
{
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(A) != 0);
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(B) != 0);
	const JudgementStats X = A.Judged();
	const JudgementStats Y = B.Judged();
	VAELEN_LOG_INFO(LogJudge, "two runs of one seed: %u/%u condemned, digests %016llx and %016llx", X.Condemned,
					Y.Condemned, static_cast<unsigned long long>(X.Digest), static_cast<unsigned long long>(Y.Digest));
	VT_CHECK_EQ(X.Condemned, Y.Condemned);
	VT_CHECK_EQ(X.BoundNow, Y.BoundNow);
	VT_CHECK_EQ(X.Digest, Y.Digest);
}
