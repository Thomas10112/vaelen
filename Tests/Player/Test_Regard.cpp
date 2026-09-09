// VAELEN - Tests/Player
// Phase 10.06: what the people around the player make of them - read out of the
// acts in the log, weighed by the standing of whoever holds the opinion.
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
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
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Player;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogRegard);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InBonds = BondageRules{}, HourRules InHours = HourRules{},
					 OrderRules InOrders = OrderRules{}, DoingRules InDoings = DoingRules{},
					 RegardRules InRegard = RegardRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			One = PlayerTypes::Declare(Instance);
			First = StartTypes::Declare(Instance);
			Clock = HourTypes::Declare(Instance);
			Queue = OrderTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Goods = EconomyTypes::Declare(Instance);
			Known = RegardTypes::Declare(Instance);
			Rules_ = InOrders;
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage, InBonds);
			Fed = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Goods, EconomyRules{});
			// The first system in nine phases to want a grain finer than the year.
			Days_ = std::make_unique<PlayerDaySystem>(Instance, Ages.Types(), Persons, One, Clock, InHours);
			// The only thing in the project allowed to act on an intent, and the
			// seven verbs it asks, each of which goes through somebody else.
			Acts_ = std::make_unique<PlayerOrderSystem>(Instance, Ages.Types(), Persons, One, Clock, Queue, InOrders);
			Hands = std::make_unique<Doings>(Ages.Types(), Persons, Families, Needs, Goods, InDoings);
			// What the world makes of it all, read from the acts and nothing else.
			Talk = std::make_unique<RegardSystem>(Instance, Ages.Types(), Persons, One, Standing, Known, InRegard);
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Bonds->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Fed.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Days_.get());
			Instance.Systems().Add(Acts_.get());
			Instance.Systems().Add(Talk.get());
			Doings_ = InDoings;
			Regard_ = InRegard;
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
		std::vector<uint32> Ranked() const
		{
			std::vector<std::pair<uint32, uint32>> All;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && P->Total > 0)
						{
							All.push_back({P->Total, R.Index});
						}
					});
			std::sort(All.begin(), All.end(), [](const auto& A, const auto& B)
					  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
			std::vector<uint32> Out;
			for (const auto& [People, Index] : All)
			{
				Out.push_back(Index);
			}
			return Out;
		}
		uint32 Begin(StartRules R = StartRules{})
		{
			return BeginEnslaved(Instance, Ages.Types(), Persons, Bondage, Standing, One, First, R, Instance.Now());
		}
		const PlayerStart* Started() const { return StartOf(Instance, First); }
		StartStats Stats() const { return MeasureStart(Instance, Persons, Bondage, One, First); }
		HourStats Clock_(HourRules R = HourRules{}) const { return MeasureHours(Instance, Persons, One, Clock, R); }
		bool Open() { return BeginOrders(Instance, One, Queue, Instance.Now()); }
		Refusal Mean(Intent Kind, uint32 Target = 0, uint32 Amount = 0)
		{
			return Order(Instance, One, Queue, Rules_, Kind, Target, Amount, Instance.Now());
		}
		Refusal Mean(const PlayerCommand& C) { return Submit(Instance, One, Queue, Rules_, C); }
		const PlayerOrders* Meant() const { return OrdersOf(Instance, Queue); }
		uint32 Waiting() const { return OrdersHeld(Instance, Queue); }
		OrderStats Acts() const { return MeasureOrders(Instance, Persons, One, Queue, Rules_); }
		/// One day of the finer grain: the day turns, then what was meant is acted on.
		void Day() { Instance.TickMany(24); }
		/// Hands the verbs to the system. Without this the world is 10.04's: an
		/// intent costs its hours and changes nothing else.
		void GiveHands() { Acts_->ObserveDoing(Hands.get()); }
		uint32 RegionOfPerson(uint32 Person) const
		{
			const PersonInfo* P = FindPerson(Instance, Persons, Person);
			return P != nullptr ? P->Region : 0u;
		}
		uint32 HouseOfPerson(uint32 Person) const
		{
			const PersonInfo* P = FindPerson(Instance, Persons, Person);
			return P != nullptr ? P->Family : 0u;
		}
		const PersonNeeds* NeedsOf(uint32 Person) const
		{
			const PersonNeeds* Out = nullptr;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (Out == nullptr && P.Index == Person)
						{
							Out = Instance.Components().GetPool(Needs.Needs).TryGet(H);
						}
					});
			return Out;
		}
		/// What a person can lay hands on: their house's stock, or the region's
		/// common one when they have no house.
		uint32 GoodsOf(uint32 Person, Good G = Good::Grain) const
		{
			const uint32 House = HouseOfPerson(Person);
			if (House != 0)
			{
				const HouseStock* S = HouseStockOf(Instance, Families, Goods, House);
				if (S != nullptr)
				{
					return S->Amount[static_cast<usize>(G)];
				}
			}
			const RegionStock* R = StockOf(Instance, Ages.Types(), Goods, RegionOfPerson(Person));
			return R != nullptr ? R->Amount[static_cast<usize>(G)] : 0u;
		}
		/// Everything in a region, common and houses, of one good.
		uint32 AllGoodsIn(uint32 Region, Good G = Good::Grain) const
		{
			uint32 Out[GoodCount] = {};
			TotalStock(Instance, Ages.Types(), Families, Goods, Region, Out);
			return Out[static_cast<usize>(G)];
		}
		/// A living person of a region other than the played one, by lowest index.
		uint32 SomebodyIn(uint32 Region, uint32 Not) const
		{
			uint32 Best = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						if (P.Region != Region || P.Index == Not || P.State != static_cast<uint8>(LifeState::Alive))
						{
							return;
						}
						Best = Best == 0 || P.Index < Best ? P.Index : Best;
					});
			return Best;
		}
		DoingStats Did() const { return MeasureDoings(Instance); }
		const PlayerRegard* Thinks() const { return RegardOf(Instance, Known); }
		int32 ThinksOf(uint32 Person) const { return RegardFrom(Instance, Known, Person); }
		int32 Repute() const { return ReputeOf(Instance, Known); }
		RegardStats Regard() const { return MeasureRegard(Instance, Persons, One, Known, Regard_); }
		/// The rank 05.02 gives somebody, 0 when it gives them none.
		uint32 RankOf(uint32 Person) const
		{
			const PersonStanding* S = StandingOf(Instance, Persons, Standing, Person);
			return S != nullptr ? S->Rank : 0u;
		}
		/// Living people of a region who have something a person could take,
		/// highest ranked by 05.02 first; ties by index so the pick is the same
		/// in every run.
		std::vector<uint32> RichestFirst(uint32 Region, uint32 Not) const
		{
			std::vector<uint32> Out;
			for (const uint32 P : EverybodyIn(Region, Not))
			{
				if (GoodsOf(P) > 0)
				{
					Out.push_back(P);
				}
			}
			std::sort(Out.begin(), Out.end(),
					  [&](uint32 A, uint32 B) { return RankOf(A) != RankOf(B) ? RankOf(A) > RankOf(B) : A < B; });
			return Out;
		}
		/// Living people of a region other than one, by lowest index first.
		std::vector<uint32> EverybodyIn(uint32 Region, uint32 Not) const
		{
			std::vector<uint32> Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						if (P.Region == Region && P.Index != Not && P.State == static_cast<uint8>(LifeState::Alive))
						{
							Out.push_back(P.Index);
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		}
		const PlayerHours* Today() const { return HoursOf(Instance, Clock); }
		uint32 Left() const { return HoursLeft(Instance, Clock); }
		uint32 Spend(uint32 Hours) { return SpendHours(Instance, Clock, Hours); }
		uint32 Played() const { return PlayerPerson(Instance, One); }
		const BondState* Bond(uint32 Person) const { return BondOf(Instance, Persons, Bondage, Person); }
		/// How many living people of the detailed regions are bound at all.
		uint32 BoundCount() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.State != static_cast<uint8>(LifeState::Alive))
						{
							return;
						}
						const BondState* B = Instance.Components().GetPool(Bondage.Bond).TryGet(H);
						Out += B != nullptr && B->Kind != static_cast<uint8>(BondKind::Free) ? 1u : 0u;
					});
			return Out;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		PlayerTypes One;
		StartTypes First;
		HourTypes Clock;
		OrderTypes Queue;
		NeedTypes Needs;
		EconomyTypes Goods;
		RegardTypes Known;
		OrderRules Rules_;
		DoingRules Doings_;
		RegardRules Regard_;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<PlayerDaySystem> Days_;
		std::unique_ptr<NeedSystem> Fed;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<PlayerOrderSystem> Acts_;
		std::unique_ptr<Doings> Hands;
		std::unique_ptr<RegardSystem> Talk;
	};

	/// A world grown to 300 years with its two busiest regions simulated person
	/// by person, then Years more so that 05.04 has had time to bind people.
	bool Grown(Run& W, uint32 Years)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return false;
		}
		// The busiest region AND a neighbour of it, because walking somewhere is
		// one of the seven verbs and a person can only walk next door.
		const std::vector<uint32> Ranked = W.Ranked();
		if (Ranked.empty() || !RequestDetail(W.Instance, W.Lod, Ranked[0]))
		{
			return false;
		}
		const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Ages.Types().World.Regions);
		uint32 Near = 0;
		if (Ranked[0] < Graph.Neighbours.size())
		{
			for (const uint32 R : Ranked)
			{
				for (const uint16 N : Graph.Neighbours[Ranked[0]])
				{
					Near = Near == 0 && uint32{N} == R ? R : Near;
				}
			}
		}
		if (Near == 0 || !RequestDetail(W.Instance, W.Lod, Near))
		{
			return false;
		}
		W.Ages.Run(Years);
		return true;
	}

	/// A world grown, a person taken and their queue opened: everything the
	/// tests below start from.
	uint32 Living(Run& W, uint32 Years = 60)
	{
		if (!Grown(W, Years))
		{
			return 0;
		}
		StartRules Anywhere;
		Anywhere.PreferOre = 0;
		const uint32 Who = W.Begin(Anywhere);
		if (Who == 0)
		{
			return 0;
		}
		W.Ages.Run(1); // the first day turns
		return W.Open() ? Who : 0u;
	}
} // namespace

VAELEN_TEST(Regard, AnOpinionIsReadOutOfWhatWasActuallyDone)
{
	// Nobody has an opinion about an intent, a menu or a thing the player meant
	// to do. They have one about what was done to them, and it comes out of the
	// log rather than out of anything this test wrote.
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
	const std::vector<uint32> Rich = W.RichestFirst(Region, Who);
	VT_REQUIRE(Near.size() >= 2 && !Rich.empty());
	const uint32 Friend_ = Near[0] != Rich[0] ? Near[0] : Near[1];
	// Taking needs somebody with something to take: a person with nothing is
	// refused, which is 10.05 being right rather than this being difficult.
	const uint32 Wronged = Rich[0];

	// Nothing has been done to anybody, so nobody thinks anything.
	VT_CHECK(W.Thinks() == nullptr);
	VT_CHECK_EQ(W.ThinksOf(Friend_), 0);

	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK_MSG(W.Thinks() == nullptr, "working all day is nobody else's business");

	VT_CHECK(W.Mean(Intent::Give, Friend_, 2u) == Refusal::None);
	VT_CHECK(W.Mean(Intent::Take, Wronged, 1u) == Refusal::None);
	W.Day();
	const RegardStats S = W.Regard();
	VAELEN_LOG_INFO(LogRegard, "person %u thinks %d of the player, person %u thinks %d; repute %d", Friend_,
					W.ThinksOf(Friend_), Wronged, W.ThinksOf(Wronged), W.Repute());
	VT_CHECK_MSG(W.ThinksOf(Friend_) > 0, "somebody given to thinks well of them");
	VT_CHECK_MSG(W.ThinksOf(Wronged) < 0, "somebody taken from does not");
	VT_CHECK_EQ(S.Known, 2u);
	VT_CHECK_EQ(S.Friends, 1u);
	VT_CHECK_EQ(S.Enemies, 1u);
	VT_CHECK_EQ(S.Kindnesses, 1u);
	VT_CHECK_EQ(S.Wrongs, 1u);
	VT_CHECK_EQ(S.Bad, 0u);
	// And a refused doing is not an act: nothing was done, so nobody saw it.
	const int32 Held = W.ThinksOf(Friend_);
	VT_CHECK(W.Mean(Intent::Give, 999999u, 1u) == Refusal::None);
	W.Day();
	VT_CHECK_EQ(W.ThinksOf(Friend_), Held);
	VT_CHECK_EQ(W.Regard().Known, 2u);
}

VAELEN_TEST(Regard, AWordIsWorthLessThanAGiftAndBothAddUp)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
	VT_REQUIRE(Near.size() >= 2);
	const RegardRules R;

	VT_CHECK(W.Mean(Intent::Speak, Near[0]) == Refusal::None);
	W.Day();
	const int32 Spoken = W.ThinksOf(Near[0]);
	VT_CHECK_EQ(Spoken, R.ForSpeaking);

	// Working first so there is something to give away.
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK(W.Mean(Intent::Give, Near[1], 1u) == Refusal::None);
	W.Day();
	VAELEN_LOG_INFO(LogRegard, "a word is worth %d, a gift %d", Spoken, W.ThinksOf(Near[1]));
	VT_CHECK_MSG(W.ThinksOf(Near[1]) > Spoken, "a word is worth little beside a thing given");

	// Twice over builds, and the opinion remembers how many times.
	VT_CHECK(W.Mean(Intent::Speak, Near[0]) == Refusal::None);
	W.Day();
	VT_CHECK_MSG(W.ThinksOf(Near[0]) > Spoken, "and speaking twice is worth more than once");
	const PlayerRegard* Held = W.Thinks();
	VT_REQUIRE(Held != nullptr);
	uint32 Met = 0;
	for (usize i = 0; i < Held->Known; ++i)
	{
		Met = Held->Who[i].Person == Near[0] ? Held->Who[i].Met : Met;
	}
	VT_CHECK_EQ(Met, 2u);
	VT_CHECK_EQ(W.Regard().Bad, 0u);
}

VAELEN_TEST(Regard, WhatThePlaceMakesOfThemWeighsWhoIsSpeaking)
{
	// The one place the standing of 05.02 enters: an opinion is worth what its
	// holder is worth. Two worlds of one seed do the same two things to the
	// same two people, the other way round - a gift to the high ranked and a
	// theft from the low one, against the mirror of that. The deeds are equal
	// and the repute is not, because the place hears the higher one louder.
	auto Ranked = [](Run& W, uint32 Who, uint32& High, uint32& Low)
	{
		const std::vector<uint32> Rich = W.RichestFirst(W.RegionOfPerson(Who), Who);
		if (Rich.size() < 2)
		{
			return false;
		}
		High = Rich.front();
		Low = Rich.back();
		return W.RankOf(High) > W.RankOf(Low);
	};
	auto Play = [](Run& W, uint32 Gift, uint32 Theft)
	{
		for (uint32 i = 0; i < 3; ++i)
		{
			W.Mean(Intent::Work);
		}
		W.Day();
		W.Mean(Intent::Give, Gift, 2u);
		W.Mean(Intent::Take, Theft, 2u);
		W.Day();
	};

	Run Well(AelvorSeed);
	Run Ill(AelvorSeed);
	const uint32 Who = Living(Well);
	VT_REQUIRE(Who != 0);
	VT_REQUIRE(Living(Ill) == Who);
	Well.GiveHands();
	Ill.GiveHands();
	Well.Day();
	Ill.Day();
	uint32 High = 0;
	uint32 Low = 0;
	VT_REQUIRE(Ranked(Well, Who, High, Low));

	Play(Well, High, Low); // the great man is given to, the small one robbed
	Play(Ill, Low, High);  // and the other way about
	VAELEN_LOG_INFO(LogRegard, "rank %u and rank %u: giving to the greater leaves repute %d, giving to the lesser %d",
					Well.RankOf(High), Well.RankOf(Low), Well.Repute(), Ill.Repute());
	VT_CHECK_MSG(Well.ThinksOf(High) == Ill.ThinksOf(Low), "the same gift is worth the same to whoever gets it");
	VT_CHECK_MSG(Well.ThinksOf(Low) == Ill.ThinksOf(High), "and so is the same theft");
	VT_CHECK_MSG(Well.Repute() > Ill.Repute(), "but what the place makes of it is not the same at all");
	VT_CHECK_EQ(Well.Regard().Bad, 0u);
	VT_CHECK_EQ(Ill.Regard().Bad, 0u);

	// And none of this writes to 05.02: the played person is ranked by their
	// house, their office and their years like everybody else in the world.
	const PersonStanding* Was = StandingOf(Well.Instance, Well.Persons, Well.Standing, High);
	const PersonStanding* Same = StandingOf(Ill.Instance, Ill.Persons, Ill.Standing, High);
	VT_REQUIRE(Was != nullptr && Same != nullptr);
	VT_CHECK_MSG(Was->Score == Same->Score && Was->Rank == Same->Rank,
				 "a standing is what 05.02 says it is, whatever the player did");
}

VAELEN_TEST(Regard, TheWorldForgets)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
	VT_REQUIRE(!Near.empty());
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK(W.Mean(Intent::Give, Near[0], 2u) == Refusal::None);
	W.Day();
	const int32 Owed = W.ThinksOf(Near[0]);
	VT_REQUIRE(Owed > 0);

	// Two years of days in which the player does nothing to them at all.
	for (uint32 Day = 0; Day < 720; ++Day)
	{
		W.Day();
	}
	VAELEN_LOG_INFO(LogRegard, "two years later, %d of %d is left of it; repute %d", W.ThinksOf(Near[0]), Owed,
					W.Repute());
	VT_CHECK_MSG(W.ThinksOf(Near[0]) < Owed, "a kindness done once is not a claim on somebody for ever");
	VT_CHECK_MSG(W.ThinksOf(Near[0]) >= 0, "and forgetting stops at nothing rather than turning into a grudge");
	VT_CHECK_EQ(W.Regard().Bad, 0u);
}

VAELEN_TEST(Regard, RulesAndEdges)
{
	// Nobody played: nobody makes anything of anybody.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 20));
		W.Day();
		VT_CHECK(W.Thinks() == nullptr);
		VT_CHECK_EQ(W.Repute(), 0);
		VT_CHECK_EQ(W.Regard().Records, 0u);
		VT_CHECK_EQ(W.Regard().Bad, 0u);
	}
	// More people dealt with than the table holds: the faintest and oldest is
	// the one who stops thinking about them, and the table never grows.
	{
		Run W(AelvorSeed);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		W.GiveHands();
		W.Day();
		const uint32 Region = W.RegionOfPerson(Who);
		const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
		VT_REQUIRE(Near.size() > MostKnown + 2);
		for (usize i = 0; i < MostKnown + 3; ++i)
		{
			VT_CHECK(W.Mean(Intent::Speak, Near[i]) == Refusal::None);
			W.Day();
		}
		const RegardStats S = W.Regard();
		VAELEN_LOG_INFO(LogRegard, "after %zu people, %u are remembered", MostKnown + 3, S.Known);
		VT_CHECK_EQ(S.Known, static_cast<uint32>(MostKnown));
		VT_CHECK_MSG(W.ThinksOf(Near[MostKnown + 2]) != 0, "the last one dealt with is remembered");
		VT_CHECK_EQ(S.Bad, 0u);
	}
	// An opinion is bounded however much is done: nobody is owed everything.
	{
		RegardRules Loud;
		Loud.ForSpeaking = 400;
		Run W(AelvorSeed, BondageRules{}, HourRules{}, OrderRules{}, DoingRules{}, Loud);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		W.GiveHands();
		W.Day();
		const uint32 Region = W.RegionOfPerson(Who);
		const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
		VT_REQUIRE(!Near.empty());
		for (uint32 i = 0; i < 12; ++i)
		{
			VT_CHECK(W.Mean(Intent::Speak, Near[0]) == Refusal::None);
			W.Day();
		}
		VT_CHECK_EQ(W.ThinksOf(Near[0]), Loud.Most);
		VT_CHECK_EQ(W.Regard().Bad, 0u);
	}
}

VAELEN_TEST(Regard, DeterministicAndSnapshotSafe)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	const uint32 Who = Living(A);
	VT_REQUIRE(Who != 0);
	VT_REQUIRE(Living(B) == Who);
	A.GiveHands();
	B.GiveHands();
	A.Day();
	B.Day();
	const uint32 Region = A.RegionOfPerson(Who);
	const std::vector<uint32> Near = A.EverybodyIn(Region, Who);
	VT_REQUIRE(Near.size() >= 3);
	for (uint32 Day = 0; Day < 30; ++Day)
	{
		const Intent Kind = Day % 3u == 0 ? Intent::Speak : (Day % 3u == 1 ? Intent::Work : Intent::Give);
		const uint32 Target = Kind == Intent::Work ? 0u : Near[Day % 3u];
		A.Mean(Kind, Target, 1u);
		B.Mean(Kind, Target, 1u);
		A.Day();
		B.Day();
	}
	const RegardStats SA = A.Regard();
	VAELEN_LOG_INFO(LogRegard, "thirty days: %u known, %u friends, %u enemies, repute %d", SA.Known, SA.Friends,
					SA.Enemies, SA.Repute);
	VT_CHECK(SA.Known > 0);
	VT_CHECK_EQ(SA.Repute, B.Regard().Repute);
	VT_CHECK_EQ(SA.Known, B.Regard().Known);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));

	// What people make of somebody survives a save and a load.
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VT_REQUIRE(!Image.empty());
	Run C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_REQUIRE(C.Thinks() != nullptr);
	VT_CHECK_EQ(C.Regard().Known, SA.Known);
	VT_CHECK_EQ(C.Repute(), SA.Repute);
	VT_CHECK_EQ(C.Regard().Bad, 0u);
}
