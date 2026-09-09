// VAELEN - Tests/Population
// Phase 04.06: the LOD bridge - requests, promotions and demotions, crossings,
// five hundred years of alternation.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.06): AELVOR 128 at
// year 300, the two busiest regions detailed in turn every 50 years for 200
// years with lives and the bridge.
// Refrozen 2026-09-08: person indices are taken from a counter that only
// goes up, so a demoted region no longer hands its indices out again to the
// people promoted after it (see PersonCounter).
#define VAELEN_LOD_FROZEN_128 0x25340bd6f788726aull
#define VAELEN_LOD_EMIGRANTS_128 207u
#define VAELEN_LOD_IMMIGRANTS_128 0u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLod);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, LodRules InRules = LodRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Lod = LodTypes::Declare(Instance);
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, LifeRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, InRules);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Bridge.get());
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
		/// Regions by people, busiest first.
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
			std::sort(All.begin(), All.end(), [](const std::pair<uint32, uint32>& A, const std::pair<uint32, uint32>& B)
					  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
			std::vector<uint32> Out;
			for (const auto& [People, Index] : All)
			{
				Out.push_back(Index);
			}
			return Out;
		}
		RegionPopulation* Counts(uint32 Region)
		{
			RegionPopulation* Found = nullptr;
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
		RegionFaith* Faith(uint32 Region)
		{
			RegionFaith* Found = nullptr;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region)
						{
							Found = Instance.Components().GetPool(Ages.Types().Religion.Faith).TryGet(H);
						}
					});
			return Found;
		}
		bool Detailed(uint32 Region) const { return IsDetailed(Instance, Ages.Types(), Persons, Region); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		LodTypes Lod;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<LodSystem> Bridge;
	};

	uint64 WorldPeople(const Run& W)
	{
		return MeasurePopulation(W.Instance, W.Ages.Types().Population).People;
	}

	uint32 Events(const World& W, usize From, EventType<LodPayload> Type, uint32 Region)
	{
		uint32 N = 0;
		const std::vector<Event>& All = W.Log().All();
		for (usize i = From; i < All.size(); ++i)
		{
			N += All[i].Is(Type) && All[i].Get<LodPayload>().Region == Region ? 1u : 0u;
		}
		return N;
	}
} // namespace

VAELEN_TEST(Lod, RequestsAreKeptInOrderAndBounded)
{
	Run W(3);
	VT_CHECK_EQ(LodStateOf(W.Instance, W.Lod).WantedCount, 0u);
	VT_CHECK(!RequestDetail(W.Instance, W.Lod, 0));
	for (uint32 R = 1; R <= LodState::MaxWanted; ++R)
	{
		VT_CHECK(RequestDetail(W.Instance, W.Lod, R * 10));
	}
	VT_CHECK(!RequestDetail(W.Instance, W.Lod, 999)); // full
	VT_CHECK(RequestDetail(W.Instance, W.Lod, 30));	  // already wanted: still true
	VT_CHECK_EQ(LodStateOf(W.Instance, W.Lod).WantedCount, LodState::MaxWanted);
	VT_CHECK(IsWanted(W.Instance, W.Lod, 30) && !IsWanted(W.Instance, W.Lod, 999));
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, 30));
	VT_CHECK(!ReleaseDetail(W.Instance, W.Lod, 30));
	VT_CHECK(!IsWanted(W.Instance, W.Lod, 30));
	const LodState& S = LodStateOf(W.Instance, W.Lod);
	VT_CHECK_EQ(S.WantedCount, LodState::MaxWanted - 1);
	VT_CHECK_EQ(S.Wanted[0], 10u);
	VT_CHECK_EQ(S.Wanted[1], 20u);
	VT_CHECK_EQ(S.Wanted[2], 40u); // compacted, order kept
	VT_CHECK_EQ(S.Wanted[LodState::MaxWanted - 1], 0u);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, 999)); // a slot is free again
	// One state entity only, however often it is asked for.
	uint32 States = 0;
	W.Instance.Components().GetPool(W.Lod.State).ForEach([&](EntityHandle, const LodState&) { ++States; });
	VT_CHECK_EQ(States, 1u);
}

VAELEN_TEST(Lod, PromotionsAndDemotionsFollowTheRequestsAndConserveTheCounts)
{
	LodRules Rules;
	Rules.MaxDetailed = 2;
	Run W(AelvorSeed, Rules);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 4);
	// A promote / demote cycle without a tick between conserves the counts exactly.
	const RegionPopulation Before = *W.Counts(Ranked[0]);
	const RegionFaith FaithBefore = *W.Faith(Ranked[0]);
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Ranked[0], W.Instance.Now()) >
			   0);
	VT_CHECK(DemoteRegion(W.Instance, W.Ages.Types(), W.Persons, Ranked[0]) > 0);
	const RegionPopulation After = *W.Counts(Ranked[0]);
	const RegionFaith FaithAfter = *W.Faith(Ranked[0]);
	VT_CHECK_EQ(After.Total, Before.Total);
	VT_CHECK_EQ(After.Capacity, Before.Capacity);
	VT_CHECK_EQ(After.Majority, Before.Majority);
	for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
	{
		VT_CHECK_EQ(After.Count[S], Before.Count[S]);
		VT_CHECK_EQ(After.Culture[S], Before.Culture[S]);
	}
	for (uint32 S = 0; S < RegionFaith::MaxFaiths; ++S)
	{
		VT_CHECK_EQ(FaithAfter.Adherents[S], FaithBefore.Adherents[S]);
		VT_CHECK_EQ(FaithAfter.Religion[S], FaithBefore.Religion[S]);
	}
	// Requests: three wanted, two detailed (the limit), in request order.
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[2]));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[0]));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[1]));
	const usize Mark = W.Instance.Log().Count();
	W.Ages.Run(1);
	VT_CHECK(W.Detailed(Ranked[2]) && W.Detailed(Ranked[0]) && !W.Detailed(Ranked[1]));
	VT_CHECK_EQ(Events(W.Instance, Mark, RegionPromotedEvent, Ranked[2]), 1u);
	VT_CHECK_EQ(Events(W.Instance, Mark, RegionPromotedEvent, Ranked[0]), 1u);
	VT_CHECK_EQ(Events(W.Instance, Mark, RegionPromotedEvent, Ranked[1]), 0u);
	// A release frees a place: the waiting region gets it the next year.
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Ranked[2]));
	W.Ages.Run(1);
	VT_CHECK(!W.Detailed(Ranked[2]) && W.Detailed(Ranked[0]) && W.Detailed(Ranked[1]));
	VT_CHECK_EQ(Events(W.Instance, Mark, RegionDemotedEvent, Ranked[2]), 1u);
	VT_CHECK_EQ(Events(W.Instance, Mark, RegionPromotedEvent, Ranked[1]), 1u);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Ranked[0]));
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Ranked[1]));
	const LodStats S = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
	VT_CHECK_EQ(S.Detailed, 2u);
	VT_CHECK_EQ(S.Promotions, 3u);
	VT_CHECK_EQ(S.Demotions, 1u);
	VT_CHECK_EQ(S.Refused, 0u);
	// An empty region cannot be detailed: refused, counted, never detailed.
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
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Ranked[0]));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Empty));
	W.Ages.Run(1);
	VT_CHECK(!W.Detailed(Empty));
	VT_CHECK_EQ(MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod).Refused, 1u);
}

VAELEN_TEST(Lod, PeopleCrossTheBorderBothWays)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Ranked()[0];
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	VT_REQUIRE(W.Detailed(Region));
	const uint32 EmigrantsAtMark = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod).Emigrants;
	// Leaving: the land shrinks under them; the crowd goes to a neighbour.
	const uint64 PeopleBefore = WorldPeople(W);
	RegionPopulation* Counts = W.Counts(Region);
	VT_REQUIRE(Counts != nullptr);
	const uint32 Start = Counts->Total;
	Counts->Capacity = Start / 2;
	usize Mark = W.Instance.Log().Count();
	W.Ages.Run(3);
	LodStats S = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
	VT_CHECK(S.Emigrants > 0);
	VT_CHECK_EQ(S.LeftEvents, S.Emigrants);
	VT_CHECK(W.Counts(Region)->Total < Start);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	// Every leaver was an unmarried adult, its person is marked gone, and the world's
	// people are conserved up to the births and deaths of the years.
	uint32 Left = 0;
	uint32 Destination = 0;
	const std::vector<Event>& All = W.Instance.Log().All();
	for (usize i = Mark; i < All.size(); ++i)
	{
		if (!All[i].Is(PersonLeftEvent))
		{
			continue;
		}
		++Left;
		const PersonPayload P = All[i].Get<PersonPayload>();
		VT_CHECK_EQ(P.Region, Region);
		VT_CHECK(P.AgeYears >= 16 && P.AgeYears < 40);
		VT_CHECK(P.Other != 0 && P.Other != Region);
		Destination = P.Other;
		const PersonInfo* Gone = FindPerson(W.Instance, W.Persons, P.Person);
		VT_REQUIRE(Gone != nullptr);
		VT_CHECK_EQ(Gone->State, static_cast<uint8>(LifeState::Gone));
		VT_CHECK_EQ(Gone->Region, Region);
	}
	VT_CHECK_EQ(Left + EmigrantsAtMark, S.Emigrants);
	VT_REQUIRE(Destination != 0);
	VT_CHECK(!W.Detailed(Destination));
	uint32 Born = 0;
	uint32 Died = 0;
	for (usize i = Mark; i < All.size(); ++i)
	{
		Born += All[i].Is(PersonBornEvent) ? 1u : 0u;
		Died += All[i].Is(PersonDiedEvent) ? 1u : 0u;
	}
	// Movers are in the destination's counts: the world lost only its dead
	// (coarse regions moved too, so compare the detailed region and its destination).
	VAELEN_LOG_INFO(LogLod, "region %u: %u -> %u people, %u left for region %u (%u born, %u died); world %llu -> %llu",
					Region, Start, W.Counts(Region)->Total, Left, Destination, Born, Died,
					static_cast<unsigned long long>(PeopleBefore), static_cast<unsigned long long>(WorldPeople(W)));
	VT_CHECK_EQ(W.Counts(Region)->Total + Left + Died, Start + Born);
	// Arriving: a crowded neighbour sends people into the detailed region's room.
	Counts = W.Counts(Region);
	Counts->Capacity = Counts->Total * 2; // wide open: well under the room line
	RegionPopulation* Crowd = W.Counts(Destination);
	VT_REQUIRE(Crowd != nullptr && Crowd->Majority != 0);
	Crowd->Capacity = Crowd->Total; // the land shrinks under the neighbour: full
	const uint32 CrowdBefore = Crowd->Total;
	Mark = W.Instance.Log().Count();
	W.Ages.Run(1);
	S = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
	VT_CHECK(S.Immigrants > 0);
	VT_CHECK_EQ(S.ArrivedEvents, S.Immigrants);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	uint32 Arrived = 0;
	for (usize i = Mark; i < All.size(); ++i)
	{
		if (!All[i].Is(PersonArrivedEvent))
		{
			continue;
		}
		++Arrived;
		const PersonPayload P = All[i].Get<PersonPayload>();
		VT_CHECK_EQ(P.Region, Region);
		VT_CHECK(P.AgeYears >= 16 && P.AgeYears < 40);
		const PersonInfo* Person = FindPerson(W.Instance, W.Persons, P.Person);
		VT_REQUIRE(Person != nullptr);
		VT_CHECK_EQ(Person->Region, Region);
		VT_CHECK(Person->Culture != 0 && Person->Language != 0);
		VT_CHECK_EQ(Person->State, static_cast<uint8>(LifeState::Alive));
		VT_CHECK_EQ(Person->Mother, 0u);
	}
	VT_CHECK_EQ(Arrived, S.Immigrants);
	VAELEN_LOG_INFO(LogLod, "region %u took %u arrivals from region %u (%u -> %u people there)", Region, Arrived,
					Destination, CrowdBefore, W.Counts(Destination)->Total);
	VT_CHECK(W.Counts(Destination)->Total < CrowdBefore);
	// Rules: no crossings when the shares are zero.
	LodRules Closed;
	Closed.LeaveSharePerMille = 0;
	Closed.ArriveSharePerMille = 0;
	Run X(AelvorSeed, Closed);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	X.Ages.Run(1);
	X.Counts(Region)->Capacity = X.Counts(Region)->Total / 2;
	X.Ages.Run(3);
	const LodStats SX = MeasureLod(X.Instance, X.Ages.Types(), X.Persons, X.Lod);
	VT_CHECK_EQ(SX.Emigrants, 0u);
	VT_CHECK_EQ(SX.Immigrants, 0u);
}

VAELEN_TEST(Lod, FiveHundredYearsOfAlternationKeepEveryInvariant)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const std::vector<uint32> Ranked = A.Ranked();
	VT_REQUIRE(Ranked.size() >= 3);
	uint32 Failures = 0;
	uint64 MinPeople = 0xffffffffffffffffull;
	std::vector<uint8> Image;
	for (uint32 Year = 1; Year <= 500; ++Year)
	{
		if (Year % 25 == 1)
		{
			// Every 25 years another pair of the three busiest regions is wanted.
			const uint32 Turn = (Year / 25) % 3;
			for (uint32 i = 0; i < 3; ++i)
			{
				if (i == Turn)
				{
					ReleaseDetail(A.Instance, A.Lod, Ranked[i]);
					ReleaseDetail(B.Instance, B.Lod, Ranked[i]);
				}
				else
				{
					VT_CHECK(RequestDetail(A.Instance, A.Lod, Ranked[i]));
					VT_CHECK(RequestDetail(B.Instance, B.Lod, Ranked[i]));
				}
			}
		}
		A.Ages.Run(1);
		B.Ages.Run(1);
		if (Year == 250)
		{
			SaveSnapshot(A.Instance, Image);
		}
		if (Year % 10 != 0)
		{
			continue;
		}
		const DetailStats D = MeasureDetail(A.Instance, A.Ages.Types(), A.Persons);
		if (D.Inconsistent != 0 || D.DetailedRegions == 0 || D.DetailedRegions > 2)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u detailed regions, %u inconsistent", Year, D.DetailedRegions,
						 D.Inconsistent);
		}
		MinPeople = std::min(MinPeople, WorldPeople(A));
		if (ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the two worlds differ", Year);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK(MinPeople > 0);
	const LodStats S = MeasureLod(A.Instance, A.Ages.Types(), A.Persons, A.Lod);
	VAELEN_LOG_INFO(
		LogLod,
		"500 years: %u promotions, %u demotions, %u emigrants, %u immigrants, %u refused, people %llu (min %llu)",
		S.Promotions, S.Demotions, S.Emigrants, S.Immigrants, S.Refused,
		static_cast<unsigned long long>(WorldPeople(A)), static_cast<unsigned long long>(MinPeople));
	VT_CHECK(S.Promotions >= 20 && S.Demotions >= 19);
	VT_CHECK_EQ(S.Promotions, S.Demotions + S.Detailed);
	// The snapshot of year 250 continues to the same year 500.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	for (uint32 Year = 251; Year <= 500; ++Year)
	{
		if (Year % 25 == 1)
		{
			const uint32 Turn = (Year / 25) % 3;
			for (uint32 i = 0; i < 3; ++i)
			{
				if (i == Turn)
				{
					ReleaseDetail(R.Instance, R.Lod, Ranked[i]);
				}
				else
				{
					RequestDetail(R.Instance, R.Lod, Ranked[i]);
				}
			}
		}
		R.Ages.Run(1);
	}
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
}

VAELEN_TEST(Lod, FrozenBridgeIsReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	for (uint32 Year = 1; Year <= 200; ++Year)
	{
		if (Year % 50 == 1)
		{
			const uint32 In = Ranked[(Year / 50) % 2];
			const uint32 Out = Ranked[1 - (Year / 50) % 2];
			ReleaseDetail(W.Instance, W.Lod, Out);
			VT_CHECK(RequestDetail(W.Instance, W.Lod, In));
		}
		W.Ages.Run(1);
	}
	const DetailStats D = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	const LodStats S = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
	VAELEN_LOG_INFO(LogLod, "frozen: lod128=%016llx emigrants=%u immigrants=%u (%u promotions, %u demotions)",
					static_cast<unsigned long long>(D.PersonsDigest), S.Emigrants, S.Immigrants, S.Promotions,
					S.Demotions);
	VT_CHECK_EQ(D.PersonsDigest, Hash64{VAELEN_LOD_FROZEN_128});
	VT_CHECK_EQ(S.Emigrants, uint32{VAELEN_LOD_EMIGRANTS_128});
	VT_CHECK_EQ(S.Immigrants, uint32{VAELEN_LOD_IMMIGRANTS_128});
	VT_CHECK_EQ(D.Inconsistent, 0u);
	VT_CHECK_EQ(D.DetailedRegions, 1u);
}

VAELEN_TEST(Lod, APersonWalksToTheNextRegionAndBothGrainsStillAgree)
{
	// A crossing turns a person into counts, because the far side is coarse.
	// This is the other move: both sides are detailed, the person stays a
	// person, and the coarse counts of both regions have to end up telling the
	// same story as the persons in them. 04.06 owns that, which is why the
	// player of Phase 10 walks through here rather than setting a field.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 From = W.Ranked()[0];
	VT_CHECK(RequestDetail(W.Instance, W.Lod, From));
	const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Ages.Types().World.Regions);
	VT_REQUIRE(From < Graph.Neighbours.size() && !Graph.Neighbours[From].empty());
	uint32 To = 0;
	for (const uint32 R : W.Ranked())
	{
		for (const uint16 N : Graph.Neighbours[From])
		{
			To = To == 0 && uint32{N} == R ? R : To;
		}
	}
	VT_REQUIRE(To != 0);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, To));
	W.Ages.Run(1);
	VT_REQUIRE(W.Detailed(From) && W.Detailed(To));

	// Somebody living there, by lowest index so the choice is the same in every run.
	uint32 Who = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (P.Region == From && P.State == static_cast<uint8>(LifeState::Alive))
				{
					Who = Who == 0 || P.Index < Who ? P.Index : Who;
				}
			});
	VT_REQUIRE(Who != 0);
	const uint32 HadFrom = W.Counts(From)->Total;
	const uint32 HadTo = W.Counts(To)->Total;
	const usize Mark = W.Instance.Log().Count();

	VT_CHECK(MovePerson(W.Instance, W.Ages.Types(), W.Persons, Who, To, W.Instance.Now()));
	VAELEN_LOG_INFO(LogLod, "person %u walked from region %u (%u people) to region %u (%u people)", Who, From,
					W.Counts(From)->Total, To, W.Counts(To)->Total);
	VT_CHECK_EQ(W.Counts(From)->Total, HadFrom - 1u);
	VT_CHECK_EQ(W.Counts(To)->Total, HadTo + 1u);
	VT_CHECK_MSG(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, From), "the region left behind still adds up");
	VT_CHECK_MSG(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, To), "and so does the one walked into");
	const PersonInfo* Moved = FindPerson(W.Instance, W.Persons, Who);
	VT_REQUIRE(Moved != nullptr);
	VT_CHECK_EQ(Moved->Region, To);
	VT_CHECK_MSG(Moved->State == static_cast<uint8>(LifeState::Alive), "and is still a person, not a count");

	uint32 Events = 0;
	const std::vector<Event>& All = W.Instance.Log().All();
	for (usize i = Mark; i < All.size(); ++i)
	{
		if (All[i].Is(PersonMovedEvent))
		{
			++Events;
			const PersonPayload P = All[i].Get<PersonPayload>();
			VT_CHECK_EQ(P.Person, Who);
			VT_CHECK_EQ(P.Region, To);
			VT_CHECK_EQ(P.Other, From); // the region left behind
		}
	}
	VT_CHECK_EQ(Events, 1u);

	// And what it refuses: the same region, an unknown person, and anywhere the
	// world is not simulating person by person.
	VT_CHECK(!MovePerson(W.Instance, W.Ages.Types(), W.Persons, Who, To, W.Instance.Now()));
	VT_CHECK(!MovePerson(W.Instance, W.Ages.Types(), W.Persons, 999999u, From, W.Instance.Now()));
	VT_CHECK(!MovePerson(W.Instance, W.Ages.Types(), W.Persons, Who, 0u, W.Instance.Now()));
	uint32 Coarse = 0;
	for (const uint32 R : W.Ranked())
	{
		Coarse = Coarse == 0 && R != From && R != To ? R : Coarse;
	}
	VT_REQUIRE(Coarse != 0);
	VT_CHECK_MSG(!MovePerson(W.Instance, W.Ages.Types(), W.Persons, Who, Coarse, W.Instance.Now()),
				 "a coarse region has no persons to walk into, only a count of them");
	VT_CHECK_EQ(FindPerson(W.Instance, W.Persons, Who)->Region, To);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, To));
}

VAELEN_TEST(Lod, AHeldRegionStaysDetailedWhateverElseTheWorldWants)
{
	// Phase 11's colony is not a region that happens to be interesting this
	// decade: it is the place the game is played, and it has to be at the fine
	// grain on the first tick and the hundred-thousandth alike. Wanting is a
	// request weighed against MaxDetailed and against everything else wanted;
	// holding is not.
	Run Scout(AelvorSeed);
	VT_REQUIRE(Scout.Ages.Generate(Run::Square(128), 300));
	const uint32 Colony = Scout.Ranked()[0];
	VT_REQUIRE(Colony != 0);

	LodRules Rules;
	Rules.MaxDetailed = 2;
	Rules.Held = Colony;
	Run W(AelvorSeed, Rules);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));

	// Nobody asked for it. It is detailed anyway, on the first year the bridge
	// runs, because the rules hold it there.
	VT_CHECK(!IsWanted(W.Instance, W.Lod, Colony));
	W.Ages.Run(1);
	VT_CHECK_MSG(W.Detailed(Colony), "a held region is detailed without being asked for");
	const uint32 People = W.Counts(Colony)->Total;
	VAELEN_LOG_INFO(LogLod, "the colony is region %u with %u people, held with MaxDetailed=%u", Colony, People,
					Rules.MaxDetailed);
	VT_CHECK(People > 0);

	// Now ask for as many others as the world will take, more than the limit
	// allows. The held one is not what gives way.
	const std::vector<uint32> Ranked = W.Ranked();
	uint32 Asked = 0;
	for (const uint32 R : Ranked)
	{
		if (R != Colony && Asked < 4)
		{
			VT_CHECK(RequestDetail(W.Instance, W.Lod, R));
			++Asked;
		}
	}
	W.Ages.Run(3);
	uint32 Fine = 0;
	W.Instance.Components().GetPool(W.Persons.Detail).ForEach([&](EntityHandle, const RegionDetail&) { ++Fine; });
	VAELEN_LOG_INFO(LogLod, "%u region(s) asked for beyond the colony, %u detailed under a limit of %u", Asked, Fine,
					Rules.MaxDetailed);
	VT_CHECK_MSG(W.Detailed(Colony), "the held region is still detailed after four others were asked for");
	VT_CHECK_MSG(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Colony), "and both grains still agree about it");

	// And releasing everything else does not release it either.
	for (const uint32 R : Ranked)
	{
		ReleaseDetail(W.Instance, W.Lod, R);
	}
	W.Ages.Run(2);
	VT_CHECK_MSG(W.Detailed(Colony), "nor does letting every other region go");
	VT_CHECK_MSG(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Colony), "and it is still coherent");

	// A world that holds nothing is the world every phase before this one had.
	{
		Run Plain(AelvorSeed);
		VT_REQUIRE(Plain.Ages.Generate(Run::Square(128), 300));
		Plain.Ages.Run(1);
		VT_CHECK_MSG(!Plain.Detailed(Colony), "with Held = 0 nothing is detailed unasked");
	}
}

VAELEN_TEST(Lod, WhatHoldingARegionCostsPerTick)
{
	// Measured rather than asserted, in the manner of the mini-world baseline:
	// the number that matters to Phase 11 is what the fine grain costs when it
	// is never given up, against the same world at the coarse grain, and it is
	// worth having on the record before anything is built on top of it.
	const auto Elapsed = [](const std::chrono::steady_clock::time_point Start) {
		return std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - Start)
			.count();
	};
	Run Coarse(AelvorSeed);
	VT_REQUIRE(Coarse.Ages.Generate(Run::Square(128), 300));
	const uint32 Colony = Coarse.Ranked()[0];
	VT_REQUIRE(Colony != 0);
	const auto ColdStart = std::chrono::steady_clock::now();
	Coarse.Ages.Run(20);
	const double Without = Elapsed(ColdStart);

	LodRules Rules;
	Rules.Held = Colony;
	Run Fine(AelvorSeed, Rules);
	VT_REQUIRE(Fine.Ages.Generate(Run::Square(128), 300));
	Fine.Ages.Run(1); // the hold takes effect on the first yearly tick
	VT_REQUIRE(Fine.Detailed(Colony));
	const uint32 People = Fine.Counts(Colony)->Total;
	const auto WarmStart = std::chrono::steady_clock::now();
	Fine.Ages.Run(20);
	const double With = Elapsed(WarmStart);

	const double Years = 20.0;
	VAELEN_LOG_INFO(LogLod,
					"twenty years at 128: %.2f s coarse, %.2f s holding region %u at the fine grain with %u people "
					"(%.0f%% more, %.1f ms a year, %.1f us a tick)",
					Without, With, Colony, People, Without > 0.0 ? (With / Without - 1.0) * 100.0 : 0.0,
					With / Years * 1000.0, With / (Years * 8640.0) * 1e6);
	VT_CHECK_MSG(With > 0.0 && Without > 0.0, "both worlds actually ran");
	VT_CHECK_MSG(Fine.Detailed(Colony), "and the held region was detailed for all of it");
	VT_CHECK_MSG(IsConsistent(Fine.Instance, Fine.Ages.Types(), Fine.Persons, Colony), "and stayed coherent");
	// The people of the held region are simulated one by one for twenty years,
	// so the world with it must do more work than the world without. Anything
	// else would mean the hold was not doing anything.
	VT_CHECK_MSG(With > Without * 1.05, "holding a region at the fine grain costs something real");
}
