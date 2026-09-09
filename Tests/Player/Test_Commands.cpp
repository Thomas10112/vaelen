// VAELEN - Tests/Player
// Phase 10.04: intent as commands - what the outside world may do (queue a
// struct) and what only the simulation may do (act on it).
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
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
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Player;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogCommands);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InBonds = BondageRules{}, HourRules InHours = HourRules{},
					 OrderRules InOrders = OrderRules{})
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
			// The first system in nine phases to want a grain finer than the year.
			Days_ = std::make_unique<PlayerDaySystem>(Instance, Ages.Types(), Persons, One, Clock, InHours);
			// The only thing in the project allowed to act on an intent.
			Acts_ = std::make_unique<PlayerOrderSystem>(Instance, Ages.Types(), Persons, One, Clock, Queue, InOrders);
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Bonds->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Days_.get());
			Instance.Systems().Add(Acts_.get());
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
		OrderRules Rules_;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<PlayerDaySystem> Days_;
		std::unique_ptr<PlayerOrderSystem> Acts_;
	};

	/// A world grown to 300 years with its two busiest regions simulated person
	/// by person, then Years more so that 05.04 has had time to bind people.
	bool Grown(Run& W, uint32 Years)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return false;
		}
		const std::vector<uint32> Ranked = W.Ranked();
		if (Ranked.size() < 2 || !RequestDetail(W.Instance, W.Lod, Ranked[0]) ||
			!RequestDetail(W.Instance, W.Lod, Ranked[1]))
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

VAELEN_TEST(Commands, MeaningSomethingIsNotDoingIt)
{
	// The whole architectural claim of the task: what arrives from outside the
	// simulation changes NOTHING. Submitting an intent moves no hour, publishes
	// no event and writes no history; the system inside does all of that, later.
	Run W(AelvorSeed);
	VT_REQUIRE(Living(W) != 0);
	VT_REQUIRE(W.Today() != nullptr);
	const uint32 Left = W.Left();
	const Hash64 Before = W.Instance.Log().Digest();
	const usize Records = W.Instance.Log().All().size();

	VT_CHECK(W.Mean(Intent::Wait) == Refusal::None);
	VT_CHECK_EQ(W.Waiting(), 1u);
	VT_CHECK_MSG(W.Left() == Left, "queueing an intent spends no hour");
	VT_CHECK_MSG(W.Instance.Log().Digest() == Before, "queueing an intent writes no history");
	VT_CHECK_EQ(W.Instance.Log().All().size(), Records);
	VT_CHECK_EQ(W.Acts().Taken, 0u);

	// And now the simulation runs, and the simulation is what acts.
	W.Day();
	const OrderStats S = W.Acts();
	VAELEN_LOG_INFO(LogCommands, "one day: %u taken, %u refused, %u waiting, %u hours left", S.Taken, S.Refused, S.Held,
					W.Left());
	VT_CHECK_EQ(S.Taken, 1u);
	VT_CHECK_EQ(S.Held, 0u);
	VT_CHECK_EQ(S.Refused, 0u);
	VT_CHECK_EQ(S.Bad, 0u);
	VT_CHECK_MSG(W.Left() < W.Today()->Awake, "doing something costs the hours it costs");
	VT_CHECK_MSG(W.Instance.Log().Digest() != Before, "what a person did is history");

	// The event says who did what, and it came out of the simulation.
	uint32 Acted = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(PlayerActedEvent))
		{
			++Acted;
		}
	}
	VT_CHECK_EQ(Acted, 1u);
}

VAELEN_TEST(Commands, ADayHoldsWhatItCanAndTheRestWaits)
{
	// A person who means eight things at dawn does not do all eight at dawn.
	Run W(AelvorSeed);
	VT_REQUIRE(Living(W) != 0);
	const OrderRules R;
	const uint32 Cost = R.HoursOf[static_cast<usize>(Intent::Work)];
	VT_REQUIRE(Cost > 0);
	const uint32 Awake = W.Today()->Awake;
	const uint32 Fits = Awake / Cost;
	VT_REQUIRE(Fits > 1 && Fits < MostOrders);

	for (usize i = 0; i < MostOrders; ++i)
	{
		VT_CHECK(W.Mean(Intent::Work, 1, 1) == Refusal::None);
	}
	VT_CHECK_EQ(W.Waiting(), static_cast<uint32>(MostOrders));
	W.Day();
	VAELEN_LOG_INFO(LogCommands, "a day of %u waking hours holds %u works of %u hours; %u still meant", Awake, Fits,
					Cost, W.Waiting());
	VT_CHECK_EQ(W.Acts().Taken, Fits);
	VT_CHECK_EQ(W.Waiting(), static_cast<uint32>(MostOrders) - Fits);
	VT_CHECK_MSG(W.Left() < Cost, "the day ended with less than one more of them left in it");
	// Tomorrow the rest, and nothing was lost in between.
	W.Day();
	VT_CHECK_EQ(W.Acts().Taken, static_cast<uint32>(MostOrders));
	VT_CHECK_EQ(W.Waiting(), 0u);
	VT_CHECK_EQ(W.Acts().Refused, 0u);
	VT_CHECK_EQ(W.Acts().Bad, 0u);
}

VAELEN_TEST(Commands, RulesAndEdges)
{
	// Nobody played, and nobody to queue for.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 20));
		VT_CHECK(!W.Open());
		VT_CHECK(W.Mean(Intent::Wait) == Refusal::NoPlayer);
		VT_CHECK(W.Meant() == nullptr);
		W.Day();
		VT_CHECK_EQ(W.Acts().Queues, 0u);
	}
	// Somebody played, but the queue was never opened: still nothing to say to.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 60));
		StartRules Anywhere;
		Anywhere.PreferOre = 0;
		VT_REQUIRE(W.Begin(Anywhere) != 0);
		VT_CHECK(W.Mean(Intent::Wait) == Refusal::NoPlayer);
		VT_CHECK(W.Open());
		VT_CHECK_MSG(!W.Open(), "a person has one queue");
	}
	// An intent with no name is refused at the door, and a full queue drops
	// what will not fit rather than forgetting what was already meant.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Living(W) != 0);
		VT_CHECK(W.Mean(Intent::None) == Refusal::Unknown);
		PlayerCommand Nonsense;
		Nonsense.Kind = 200;
		VT_CHECK(W.Mean(Nonsense) == Refusal::Unknown);
		VT_CHECK_EQ(W.Waiting(), 0u);
		for (usize i = 0; i < MostOrders; ++i)
		{
			VT_CHECK(W.Mean(Intent::Work, 1, 1) == Refusal::None);
		}
		VT_CHECK(W.Mean(Intent::Work, 1, 1) == Refusal::Full);
		VT_CHECK_EQ(W.Waiting(), static_cast<uint32>(MostOrders));
		VT_CHECK_EQ(W.Acts().Dropped, 1u);
		VT_CHECK_EQ(W.Acts().Bad, 0u);
	}
	// Something no day is long enough for is refused rather than left to block
	// every intent behind it for the rest of the life.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Living(W) != 0);
		PlayerCommand Endless;
		Endless.Kind = static_cast<uint8>(Intent::Work);
		Endless.Hours = 100;
		Endless.Issued = W.Instance.Now();
		VT_CHECK(W.Mean(Endless) == Refusal::None);
		VT_CHECK(W.Mean(Intent::Wait) == Refusal::None);
		W.Day();
		const OrderStats S = W.Acts();
		VAELEN_LOG_INFO(LogCommands, "refused: %s", RefusalName(static_cast<Refusal>(W.Meant()->Last)));
		VT_CHECK_EQ(S.Refused, 1u);
		VT_CHECK_MSG(S.Taken == 1u, "and what was behind it still got done");
		VT_CHECK_EQ(S.Held, 0u);
		VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::Costly));
	}
	// An intent that waited longer than it was meant for is no longer meant.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Living(W) != 0);
		PlayerCommand Ancient;
		Ancient.Kind = static_cast<uint8>(Intent::Eat);
		Ancient.Issued = 0; // a month of hours ago and more
		VT_REQUIRE(W.Instance.Now() > OrderRules{}.StaleAfter);
		VT_CHECK(W.Mean(Ancient) == Refusal::None);
		W.Day();
		VT_CHECK_EQ(W.Acts().Refused, 1u);
		VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::Stale));
		VT_CHECK_EQ(W.Acts().Taken, 0u);
	}
	// A queue closed drops what was waiting and leaves the world running.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Living(W) != 0);
		VT_CHECK(W.Mean(Intent::Wait) == Refusal::None);
		VT_CHECK(EndOrders(W.Instance, W.Queue));
		VT_CHECK(!EndOrders(W.Instance, W.Queue));
		W.Day();
		VT_CHECK_EQ(W.Acts().Queues, 0u);
		VT_CHECK_EQ(W.Acts().Bad, 0u);
	}
}

VAELEN_TEST(Commands, TheDeadMeanNothing)
{
	// A life ends, and the intents of somebody who is no longer there are
	// refused rather than silently done by a corpse.
	Run W(AelvorSeed);
	VT_REQUIRE(Living(W) != 0);
	W.Ages.Run(90); // long past any life the world gives
	const PersonInfo* Who = nullptr;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				const PlayerMark* M = W.Instance.Components().GetPool(W.One.Mark).TryGet(H);
				if (M != nullptr)
				{
					Who = &P;
				}
			});
	const bool Gone = Who == nullptr || Who->State != static_cast<uint8>(LifeState::Alive);
	VAELEN_LOG_INFO(LogCommands, "after ninety years the played person is %s", Gone ? "gone" : "still alive");
	VT_REQUIRE(Gone);
	VT_CHECK(W.Mean(Intent::Eat) == Refusal::None); // the queue still takes it
	W.Day();
	VT_CHECK_EQ(W.Acts().Taken, 0u);
	VT_CHECK_EQ(W.Acts().Refused, 1u);
	VT_CHECK_EQ(W.Meant()->Last, static_cast<uint32>(Refusal::Dead));
	VT_CHECK_EQ(W.Acts().Bad, 0u);
}

VAELEN_TEST(Commands, ARecordedStreamReplaysToTheSameLife)
{
	// The reason for all the ceremony. A stream of intents with the ticks they
	// were meant on is the player's half of the input; the seed is the world's
	// half. Applied to the same seed, the same stream gives the same life -
	// which is only true because the intents are inputs and the simulation is
	// the only writer.
	struct Recorded
	{
		uint64 Tick = 0;
		PlayerCommand Command;
		Refusal Verdict = Refusal::None; ///< what the door said at the time
	};
	std::vector<Recorded> Stream;

	Run Lived(AelvorSeed);
	const uint32 Who = Living(Lived);
	VT_REQUIRE(Who != 0);
	for (uint32 Day = 0; Day < 200; ++Day)
	{
		// Six things a day, more hours of them than a day has, so that the queue
		// really does back up and spill: a replay that only ever sees an empty
		// queue has not been tested.
		for (uint32 i = 0; i < 6; ++i)
		{
			// What is meant depends on the world it is meant in, which is what
			// makes this a recording and not a script.
			const uint32 Pick = (Lived.Left() + Who + Day + i) % 8u;
			PlayerCommand C;
			C.Kind = static_cast<uint8>(1u + Pick);
			C.Target = Day % 3u;
			C.Amount = 1u + (Day % 5u);
			C.Hours = 1u + ((Day * 7u + i * 3u) % 6u);
			if ((Day * 6u + i) % 37u == 0)
			{
				C.Hours = 100u; // now and then, something no day is long enough for
			}
			C.Issued = Lived.Instance.Now();
			// Everything the outside said is recorded, including what the door
			// turned away: a stream that keeps only what was accepted is not
			// the input, it is a summary of it.
			Stream.push_back(Recorded{C.Issued, C, Lived.Mean(C)});
		}
		Lived.Day();
	}
	const OrderStats A = Lived.Acts();
	const Hash64 LogA = Lived.Instance.Log().Digest();
	VAELEN_LOG_INFO(LogCommands, "a life of %zu intents recorded: %u taken, %u refused, %u dropped; log=%016llx",
					Stream.size(), A.Taken, A.Refused, A.Dropped, static_cast<unsigned long long>(LogA));
	VT_REQUIRE(A.Taken > 100);
	VT_CHECK_MSG(A.Dropped > 0, "the queue really did fill, so the replay had to fill it the same way");
	VT_CHECK_MSG(A.Refused > 0, "and the world really did refuse some of it");

	// The replay knows nothing about the world: it submits what was recorded,
	// on the tick it was recorded on, and nothing else.
	Run Again(AelvorSeed);
	VT_REQUIRE(Living(Again) == Who);
	usize Next = 0;
	for (uint32 Day = 0; Day < 200; ++Day)
	{
		while (Next < Stream.size() && Stream[Next].Tick <= Again.Instance.Now())
		{
			VT_CHECK_MSG(Again.Mean(Stream[Next].Command) == Stream[Next].Verdict,
						 "the door gave the same answer it gave the first time");
			++Next;
		}
		Again.Day();
	}
	const OrderStats B = Again.Acts();
	VT_CHECK_EQ(Next, Stream.size());
	VT_CHECK_MSG(B.Dropped == A.Dropped, "the queue filled at the same moments");
	VT_CHECK_MSG(B.Taken == A.Taken, "the same intents were acted on");
	VT_CHECK_MSG(B.Refused == A.Refused, "and the same ones were refused");
	VT_CHECK_MSG(B.Digest == A.Digest, "down to the same intents in the same slots");
	VT_CHECK_MSG(Again.Instance.Log().Digest() == LogA, "a recorded stream replays to the same life");
	VT_CHECK_EQ(Again.Clock_().Digest, Lived.Clock_().Digest);
	VT_CHECK_EQ(B.Bad, 0u);
}

VAELEN_TEST(Commands, DeterministicAndSnapshotSafe)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(Living(A) != 0);
	VT_REQUIRE(Living(B) != 0);
	for (uint32 Day = 0; Day < 40; ++Day)
	{
		A.Mean(Intent::Work, Day % 4u, 1u);
		B.Mean(Intent::Work, Day % 4u, 1u);
		A.Day();
		B.Day();
	}
	VT_CHECK_EQ(A.Acts().Digest, B.Acts().Digest);
	VT_CHECK_EQ(A.Acts().Taken, B.Acts().Taken);

	// A queue with intents still waiting in it survives a save and a load: what
	// a person means is part of the world, not of the program that ran it.
	VT_CHECK(A.Mean(Intent::Speak, 7, 2) == Refusal::None);
	VT_CHECK(A.Mean(Intent::Give, 9, 3) == Refusal::None);
	const uint32 Waiting = A.Waiting();
	VT_REQUIRE(Waiting == 2u);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VT_REQUIRE(!Image.empty());
	Run C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_REQUIRE(C.Meant() != nullptr);
	VT_CHECK_EQ(C.Waiting(), Waiting);
	VT_CHECK_EQ(C.Acts().Digest, A.Acts().Digest);
	VT_CHECK_EQ(C.Acts().Bad, 0u);
}
