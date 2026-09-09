// VAELEN - Tests/Player
// Phase 10.07: the player in the chronicle - a life as records, and the why of
// anything that happened because of them walked back through every layer below.
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Player/PlayerHistory.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-09 (10.07): AELVOR 128, the
// player of 10.02, twenty days of a life with the doings of 10.05 and the
// opinions of 10.06 in it, written by ExportLife.
#define VAELEN_LIFE_FROZEN_TEXT 0x3727cebce1fc782cull
#define VAELEN_LIFE_FROZEN_RECORDS 12u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLife);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InBonds = BondageRules{}, HourRules InHours = HourRules{},
					 OrderRules InOrders = OrderRules{}, DoingRules InDoings = DoingRules{},
					 RegardRules InRegard = RegardRules{}, LifeChronicleRules InLife = LifeChronicleRules{})
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
			Told = LifeChronicleTypes::Declare(Instance);
			Kept = PersonChronicleTypes::Declare(Instance);
			// Only the types: the economy describer needs them to name things,
			// not a system to run them.
			Bought = TradeTypes::Declare(Instance);
			Sold = MarketTypes::Declare(Instance);
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
			// The chronicle of the layer below, so that the fall-through has
			// something of another layer to fall through to.
			Lives_ = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families, Kept,
													   PersonChronicleRules{});
			Lives_->Attach();
			Chron = MakeContext();
			Told_ = std::make_unique<LifeChronicle>(Instance, Ages.Types(), Chron, Told, InLife);
			Told_->Attach();
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
		LifeContext MakeContext()
		{
			Money.Persons = Persons;
			Money.Families = Families;
			Money.Trade = Bought;
			Money.Markets = Sold;
			LifeContext C;
			// The layer under this one, so that the grain a life moves is named
			// by the module that owns grain rather than by its event id.
			C.Goods = &Money;
			C.Persons = Persons;
			C.Families = Families;
			C.Player = One;
			C.Regard = Known;
			return C;
		}
		LifeChronicleStats Kept_() const { return CheckLifeChronicle(Instance, Ages.Types(), Chron, Told); }
		std::string Chronicle(uint32 MaxLines = 0) const
		{
			std::string Out;
			ExportChronicleWithLife(Instance, Ages.Types(), Chron, Out, MaxLines);
			return Out;
		}
		std::string Story(uint32 MaxActs = 0) const
		{
			std::string Out;
			ExportLife(Instance, Ages.Types(), Chron, Out, MaxActs);
			return Out;
		}
		std::string Why(PersistentId Id) const
		{
			std::string Out;
			ExportWhyWithLife(Instance, Ages.Types(), Chron, Id, Out);
			return Out;
		}
		std::string Says(const Event& E) const
		{
			std::string Out;
			DescribeLifeEvent(Instance, Ages.Types(), Chron, E, Out);
			return Out;
		}
		/// The last event the log holds that was caused by one of the player's acts.
		PersistentId LastCausedByAnAct() const
		{
			std::vector<const Event*> Acts;
			LifeTimeline(Instance, Chron, Acts);
			PersistentId Out;
			for (const Event& E : Instance.Log().All())
			{
				for (const Event* A : Acts)
				{
					Out = E.Cause.IsValid() && A->Id == E.Cause ? E.Id : Out;
				}
			}
			return Out;
		}
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
		LifeChronicleTypes Told;
		TradeTypes Bought;
		MarketTypes Sold;
		EconomyContext Money;
		PersonChronicleTypes Kept;
		LifeContext Chron;
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
		std::unique_ptr<PersonChronicle> Lives_;
		std::unique_ptr<LifeChronicle> Told_;
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

VAELEN_TEST(LifeHistory, ALifeIsWhatTouchedSomebodyAndNotEveryHourOfIt)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Rich = W.RichestFirst(Region, Who);
	const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
	VT_REQUIRE(!Rich.empty() && Near.size() >= 2);

	// A day of work and a meal: real, and not history.
	const uint32 Before = W.Kept_().Records;
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK(W.Mean(Intent::Eat) == Refusal::None);
	W.Day();
	VT_CHECK_MSG(W.Kept_().Records == Before, "a day of work is not history");

	// What touched somebody else is.
	VT_CHECK(W.Mean(Intent::Give, Rich[0], 2u) == Refusal::None);
	W.Day();
	VT_CHECK(W.Mean(Intent::Take, Rich[0], 1u) == Refusal::None);
	W.Day();
	const LifeChronicleStats S = W.Kept_();
	VAELEN_LOG_INFO(LogLife, "%u record(s) kept, %u of them of the player, %u described, %u dropped", S.Records,
					S.OfThePlayer, S.Described, S.Dropped);
	VT_CHECK_EQ(S.Records, Before + 2u);
	VT_CHECK_EQ(S.OfThePlayer, S.Records);
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Records);
	VT_CHECK_EQ(S.ByType[0], S.Records); // both are doings, neither a refusal
	VT_CHECK_EQ(S.ByType[1], 0u);

	// A refusal is not history either, by default.
	VT_CHECK(W.Mean(Intent::Give, 999999u, 1u) == Refusal::None);
	W.Day();
	VT_CHECK_EQ(W.Kept_().Records, S.Records);
	VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::NoOne));
}

VAELEN_TEST(LifeHistory, EveryEventHasASentenceInTheSameHand)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Rich = W.RichestFirst(Region, Who);
	VT_REQUIRE(!Rich.empty());
	// Work first: giving with nothing to give is refused, and a refusal is not
	// an act - which is 10.05 and 10.06 being right rather than this being awkward.
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK(W.Mean(Intent::Give, Rich[0], 2u) == Refusal::None);
	W.Day();

	// The act has a line of this layer, with the year in it.
	std::string Act;
	std::string Below;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(PlayerActedEvent))
		{
			Act = W.Says(E);
		}
		if (E.Is(PersonBornEvent) && Below.empty())
		{
			Below = W.Says(E); // an event of the layer under this one
		}
	}
	VAELEN_LOG_INFO(LogLife, "this layer: %s", Act.c_str());
	VAELEN_LOG_INFO(LogLife, "the one below: %s", Below.c_str());
	VT_CHECK_MSG(!Act.empty(), "an act of the player has a sentence");
	VT_CHECK_MSG(Act.find("Year ") != std::string::npos, "stamped with the year like every layer below");
	VT_CHECK_MSG(Act.find("gave to") != std::string::npos, "and it says what was done");
	VT_CHECK_MSG(!Below.empty(), "and an event of a lower layer still has one");
	VT_CHECK_MSG(Below.find("born") != std::string::npos, "written by the layer that owns it");

	// The chronicle reads in order and holds both layers.
	const std::string Text = W.Chronicle();
	VT_CHECK_MSG(!Text.empty(), "the chronicle is not empty");
	VT_CHECK_MSG(Text.find("gave to") != std::string::npos, "the life is in it");
}

VAELEN_TEST(LifeHistory, TheWhyOfWhatHappenedWalksBackToTheAct)
{
	// 10.05 passes the act's own event as the cause of everything a doing
	// moves, so the grain that left a house points back at the giving. This is
	// the chain, read as text.
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Rich = W.RichestFirst(Region, Who);
	VT_REQUIRE(!Rich.empty());
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK(W.Mean(Intent::Give, Rich[0], 2u) == Refusal::None);
	W.Day();

	const PersistentId Moved = W.LastCausedByAnAct();
	VT_REQUIRE(Moved.IsValid());
	const std::string Chain = W.Why(Moved);
	VAELEN_LOG_INFO(LogLife, "why:\n%s", Chain.c_str());
	VT_CHECK_MSG(Chain.find("because") != std::string::npos, "the goods that moved have a reason");
	VT_CHECK_MSG(Chain.find("gave to") != std::string::npos, "and the reason is the giving");
	uint32 Lines = 0;
	for (const char C : Chain)
	{
		Lines += C == '\n' ? 1u : 0u;
	}
	VT_CHECK_MSG(Lines >= 2, "the chain is the thing and then its cause");
}

VAELEN_TEST(LifeHistory, TheLifeReadsAsALife)
{
	Run W(AelvorSeed);
	const uint32 Who = Living(W);
	VT_REQUIRE(Who != 0);
	W.GiveHands();
	W.Day();
	const uint32 Region = W.RegionOfPerson(Who);
	const std::vector<uint32> Rich = W.RichestFirst(Region, Who);
	const std::vector<uint32> Near = W.EverybodyIn(Region, Who);
	VT_REQUIRE(!Rich.empty() && Near.size() >= 2);
	for (uint32 i = 0; i < 3; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
	}
	W.Day();
	VT_CHECK(W.Mean(Intent::Give, Rich[0], 2u) == Refusal::None);
	VT_CHECK(W.Mean(Intent::Speak, Near[0] != Rich[0] ? Near[0] : Near[1]) == Refusal::None);
	W.Day();
	VT_CHECK(W.Mean(Intent::Take, Rich[0], 1u) == Refusal::None);
	W.Day();

	const std::string Story = W.Story();
	VAELEN_LOG_INFO(LogLife, "the life:\n%s", Story.c_str());
	VT_CHECK_MSG(!Story.empty(), "a life is not empty");
	VT_CHECK_MSG(Story.find("gave to") != std::string::npos, "what they gave is in it");
	VT_CHECK_MSG(Story.find("took from") != std::string::npos, "and what they took");
	VT_CHECK_MSG(Story.find("thinks") != std::string::npos, "and what the people who know them make of it");
	VT_CHECK_MSG(Story.find("the place at large") != std::string::npos, "and what the place makes of them");
	VT_CHECK_MSG(Story.find("why:") != std::string::npos, "and why the last of it happened");
	VT_CHECK_MSG(Story.find("because") != std::string::npos, "walked back to what caused it");
}

VAELEN_TEST(LifeHistory, RulesAndEdges)
{
	// Nobody played: nothing to chronicle, and every call still answers.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 20));
		W.Day();
		VT_CHECK_EQ(W.Kept_().Records, 0u);
		std::string Name;
		NamePlayed(W.Instance, W.Ages.Types(), W.Chron, Name);
		VT_CHECK_MSG(Name == "nobody", "and it says so rather than inventing one");
		VT_CHECK_MSG(!W.Story().empty(), "a life of nobody is a line saying nobody");
	}
	// Every hour of it, when that is what is wanted, and the refusals with it.
	{
		LifeChronicleRules Everything;
		Everything.RecordSmallDoings = 1;
		Everything.RecordRefusals = 1;
		Run W(AelvorSeed, BondageRules{}, HourRules{}, OrderRules{}, DoingRules{}, RegardRules{}, Everything);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		W.GiveHands();
		W.Day();
		for (uint32 i = 0; i < 3; ++i)
		{
			VT_CHECK(W.Mean(Intent::Work) == Refusal::None);
		}
		W.Day();
		VT_CHECK_MSG(W.Kept_().Records >= 3u, "a day of work is history when the rules say it is");
		VT_CHECK(W.Mean(Intent::Give, 999999u, 1u) == Refusal::None);
		W.Day();
		const LifeChronicleStats S = W.Kept_();
		VT_CHECK_MSG(S.ByType[1] > 0, "and so is what the world would not let them do");
		std::string Text = W.Chronicle();
		VT_CHECK_MSG(Text.find("could not") != std::string::npos, "which reads as what it was");
		VT_CHECK_MSG(Text.find("nobody there") != std::string::npos, "and says why");
	}
	// A life is not a ledger: past the year's worth, the rest is dropped and
	// counted rather than kept in silence.
	{
		LifeChronicleRules Few;
		Few.MaxRecordsPerYear = 2;
		Run W(AelvorSeed, BondageRules{}, HourRules{}, OrderRules{}, DoingRules{}, RegardRules{}, Few);
		const uint32 Who = Living(W);
		VT_REQUIRE(Who != 0);
		W.GiveHands();
		W.Day();
		const std::vector<uint32> Near = W.EverybodyIn(W.RegionOfPerson(Who), Who);
		VT_REQUIRE(Near.size() >= 5);
		for (usize i = 0; i < 5; ++i)
		{
			VT_CHECK(W.Mean(Intent::Speak, Near[i]) == Refusal::None);
			W.Day();
		}
		const LifeChronicleStats S = W.Kept_();
		VAELEN_LOG_INFO(LogLife, "five doings, %u kept and %u dropped", S.Records, S.Dropped);
		VT_CHECK_EQ(S.Records, 2u);
		VT_CHECK_EQ(S.Dropped, 3u);
	}
}

VAELEN_TEST(LifeHistory, TheSameLifeIsWrittenTheSameWayEveryTime)
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
	const std::vector<uint32> Rich = A.RichestFirst(A.RegionOfPerson(Who), Who);
	const std::vector<uint32> Near = A.EverybodyIn(A.RegionOfPerson(Who), Who);
	VT_REQUIRE(!Rich.empty() && Near.size() >= 2);
	for (uint32 Day = 0; Day < 20; ++Day)
	{
		const Intent Kind = Day % 4u == 0
								? Intent::Give
								: (Day % 4u == 1 ? Intent::Work : (Day % 4u == 2 ? Intent::Speak : Intent::Take));
		const uint32 Target = Kind == Intent::Work ? 0u : (Kind == Intent::Speak ? Near[Day % 2u] : Rich[0]);
		A.Mean(Kind, Target, 1u);
		B.Mean(Kind, Target, 1u);
		A.Day();
		B.Day();
	}
	const std::string StoryA = A.Story();
	const std::string StoryB = B.Story();
	const LifeChronicleStats S = A.Kept_();
	const Hash64 TextA = HashBytes(StoryA.data(), StoryA.size());
	VAELEN_LOG_INFO(LogLife, "twenty days: %u record(s), %zu bytes of life, text=%016llx", S.Records, StoryA.size(),
					static_cast<unsigned long long>(TextA));
	VT_CHECK(S.Records > 5);
	VT_CHECK_MSG(StoryA == StoryB, "the same life is written the same way, word for word");
	// And the same way on every compiler and platform that runs this, which is
	// what the frozen value is for.
	VT_CHECK_EQ(S.Records, VAELEN_LIFE_FROZEN_RECORDS);
	VT_CHECK_EQ(TextA, Hash64{VAELEN_LIFE_FROZEN_TEXT});
	VT_CHECK_EQ(HashBytes(StoryB.data(), StoryB.size()), TextA);
	VT_CHECK_EQ(A.Chronicle(), B.Chronicle());
	VT_CHECK_EQ(A.Kept_().Records, B.Kept_().Records);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
}
