// VAELEN - Tests/Player
// Phase 10.05: what the player can do - seven verbs, each of them going through
// the system that already owns that change.
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Doings.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogDoings);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InBonds = BondageRules{}, HourRules InHours = HourRules{},
					 OrderRules InOrders = OrderRules{}, DoingRules InDoings = DoingRules{})
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
			Doings_ = InDoings;
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
		OrderRules Rules_;
		DoingRules Doings_;
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

	/// The two regions the world is simulating person by person, in rank order.
	std::vector<uint32> Detailed(const Run& W)
	{
		std::vector<uint32> Out;
		for (const uint32 R : W.Ranked())
		{
			if (IsDetailed(W.Instance, W.Ages.Types(), W.Persons, R))
			{
				Out.push_back(R);
			}
		}
		return Out;
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

VAELEN_TEST(Doings, WorkingGoesThroughTheEconomy)
{
	// A day of work puts something in the store, and it gets there the way
	// everything else in the world gets there: Economy::AddStock, which
	// publishes what it moved with the act as its cause.
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	const uint32 Before = W.GoodsOf(Who);
	const DoingRules R;

	VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	W.Day();
	const uint32 After = W.GoodsOf(Who);
	const DoingStats S = W.Did();
	VAELEN_LOG_INFO(LogDoings, "a day of work: %u -> %u of grain, %u act(s), %u caused", Before, After, S.Acts,
					S.Caused);
	VT_CHECK_EQ(S.Worked, 1u);
	VT_CHECK_MSG(After == Before + R.WorkYield, "what a day of work brings in is in the store");
	VT_CHECK_MSG(S.Caused > 0, "and the goods that moved carry the act as their cause");
	VT_CHECK_EQ(W.Acts().Refused, 0u);
}

VAELEN_TEST(Doings, EatingCostsGrainAndFillsTheBelly)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	const DoingRules R;
	// One day first, because the tick after a whole year is the tick the yearly
	// systems run in: the ration of 04.04 tops everybody up there, and a day of
	// hunger inside it is levelled out by it. Every other day of the three
	// hundred and sixty is the player's own.
	W.Day();
	// Work: it puts grain within reach, and it makes the person hungry, which is
	// what gives a meal something to do.
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	const uint32 Grain = W.GoodsOf(Who);
	VT_REQUIRE(Grain >= R.EatGrain);
	const PersonNeeds* Was = W.NeedsOf(Who);
	VT_REQUIRE(Was != nullptr);
	const uint32 Food = Was->Food;
	const uint32 Rest = Was->Rest;
	VT_CHECK_MSG(Food < 255u, "a day of work costs the body something");

	VT_CHECK(W.Mean(Intent::Eat) == Refusal::None);
	W.Day();
	VT_CHECK_EQ(W.GoodsOf(Who), Grain - R.EatGrain);
	const PersonNeeds* Now = W.NeedsOf(Who);
	VT_REQUIRE(Now != nullptr);
	VAELEN_LOG_INFO(LogDoings, "three days of work then a meal: grain %u -> %u, food %u -> %u, rest %u -> %u", Grain,
					W.GoodsOf(Who), Food, static_cast<uint32>(Now->Food), Rest, static_cast<uint32>(Now->Rest));
	VT_CHECK_MSG(Now->Food > Food, "a meal fills the belly, through 04.04 and nothing else");
	VT_CHECK_EQ(W.Did().Ate, 1u);
}

VAELEN_TEST(Doings, WalkingGoesThroughTheGrainThatOwnsIt)
{
	// Moving is the verb with the most under it: a person's region is a fact
	// two grains have to agree about, and 04.06 is what owns that agreement.
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	const std::vector<uint32> Fine = Detailed(W);
	VT_REQUIRE(Fine.size() >= 2);
	const uint32 From = W.RegionOfPerson(Who);
	const uint32 To = Fine[0] == From ? Fine[1] : Fine[0];
	VT_REQUIRE(From != 0 && To != 0 && From != To);

	VT_CHECK(W.Mean(Intent::Move, To) == Refusal::None);
	W.Day();
	VAELEN_LOG_INFO(LogDoings, "walked from region %u to region %u; %u move event(s)", From, W.RegionOfPerson(Who),
					W.Did().Moved);
	VT_CHECK_MSG(W.RegionOfPerson(Who) == To, "the person is where they walked to");
	VT_CHECK_EQ(W.Did().Moved, 1u);
	// And both grains still agree about both regions, which is the whole reason
	// this goes through 04.06 rather than setting a field.
	VT_CHECK_MSG(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, From), "the region left behind still adds up");
	VT_CHECK_MSG(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, To), "and so does the one walked into");

	// Somewhere that is not next door is refused, and costs nothing.
	const uint32 Left = W.Left();
	VT_CHECK(W.Mean(Intent::Move, 60000u) == Refusal::None); // the door takes it
	W.Day();
	VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::TooFar));
	VT_CHECK_EQ(W.RegionOfPerson(Who), To);
	VT_CHECK_MSG(W.Left() >= Left, "a refused walk costs no hours");
}

VAELEN_TEST(Doings, GivingAndTakingMoveGoodsAndKeepTheTotal)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	const uint32 Region = W.RegionOfPerson(Who);
	const uint32 Other = W.SomebodyIn(Region, Who);
	VT_REQUIRE(Other != 0);
	for (uint32 i = 0; i < 4; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	const uint32 Total = W.AllGoodsIn(Region);
	const uint32 Mine = W.GoodsOf(Who);
	VT_REQUIRE(Mine > 0);

	VT_CHECK(W.Mean(Intent::Give, Other, 2u) == Refusal::None);
	W.Day();
	VAELEN_LOG_INFO(LogDoings, "gave: region held %u, now %u; mine %u -> %u", Total, W.AllGoodsIn(Region), Mine,
					W.GoodsOf(Who));
	VT_CHECK_MSG(W.AllGoodsIn(Region) == Total, "giving moves goods and makes none");
	VT_CHECK_EQ(W.Did().Gave, 1u);

	// Aimed at nobody, or at somebody who is not here: refused.
	VT_CHECK(W.Mean(Intent::Give, 0u, 1u) == Refusal::None);
	W.Day();
	VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::NoOne));
	VT_CHECK(W.Mean(Intent::Take, Other, 1u) == Refusal::None);
	W.Day();
	VT_CHECK_MSG(W.AllGoodsIn(Region) == Total, "and so does taking");
}

VAELEN_TEST(Doings, RulesAndEdges)
{
	// Resting goes through the field 04.04 reserved for exactly this.
	{
		Run W(AelvorSeed);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		W.GiveHands();
		// Work first, so that there is tiredness for the rest to undo.
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
		W.Day();
		const PersonNeeds* Was = W.NeedsOf(Who);
		VT_REQUIRE(Was != nullptr);
		const uint32 Rest = Was->Rest;
		VT_CHECK(W.Mean(Intent::Rest) == Refusal::None);
		W.Day();
		const PersonNeeds* Now = W.NeedsOf(Who);
		VT_REQUIRE(Now != nullptr);
		VT_CHECK_MSG(Now->Rest > Rest, "resting raises the rest of the person who rested");
		VT_CHECK_EQ(W.Did().Rested, 1u);
	}
	// Speaking to somebody who is not there is refused; speaking to somebody who
	// is moves nothing at all, and leaves the act in the log for 10.06 to read.
	{
		Run W(AelvorSeed);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		W.GiveHands();
		const uint32 Region = W.RegionOfPerson(Who);
		const uint32 Other = W.SomebodyIn(Region, Who);
		VT_REQUIRE(Other != 0);
		VT_CHECK(W.Mean(Intent::Speak, 999999u) == Refusal::None);
		W.Day();
		VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::NoOne));
		const uint32 Held = W.AllGoodsIn(Region);
		VT_CHECK(W.Mean(Intent::Speak, Other) == Refusal::None);
		W.Day();
		VT_CHECK_EQ(W.AllGoodsIn(Region), Held);
		VT_CHECK_MSG(W.Did().Acts >= 1, "and what was said is in the log as an act");
	}
	// A world whose system was never handed the verbs is the world 10.04 left:
	// the intent costs its hours and changes nothing else.
	{
		Run W(AelvorSeed);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		const uint32 Held = W.GoodsOf(Who);
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
		W.Day();
		VT_CHECK_EQ(W.Acts().Taken, 1u);
		VT_CHECK_MSG(W.GoodsOf(Who) == Held, "with no verbs handed over, nothing moves");
		VT_CHECK_EQ(W.Did().Worked, 1u); // the act happened; only its effect is absent
	}
}

VAELEN_TEST(Doings, DeterministicAndReplayable)
{
	// The claim of 10.04 has to survive the verbs having effects: the same
	// stream on the same seed gives the same world, economy and people
	// included, not merely the same queue.
	struct Recorded
	{
		uint64 Tick = 0;
		PlayerCommand Command;
		Refusal Verdict = Refusal::None;
	};
	std::vector<Recorded> Stream;

	Run Lived(AelvorSeed);
	const uint32 Who = Living(Lived);
	VT_REQUIRE(Who != 0);
	Lived.GiveHands();
	const uint32 Region = Lived.RegionOfPerson(Who);
	const uint32 Other = Lived.SomebodyIn(Region, Who);
	VT_REQUIRE(Other != 0);
	for (uint32 Day = 0; Day < 60; ++Day)
	{
		for (uint32 i = 0; i < 3; ++i)
		{
			PlayerCommand C;
			C.Kind = static_cast<uint8>(1u + (Lived.Left() + Day + i) % 8u);
			C.Target = (Day + i) % 2u == 0 ? Other : 0u;
			C.Amount = 1u + (Day % 3u);
			C.Issued = Lived.Instance.Now();
			Stream.push_back(Recorded{C.Issued, C, Lived.Mean(C)});
		}
		Lived.Day();
	}
	const DoingStats A = Lived.Did();
	const Hash64 StateA = ComputeStateDigest(Lived.Instance);
	const Hash64 LogA = Lived.Instance.Log().Digest();
	VAELEN_LOG_INFO(LogDoings, "sixty days: %u act(s), %u refused, %u worked, %u ate, %u caused; state=%016llx", A.Acts,
					A.Refused, A.Worked, A.Ate, A.Caused, static_cast<unsigned long long>(StateA));
	VT_REQUIRE(A.Acts > 50);
	VT_CHECK_MSG(A.Caused > 0, "the world moved because of what was done");

	Run Again(AelvorSeed);
	VT_REQUIRE(Living(Again) == Who);
	Again.GiveHands();
	usize Next = 0;
	for (uint32 Day = 0; Day < 60; ++Day)
	{
		while (Next < Stream.size() && Stream[Next].Tick <= Again.Instance.Now())
		{
			VT_CHECK(Again.Mean(Stream[Next].Command) == Stream[Next].Verdict);
			++Next;
		}
		Again.Day();
	}
	VT_CHECK_EQ(Next, Stream.size());
	VT_CHECK_MSG(Again.Instance.Log().Digest() == LogA, "a recorded stream replays to the same life");
	VT_CHECK_MSG(ComputeStateDigest(Again.Instance) == StateA, "and to the same world, economy and all");
	VT_CHECK_EQ(Again.Did().Acts, A.Acts);
	VT_CHECK_EQ(Again.Acts().Bad, 0u);
}
