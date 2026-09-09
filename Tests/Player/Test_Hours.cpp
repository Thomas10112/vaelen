// VAELEN - Tests/Player
// Phase 10.03: the player's grain - a day at a time, for one person, in a world
// that runs at the year, and the history it does not change.
//
// STATUS: PROTOTYPE (Phase 10)

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
	VAELEN_DEFINE_LOG_CATEGORY(LogHours);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InBonds = BondageRules{}, HourRules InHours = HourRules{})
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
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<PlayerDaySystem> Days_;
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
} // namespace

VAELEN_TEST(Hours, ADayTurnsForThePlayedPerson)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Grown(W, 60));
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	const uint32 Who = W.Begin(Anywhere);
	VT_REQUIRE(Who != 0);

	// Nothing has turned yet: the grain begins the first day the system runs.
	W.Ages.Run(1);
	const PlayerHours* Today = W.Today();
	VT_REQUIRE(Today != nullptr);
	const HourRules R;
	VAELEN_LOG_INFO(LogHours, "day %u, %u awake, %u spent, %u days lived, %u missed", Today->Day, Today->Awake,
					Today->Spent, Today->Days, Today->Missed);
	VT_CHECK_EQ(Today->Awake, R.HoursPerDay - R.SleepHours);
	VT_CHECK_EQ(Today->Spent, 0u);
	VT_CHECK_EQ(W.Left(), Today->Awake);

	// A year of days: the finer grain really is finer. The world ticks 8640
	// times a year and the day turns 360 of them.
	const uint32 Before = Today->Days;
	W.Ages.Run(1);
	const HourStats S = W.Clock_();
	VAELEN_LOG_INFO(LogHours, "after one more year: %u days lived, %u missed, bad=%u", S.Days, S.Missed, S.Bad);
	VT_CHECK_MSG(S.Days > Before, "the day turns while the world runs at the year");
	VT_CHECK_MSG(S.Days >= Before + 300, "a year is three hundred and sixty days of it");
	VT_CHECK_EQ(S.Records, 1u);
	VT_CHECK_EQ(S.Bad, 0u);
}

VAELEN_TEST(Hours, ADayIsABudgetAndNeverAnOverdraft)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Grown(W, 60));
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	VT_REQUIRE(W.Begin(Anywhere) != 0);
	W.Ages.Run(1);
	VT_REQUIRE(W.Today() != nullptr);

	const uint32 Awake = W.Today()->Awake;
	VT_REQUIRE(Awake > 2);
	VT_CHECK_EQ(W.Spend(2), 2u);
	VT_CHECK_EQ(W.Left(), Awake - 2u);
	// Asking for more than is left gives what is left, and nothing goes negative.
	VT_CHECK_EQ(W.Spend(1000), Awake - 2u);
	VT_CHECK_EQ(W.Left(), 0u);
	VT_CHECK_EQ(W.Spend(1), 0u);
	VT_CHECK_EQ(W.Clock_().Bad, 0u);

	// And the next day gives the hours back.
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Left(), Awake);
	VT_CHECK_EQ(W.Today()->Spent, 0u);
}

VAELEN_TEST(Hours, TheFinerGrainWritesNoHistory)
{
	// The claim that lets a person live an hour at a time inside a world that
	// runs at the year: the day publishes nothing, so the two worlds write the
	// same history. The state digests differ - the mark and the day ARE state -
	// and the event log, which is what the world remembers, does not.
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	Run Lived(AelvorSeed);
	Run Watched(AelvorSeed);
	VT_REQUIRE(Grown(Lived, 60));
	VT_REQUIRE(Grown(Watched, 60));
	VT_REQUIRE(Lived.Begin(Anywhere) != 0);
	// Watched has the same day system registered and nobody played, so the only
	// difference between the two worlds is that one of them has a player in it.
	for (uint32 Year = 0; Year < 20; ++Year)
	{
		Lived.Ages.Run(1);
		Watched.Ages.Run(1);
	}
	const Hash64 LogA = Lived.Instance.Log().Digest();
	const Hash64 LogB = Watched.Instance.Log().Digest();
	const HourStats S = Lived.Clock_();
	VAELEN_LOG_INFO(LogHours, "%u days lived; log played=%016llx watched=%016llx", S.Days,
					static_cast<unsigned long long>(LogA), static_cast<unsigned long long>(LogB));
	VT_CHECK_MSG(S.Days > 7000, "twenty years of days really turned");
	VT_CHECK_MSG(LogA == LogB, "a world with somebody living an hour at a time writes the same history");
	VT_CHECK_EQ(S.Bad, 0u);

	// And the people of the world are the same people.
	const PopulationStats PA = MeasurePopulation(Lived.Instance, Lived.Ages.Types().Population);
	const PopulationStats PB = MeasurePopulation(Watched.Instance, Watched.Ages.Types().Population);
	VT_CHECK_EQ(PA.People, PB.People);
	VT_CHECK_EQ(PA.Regions, PB.Regions);
}

VAELEN_TEST(Hours, RulesAndEdges)
{
	// Nobody played: the grain has nobody to run for and writes nothing.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 20));
		W.Ages.Run(2);
		VT_CHECK(W.Today() == nullptr);
		VT_CHECK_EQ(W.Left(), 0u);
		VT_CHECK_EQ(W.Spend(5), 0u);
		const HourStats S = W.Clock_();
		VT_CHECK_EQ(S.Records, 0u);
		VT_CHECK_EQ(S.Bad, 0u);
	}
	// A day with no sleep in it is all waking hours; a day that is all sleep has
	// none, and neither is incoherent.
	{
		HourRules Sleepless;
		Sleepless.SleepHours = 0;
		Run W(AelvorSeed, BondageRules{}, Sleepless);
		VT_REQUIRE(Grown(W, 60));
		StartRules Anywhere;
		Anywhere.PreferOre = 0;
		VT_REQUIRE(W.Begin(Anywhere) != 0);
		W.Ages.Run(1);
		VT_REQUIRE(W.Today() != nullptr);
		VT_CHECK_EQ(W.Today()->Awake, Sleepless.HoursPerDay);
		VT_CHECK_EQ(W.Clock_(Sleepless).Bad, 0u);
	}
	{
		HourRules Abed;
		Abed.SleepHours = 24;
		Run W(AelvorSeed, BondageRules{}, Abed);
		VT_REQUIRE(Grown(W, 60));
		StartRules Anywhere;
		Anywhere.PreferOre = 0;
		VT_REQUIRE(W.Begin(Anywhere) != 0);
		W.Ages.Run(1);
		VT_REQUIRE(W.Today() != nullptr);
		VT_CHECK_EQ(W.Today()->Awake, 0u);
		VT_CHECK_EQ(W.Left(), 0u);
		VT_CHECK_EQ(W.Spend(1), 0u);
		VT_CHECK_EQ(W.Clock_(Abed).Bad, 0u);
	}
}

VAELEN_TEST(Hours, DeterministicAndSnapshotSafe)
{
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(Grown(A, 60));
	VT_REQUIRE(Grown(B, 60));
	const uint32 Who = A.Begin(Anywhere);
	VT_REQUIRE(Who != 0);
	VT_REQUIRE(B.Begin(Anywhere) == Who);
	A.Ages.Run(5);
	B.Ages.Run(5);
	VT_CHECK_EQ(A.Clock_().Digest, B.Clock_().Digest);
	VT_CHECK_EQ(A.Clock_().Days, B.Clock_().Days);

	// The day survives a save and a load, half spent.
	VT_REQUIRE(A.Spend(3) == 3u);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VT_REQUIRE(!Image.empty());
	Run C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_REQUIRE(C.Today() != nullptr);
	VT_CHECK_EQ(C.Today()->Spent, 3u);
	VT_CHECK_EQ(C.Clock_().Digest, A.Clock_().Digest);
	VT_CHECK_EQ(C.Clock_().Bad, 0u);
}
