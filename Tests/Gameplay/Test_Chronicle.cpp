// VAELEN - Tests/Gameplay
// Phase 12.07: gameplay in the chronicle.
//
// The phase's rule is that a person acts on what they believe and the world
// acts on what it has heard. This file is where that stops being a slogan: it
// takes a man bound in a place he has never been spoken well of, and walks the
// reason back - not to what he did, but to the telling that reached the place
// that bound him, and to the telling behind that one.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Gameplay/GameplayHistory.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogChron);

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
			Papers = DocumentTypes::Declare(Instance);
			Charts = MapTypes::Declare(Instance);
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
		GameplayContext Heard() const { return GameplayContext{Persons, Names, Papers, Charts, Fame_}; }
		ChronicleStats Chronicle() const { return MeasureChronicle(Instance, Ages.Types(), Heard()); }
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
		DocumentTypes Papers;
		MapTypes Charts;
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

VAELEN_TEST(Chronicle, EveryActOfBeliefHasASentence)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(W) != 0);
	const ChronicleStats S = W.Chronicle();
	VAELEN_LOG_INFO(LogChron, "%u events of belief in the log, %u of them with a sentence; digest %016llx", S.Events,
					S.Described, static_cast<unsigned long long>(S.Digest));
	VT_CHECK_MSG(S.Events > 0, "the world did something worth chronicling");
	VT_CHECK_MSG(S.Described == S.Events, "and every one of it has a sentence, with nothing falling through");
	VT_CHECK_MSG(S.Digest != 0, "the chronicle is a pure function of the state and the log");
}

VAELEN_TEST(Chronicle, ABondageWalksBackToATelling)
{
	// The claim the whole phase was for. A man is bound; the chronicle is asked
	// why; and the answer is not what he did. It is what the place that bound
	// him had been told, and what the place that told THEM had been told.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);

	uint32 Judgements = 0;
	uint32 WithACause = 0;
	uint32 Longest = 0;
	std::vector<BeliefStep> Deepest;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(CondemnedEvent))
		{
			continue;
		}
		++Judgements;
		std::vector<BeliefStep> Chain;
		WhyBelieved(W.Instance, W.Ages.Types(), W.Heard(), E.Id, Chain);
		WithACause += Chain.size() > 1 ? 1u : 0u;
		if (Chain.size() > Longest)
		{
			Longest = static_cast<uint32>(Chain.size());
			Deepest = Chain;
		}
	}
	VAELEN_LOG_INFO(LogChron, "%u bound by judgement; %u of them on a telling; the longest chain is %u steps",
					Judgements, WithACause, Longest);
	for (const BeliefStep& Step : Deepest)
	{
		VAELEN_LOG_INFO(LogChron, "  year %llu: %s", static_cast<unsigned long long>(Step.Tick / TicksPerYear),
						Step.Text.c_str());
	}
	VT_CHECK_MSG(Judgements > 0, "somebody was bound");
	VT_CHECK_MSG(WithACause > 0, "and the chronicle can say what the place had been told");
	VT_CHECK_MSG(Longest >= 2, "a judgement and the telling behind it, at the least");
	VT_REQUIRE(!Deepest.empty());
	VT_CHECK_MSG(!Deepest.front().Text.empty(), "every step of it is a sentence and not an id");
}

VAELEN_TEST(Chronicle, APlaceCanBeAskedWhatItKnows)
{
	// "What was known, by whom, and when" - answered for a place rather than
	// for the world, because after 12.05 that is where knowing lives.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Home = Settle(W);
	VT_REQUIRE(Home != 0);
	const FameStats F = W.Fame();
	VT_REQUIRE(F.Names > 0);

	// Somebody a place away has heard of, and the place that has not.
	uint32 Answered = 0;
	uint32 Refused = 0;
	std::string Sample;
	std::string Abroad;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionNames* N = W.Instance.Components().GetPool(W.Fame_.Names).TryGet(H);
				if (N == nullptr || N->Count == 0)
				{
					return;
				}
				for (uint32 k = 0; k < N->Count && k < MostNames; ++k)
				{
					std::string Line;
					if (WhatWasKnown(W.Instance, W.Ages.Types(), W.Heard(), R.Index, N->Who[k].Person, Line))
					{
						++Answered;
						if (Sample.empty() && N->Who[k].Hops == 0)
						{
							Sample = Line;
						}
						if (Abroad.empty() && N->Who[k].Hops > 0)
						{
							Abroad = Line;
						}
					}
					else
					{
						++Refused;
					}
				}
			});
	// And a person nowhere has ever heard of: an index past everybody.
	std::string Nothing;
	const bool Known = WhatWasKnown(W.Instance, W.Ages.Types(), W.Heard(), Home, 0xFFFFFFFFu, Nothing);

	VAELEN_LOG_INFO(LogChron, "%u names could be asked about and answered, %u could not", Answered, Refused);
	VAELEN_LOG_INFO(LogChron, "  %s", Sample.c_str());
	VAELEN_LOG_INFO(LogChron, "  %s", Abroad.c_str());
	VT_CHECK_MSG(Answered == F.Names, "every name a place carries can be asked about");
	VT_CHECK_MSG(Refused == 0, "and none of them comes back empty");
	VT_CHECK_MSG(!Sample.empty(), "a place can say what it knows of somebody standing in it");
	VT_CHECK_MSG(!Abroad.empty(), "and of somebody it has only heard of");
	VT_CHECK_MSG(!Known, "and says nothing at all about a person it has never heard of");
}

VAELEN_TEST(Chronicle, TheSameWorldTellsTheSameStory)
{
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(A) != 0);
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Settle(B) != 0);
	const ChronicleStats X = A.Chronicle();
	const ChronicleStats Y = B.Chronicle();
	VAELEN_LOG_INFO(LogChron, "two runs of one seed: %u/%u sentences, digests %016llx and %016llx", X.Described,
					Y.Described, static_cast<unsigned long long>(X.Digest), static_cast<unsigned long long>(Y.Digest));
	VT_CHECK_EQ(X.Described, Y.Described);
	VT_CHECK_EQ(X.Believed, Y.Believed);
	VT_CHECK_EQ(X.Digest, Y.Digest);
}
