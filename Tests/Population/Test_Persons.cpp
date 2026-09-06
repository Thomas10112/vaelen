// VAELEN - Tests/Population
// Phase 04.01: persons and the two grains of population.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.01): every settled
// region of AELVOR 128 promoted after 300 years of pre-history.
#define VAELEN_PERSONS_FROZEN_128 0xa92da70b85f09c0full
#define VAELEN_PERSONS_COUNT_128 19781u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPersons);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance);
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
		// The most populated region.
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
	};
} // namespace

VAELEN_TEST(Persons, PromotionMaterialisesTheCountsExactly)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(Region != 0);
	const RegionPopulation Before = *W.Counts(Region);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	const uint32 Created =
		PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now());
	VT_CHECK_EQ(Created, Before.Total);
	VT_CHECK(IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	const RegionCensus C = CountPersons(W.Instance, W.Persons, Region);
	VT_CHECK_EQ(C.Alive, Before.Total);
	VT_CHECK_EQ(C.Dead, 0u);
	VT_CHECK_EQ(C.Female + C.Male, C.Alive);
	VT_CHECK(C.Female * 10 > C.Alive * 4 && C.Female * 10 < C.Alive * 6); // near half
	VAELEN_LOG_INFO(LogPersons, "region %u: %u persons (%u women, %u men), %u faithless", Region, C.Alive, C.Female,
					C.Male, C.Faithless);
	// Every person: alive, in the region, of a culture the region holds, born at
	// most MaxAgeYears ago, with a unique index and identity.
	std::vector<uint32> Indices;
	std::vector<Hash64> Identities;
	const MaterialiseRules Rules;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				VT_CHECK_EQ(P.Region, Region);
				VT_CHECK_EQ(P.State, static_cast<uint8>(LifeState::Alive));
				VT_CHECK_EQ(P.Died, 0u);
				VT_CHECK(Before.SlotOf(P.Culture) < RegionPopulation::MaxCultures);
				VT_CHECK(P.Born <= W.Instance.Now());
				VT_CHECK(W.Instance.Now() - P.Born <= uint64{Rules.MaxAgeYears + 1} * TicksPerYear);
				VT_CHECK(P.Identity != 0);
				VT_CHECK_EQ(W.Instance.Entities().GetId(H).Kind(), IdKind::Person);
				Indices.push_back(P.Index);
				Identities.push_back(P.Identity);
			});
	std::sort(Indices.begin(), Indices.end());
	VT_CHECK(std::unique(Indices.begin(), Indices.end()) == Indices.end());
	VT_CHECK_EQ(Indices.front(), 1u);
	VT_CHECK_EQ(Indices.back(), Created);
	std::sort(Identities.begin(), Identities.end());
	VT_CHECK(std::unique(Identities.begin(), Identities.end()) == Identities.end());
	// Ages form a young pyramid: at least a third are under MaxAge / 3.
	uint32 Young = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach([&](EntityHandle, const PersonInfo& P)
				 { Young += (W.Instance.Now() - P.Born) < uint64{Rules.MaxAgeYears / 3} * TicksPerYear ? 1u : 0u; });
	VT_CHECK(Young * 3 >= C.Alive);
	// Misuse: promoting again, an unsettled or unknown region, or above the cap.
	VT_CHECK_EQ(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, Rules, Region, W.Instance.Now()), 0u);
	VT_CHECK_EQ(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, Rules, 0, W.Instance.Now()), 0u);
	VT_CHECK_EQ(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, Rules, 9999, W.Instance.Now()), 0u);
	uint32 Empty = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionPopulation* P =
					W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				if (Empty == 0 && P != nullptr && P->Total == 0)
				{
					Empty = R.Index;
				}
			});
	VT_REQUIRE(Empty != 0);
	VT_CHECK_EQ(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, Rules, Empty, W.Instance.Now()), 0u);
	MaterialiseRules Tiny;
	Tiny.MaxPersonsPerRegion = 10;
	uint32 Other = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionPopulation* P =
					W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				if (Other == 0 && R.Index != Region && P != nullptr && P->Total > 10)
				{
					Other = R.Index;
				}
			});
	VT_REQUIRE(Other != 0);
	VT_CHECK_EQ(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, Tiny, Other, W.Instance.Now()), 0u);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Other));
	const DetailStats S = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	VT_CHECK_EQ(S.DetailedRegions, 1u);
	VT_CHECK_EQ(S.Persons, Created);
	VT_CHECK_EQ(S.Inconsistent, 0u);
}

VAELEN_TEST(Persons, DemotionFoldsTheLivingBackAndDestroysThePersons)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	const RegionPopulation Before = *W.Counts(Region);
	const uint32 EntitiesBefore = W.Instance.Entities().GetAliveCount();
	const PopulationStats PeopleBefore = MeasurePopulation(W.Instance, W.Ages.Types().Population);
	const FaithStats FaithBefore = MeasureFaith(W.Instance, W.Ages.Types().Population, W.Ages.Types().Religion);
	const uint32 Created =
		PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now());
	VT_REQUIRE(Created > 0);
	VT_CHECK_EQ(W.Instance.Entities().GetAliveCount(), EntitiesBefore + Created);
	// A few persons die and one changes faith by hand: the fold reflects the living.
	uint32 Killed = 0;
	uint32 Converted = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, PersonInfo& P)
			{
				if (Killed < 7)
				{
					P.State = static_cast<uint8>(LifeState::Dead);
					P.Died = W.Instance.Now();
					++Killed;
				}
				else if (Converted == 0 && P.Religion != 0)
				{
					P.Religion = 0;
					++Converted;
				}
			});
	VT_CHECK(!IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region)); // the counts lag
	const uint32 Removed = DemoteRegion(W.Instance, W.Ages.Types(), W.Persons, Region);
	VT_CHECK_EQ(Removed, Created);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	VT_CHECK_EQ(W.Instance.Entities().GetAliveCount(), EntitiesBefore);
	VT_CHECK_EQ(MeasureDetail(W.Instance, W.Ages.Types(), W.Persons).Persons, 0u);
	const RegionPopulation After = *W.Counts(Region);
	VT_CHECK_EQ(After.Total, Before.Total - Killed);
	VT_CHECK_EQ(After.Capacity, Before.Capacity);
	const FaithStats FaithAfter = MeasureFaith(W.Instance, W.Ages.Types().Population, W.Ages.Types().Religion);
	VT_CHECK(FaithAfter.Adherents <= FaithBefore.Adherents);
	VT_CHECK(FaithAfter.Adherents + Killed + Converted >= FaithBefore.Adherents);
	VT_CHECK_EQ(DemoteRegion(W.Instance, W.Ages.Types(), W.Persons, Region), 0u); // not detailed any more
	// A clean round trip changes nothing in the counts.
	const uint32 Again =
		PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now());
	VT_CHECK_EQ(Again, After.Total);
	VT_CHECK_EQ(DemoteRegion(W.Instance, W.Ages.Types(), W.Persons, Region), Again);
	const RegionPopulation RoundTrip = *W.Counts(Region);
	VT_CHECK_EQ(RoundTrip.Total, After.Total);
	for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
	{
		VT_CHECK_EQ(RoundTrip.Culture[S], After.Culture[S]);
		VT_CHECK_EQ(RoundTrip.Count[S], After.Count[S]);
	}
	VT_CHECK_EQ(MeasurePopulation(W.Instance, W.Ages.Types().Population).People, PeopleBefore.People - Killed);
}

VAELEN_TEST(Persons, EveryRegionCanBeDetailedAndTheWorldStillAddsUp)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const PopulationStats People = MeasurePopulation(W.Instance, W.Ages.Types().Population);
	const FaithStats Faith = MeasureFaith(W.Instance, W.Ages.Types().Population, W.Ages.Types().Religion);
	std::vector<uint32> Regions;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach([&](EntityHandle, const RegionInfo& R) { Regions.push_back(R.Index); });
	std::sort(Regions.begin(), Regions.end());
	uint32 Created = 0;
	uint32 Detailed = 0;
	for (const uint32 R : Regions)
	{
		const uint32 N = PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, R, W.Instance.Now());
		Created += N;
		Detailed += N > 0 ? 1u : 0u;
	}
	VT_CHECK_EQ(uint64{Created}, People.People);
	VT_CHECK_EQ(Detailed, People.SettledRegions);
	const DetailStats S = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	VT_CHECK_EQ(S.DetailedRegions, Detailed);
	VT_CHECK_EQ(S.Persons, Created);
	VT_CHECK_EQ(S.Alive, Created);
	VT_CHECK_EQ(S.Inconsistent, 0u);
	// Believers among persons equal the believers of the world.
	uint64 Believers = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach([&](EntityHandle, const PersonInfo& P) { Believers += P.Religion != 0 ? 1u : 0u; });
	VT_CHECK_EQ(Believers, Faith.Adherents);
	VAELEN_LOG_INFO(LogPersons, "AELVOR 128 after 300 years: %u persons in %u regions, %llu believers, digest %016llx",
					Created, Detailed, static_cast<unsigned long long>(Believers),
					static_cast<unsigned long long>(S.PersonsDigest));
	// Demote everything: the coarse world is exactly as before.
	uint32 Removed = 0;
	for (const uint32 R : Regions)
	{
		Removed += DemoteRegion(W.Instance, W.Ages.Types(), W.Persons, R);
	}
	VT_CHECK_EQ(Removed, Created);
	VT_CHECK_EQ(MeasurePopulation(W.Instance, W.Ages.Types().Population).People, People.People);
	VT_CHECK_EQ(MeasureFaith(W.Instance, W.Ages.Types().Population, W.Ages.Types().Religion).Adherents,
				Faith.Adherents);
	VT_CHECK_EQ(MeasureDetail(W.Instance, W.Ages.Types(), W.Persons).DetailedRegions, 0u);
}

VAELEN_TEST(Persons, DeterministicAndSnapshotSafe)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_CHECK_EQ(Region, B.Busiest());
	const uint32 NA =
		PromoteRegion(A.Instance, A.Ages.Types(), A.Persons, MaterialiseRules{}, Region, A.Instance.Now());
	const uint32 NB =
		PromoteRegion(B.Instance, B.Ages.Types(), B.Persons, MaterialiseRules{}, Region, B.Instance.Now());
	VT_CHECK_EQ(NA, NB);
	VT_CHECK(NA > 0);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(MeasureDetail(A.Instance, A.Ages.Types(), A.Persons).PersonsDigest,
				MeasureDetail(B.Instance, B.Ages.Types(), B.Persons).PersonsDigest);
	// The persons survive a snapshot byte for byte and the world continues identically.
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(IsDetailed(R.Instance, R.Ages.Types(), R.Persons, Region));
	VT_CHECK(IsConsistent(R.Instance, R.Ages.Types(), R.Persons, Region));
	VT_CHECK_EQ(MeasureDetail(R.Instance, R.Ages.Types(), R.Persons).PersonsDigest,
				MeasureDetail(A.Instance, A.Ages.Types(), A.Persons).PersonsDigest);
	A.Ages.Run(5);
	R.Ages.Run(5);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	// The tick of the promotion is part of the draw: a later promotion differs.
	Run C(AelvorSeed);
	VT_REQUIRE(C.Ages.Generate(Run::Square(64), 120));
	C.Instance.TickMany(3);
	PromoteRegion(C.Instance, C.Ages.Types(), C.Persons, MaterialiseRules{}, Region, C.Instance.Now());
	VT_CHECK(MeasureDetail(C.Instance, C.Ages.Types(), C.Persons).PersonsDigest !=
			 MeasureDetail(B.Instance, B.Ages.Types(), B.Persons).PersonsDigest);
	// Rules matter: an all-female world.
	Run F(AelvorSeed);
	VT_REQUIRE(F.Ages.Generate(Run::Square(64), 120));
	MaterialiseRules Women;
	Women.FemalePerMille = 1000;
	PromoteRegion(F.Instance, F.Ages.Types(), F.Persons, Women, Region, F.Instance.Now());
	const RegionCensus CF = CountPersons(F.Instance, F.Persons, Region);
	VT_CHECK_EQ(CF.Male, 0u);
	VT_CHECK_EQ(CF.Female, CF.Alive);
}

VAELEN_TEST(Persons, FrozenPersonsAreReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	std::vector<uint32> Regions;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach([&](EntityHandle, const RegionInfo& R) { Regions.push_back(R.Index); });
	std::sort(Regions.begin(), Regions.end());
	uint32 Created = 0;
	for (const uint32 R : Regions)
	{
		Created += PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, R, W.Instance.Now());
	}
	const DetailStats S = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	VAELEN_LOG_INFO(LogPersons, "frozen: persons128=%016llx count=%u", static_cast<unsigned long long>(S.PersonsDigest),
					Created);
	VT_CHECK_EQ(S.PersonsDigest, Hash64{VAELEN_PERSONS_FROZEN_128});
	VT_CHECK_EQ(Created, uint32{VAELEN_PERSONS_COUNT_128});
}
