// VAELEN - Tests/Population
// Phase 04.02: ageing, mortality, fertility and the reconciliation of the grains.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.02): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 200 years.
#define VAELEN_LIVES_FROZEN_128 0x9f2615d35856a752ull
#define VAELEN_LIVES_ALIVE_128 1499u
#define VAELEN_LIVES_BORN_128 1499u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLives);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, LifeRules InRules = LifeRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, InRules);
			Instance.Systems().Add(Lives.get());
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
						if (P != nullptr && P->Total > People)
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		const RegionPopulation* Counts(uint32 Region) const
		{
			const RegionPopulation* Found = nullptr;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region)
						{
							Found = Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						}
					});
			return Found;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		std::unique_ptr<LifeSystem> Lives;
	};

	void LogLifeStats(const char* Title, const LifeStats& S, uint32 Capacity)
	{
		VAELEN_LOG_INFO(
			LogLives,
			"%s: %u alive (%u children, %u elders, oldest %u, mean age %llu), %u dead, %u born here, capacity %u",
			Title, S.Alive, S.Children, S.Elders, S.Oldest,
			static_cast<unsigned long long>(S.Alive > 0 ? S.AgeSum / S.Alive : 0u), S.Dead, S.BornHere, Capacity);
	}
} // namespace

VAELEN_TEST(Lives, AgesAndBandsFollowTheRules)
{
	const LifeRules Rules;
	PersonInfo P;
	P.Born = TicksPerYear * 10;
	VT_CHECK_EQ(AgeYears(P, TicksPerYear * 10), 0u);
	VT_CHECK_EQ(AgeYears(P, TicksPerYear * 11 - 1), 0u);
	VT_CHECK_EQ(AgeYears(P, TicksPerYear * 11), 1u);
	VT_CHECK_EQ(AgeYears(P, TicksPerYear * 5), 0u); // born later than the tick
	VT_CHECK_EQ(BandOf(0, Rules), 0u);
	VT_CHECK_EQ(BandOf(1, Rules), 1u);
	VT_CHECK_EQ(BandOf(14, Rules), 2u);
	VT_CHECK_EQ(BandOf(15, Rules), 3u);
	VT_CHECK_EQ(BandOf(74, Rules), 6u);
	VT_CHECK_EQ(BandOf(75, Rules), 7u);
	VT_CHECK_EQ(BandOf(89, Rules), 7u);
	VT_CHECK_EQ(BandOf(90, Rules), 8u);
	VT_CHECK_EQ(BandOf(200, Rules), LifeRules::Bands - 1);
	// Every band's deaths are a chance, never a certainty; infants and elders die most.
	for (uint32 B = 0; B < LifeRules::Bands; ++B)
	{
		VT_CHECK(Rules.DeathsPerMille[B] < 1000);
	}
	VT_CHECK(Rules.DeathsPerMille[0] > Rules.DeathsPerMille[2] && Rules.DeathsPerMille[8] > Rules.DeathsPerMille[3]);
}

VAELEN_TEST(Lives, TwoCenturiesInADetailedRegionKeepTheGrainsConsistent)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	const RegionPopulation Start = *W.Counts(Region);
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	uint32 Failures = 0;
	uint32 MinAlive = 0xffffffffu;
	uint32 MaxAlive = 0;
	for (uint32 Year = 1; Year <= 200; ++Year)
	{
		W.Ages.Run(1);
		if (!IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the persons and the counts disagree", Year);
		}
		const RegionPopulation* Counts = W.Counts(Region);
		VT_REQUIRE(Counts != nullptr);
		MinAlive = Counts->Total < MinAlive ? Counts->Total : MinAlive;
		MaxAlive = Counts->Total > MaxAlive ? Counts->Total : MaxAlive;
		// Nobody lives past the last band for long: the oldest is under 110.
		const LifeStats S = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
		VT_CHECK(S.Oldest < 110);
		if (Year % 50 == 0)
		{
			char Title[32];
			std::snprintf(Title, sizeof(Title), "year %u", Year);
			LogLifeStats(Title, S, Counts->Capacity);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const RegionPopulation End = *W.Counts(Region);
	const LifeStats S = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
	VT_CHECK(S.Alive > 0 && S.Dead > 0 && S.BornHere > 0);
	VT_CHECK(S.Children > 0 && S.Elders > 0);
	VT_CHECK(End.Total <= End.Capacity + End.Capacity / 5); // births slow near capacity
	VT_CHECK(End.Total * 10 >= End.Capacity * 7);			// and the region stays peopled
	VT_CHECK(MinAlive * 4 > Start.Total);					// no collapse
	VT_CHECK_EQ(End.Capacity, Start.Capacity);
	VAELEN_LOG_INFO(LogLives, "region %u: %u -> %u people over 200 years (min %u, max %u)", Region, Start.Total,
					End.Total, MinAlive, MaxAlive);
	// Events: one PersonBorn per person with a mother, one PersonDied per dead person.
	uint32 Born = 0;
	uint32 Died = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Born += E.Is(PersonBornEvent) ? 1u : 0u;
		if (E.Is(PersonDiedEvent))
		{
			++Died;
			VT_CHECK(E.Subject.IsValid());
			VT_CHECK_EQ(E.Get<PersonPayload>().Region, Region);
		}
	}
	uint32 WithMother = 0;
	uint32 Dead = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				WithMother += P.Mother != 0 ? 1u : 0u;
				Dead += P.State == static_cast<uint8>(LifeState::Dead) ? 1u : 0u;
				if (P.Mother != 0)
				{
					VT_CHECK(P.Father != 0);
					VT_CHECK(P.Born >= TicksPerYear * 300);
				}
			});
	VT_CHECK_EQ(Born, WithMother);
	VT_CHECK_EQ(Died, Dead);
	// The rest of the world went on: the coarse systems still moved the other regions.
	const PopulationStats World_ = MeasurePopulation(W.Instance, W.Ages.Types().Population);
	VT_CHECK(World_.Regions > 1 && World_.People > End.Total);
}

VAELEN_TEST(Lives, TheCoarseSystemsLeaveDetailedRegionsAlone)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	// No migration wave leaves or reaches the detailed region while it is detailed.
	const usize Mark = W.Instance.Log().Count();
	W.Ages.Run(30);
	const std::vector<Event>& Events = W.Instance.Log().All();
	uint32 WavesTouching = 0;
	uint32 WavesElsewhere = 0;
	for (usize i = Mark; i < Events.size(); ++i)
	{
		if (Events[i].Is(MigrationWaveEvent))
		{
			const RegionPeople P = Events[i].Get<RegionPeople>();
			WavesTouching += P.Region == Region || P.Reserved == Region ? 1u : 0u;
			WavesElsewhere += P.Region != Region && P.Reserved != Region ? 1u : 0u;
		}
	}
	VT_CHECK_EQ(WavesTouching, 0u);
	VT_CHECK(WavesElsewhere > 0);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	// Demoted, the region rejoins the coarse world: the coarse growth moves its
	// counts again within a few years.
	VT_CHECK(DemoteRegion(W.Instance, W.Ages.Types(), W.Persons, Region) > 0);
	const uint32 AtDemotion = W.Counts(Region)->Total;
	W.Ages.Run(30);
	VT_CHECK(W.Counts(Region)->Total != AtDemotion);
	// Without a detailed region the life system does nothing.
	Run Q(11);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(64), 60));
	const Hash64 Before = ComputeStateDigest(Q.Instance);
	RandomStream Stream(1);
	TickContext C;
	C.Tick = Q.Instance.Now();
	C.Entities = &Q.Instance.Entities();
	C.Components = &Q.Instance.Components();
	C.Random = &Stream;
	C.Events = &Q.Instance.Events();
	Q.Lives->Tick(C);
	VT_CHECK_EQ(ComputeStateDigest(Q.Instance), Before);
}

VAELEN_TEST(Lives, DeterministicSnapshotSafeAndRulesMatter)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_REQUIRE(PromoteRegion(A.Instance, A.Ages.Types(), A.Persons, MaterialiseRules{}, Region, A.Instance.Now()) > 0);
	VT_REQUIRE(PromoteRegion(B.Instance, B.Ages.Types(), B.Persons, MaterialiseRules{}, Region, B.Instance.Now()) > 0);
	A.Ages.Run(40);
	B.Ages.Run(40);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Instance.Log().Digest(), B.Instance.Log().Digest());
	// A snapshot between two yearly ticks continues identically, persons included.
	A.Instance.TickMany(100);
	B.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(IsDetailed(R.Instance, R.Ages.Types(), R.Persons, Region));
	A.Ages.Run(40);
	R.Ages.Run(40);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK(IsConsistent(R.Instance, R.Ages.Types(), R.Persons, Region));
	// Rules: no births leave only deaths; no deaths leave only births.
	LifeRules Barren;
	Barren.BirthsPerMille = 0;
	Barren.MinimumBirthsPerMille = 0;
	Run X(AelvorSeed, Barren);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(PromoteRegion(X.Instance, X.Ages.Types(), X.Persons, MaterialiseRules{}, Region, X.Instance.Now()) > 0);
	const uint32 StartX = X.Counts(Region)->Total;
	X.Ages.Run(40);
	const LifeStats SX = MeasureLives(X.Instance, X.Persons, Region, X.Instance.Now());
	VT_CHECK_EQ(SX.BornHere, 0u);
	VT_CHECK(SX.Dead > 0 && SX.Alive < StartX);
	LifeRules Immortal;
	for (uint32 Bd = 0; Bd < LifeRules::Bands; ++Bd)
	{
		Immortal.DeathsPerMille[Bd] = 0;
	}
	Run Y(AelvorSeed, Immortal);
	VT_REQUIRE(Y.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(PromoteRegion(Y.Instance, Y.Ages.Types(), Y.Persons, MaterialiseRules{}, Region, Y.Instance.Now()) > 0);
	Y.Ages.Run(40);
	const LifeStats SY = MeasureLives(Y.Instance, Y.Persons, Region, Y.Instance.Now());
	VT_CHECK_EQ(SY.Dead, 0u);
	VT_CHECK(SY.BornHere > 0);
}

VAELEN_TEST(Lives, FrozenLivesAreReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	W.Ages.Run(200);
	const DetailStats D = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	const LifeStats S = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
	VAELEN_LOG_INFO(LogLives, "frozen: lives128=%016llx alive=%u born=%u",
					static_cast<unsigned long long>(D.PersonsDigest), S.Alive, S.BornHere);
	VT_CHECK_EQ(D.PersonsDigest, Hash64{VAELEN_LIVES_FROZEN_128});
	VT_CHECK_EQ(S.Alive, uint32{VAELEN_LIVES_ALIVE_128});
	VT_CHECK_EQ(S.BornHere, uint32{VAELEN_LIVES_BORN_128});
	VT_CHECK_EQ(D.Inconsistent, 0u);
}
