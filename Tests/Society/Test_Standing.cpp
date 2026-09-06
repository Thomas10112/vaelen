// VAELEN - Tests/Society
// Phase 05.02: standing - scores, ranks, tiers, offices, the elite of a region.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.02): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 100 years with
// lives, families, traits, the bridge, organisations and standing.
#define VAELEN_STANDING_FROZEN_128 0xc62c5f7a89880cb9ull
#define VAELEN_STANDING_RANKED_128 1045u
#define VAELEN_STANDING_ELITE_128 52u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogStanding);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, StandingRules InRules = StandingRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, InRules);
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
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
		StandingStats Stats(uint32 Region = 0) const { return MeasureStanding(Instance, Persons, Standing, Region); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
	};
} // namespace

VAELEN_TEST(Standing, TheScoreFollowsTheRules)
{
	const StandingRules Rules;
	PersonInfo P;
	P.Born = 0;
	const uint64 At40 = TicksPerYear * 40;
	PersonTraits T;
	for (uint32 K = 0; K < static_cast<uint32>(Trait::Count); ++K)
	{
		T.Traits[K] = 128;
	}
	// A common adult of 40 with ordinary traits and no skill: the age band and the traits.
	const uint32 Base = StandingScore(P, &T, 0, 0, At40, Rules);
	VT_CHECK_EQ(Base, Rules.AgeBandPoints[1] + 128u * Rules.TraitPointsPerMille / 1000u);
	// A house of ten adds ten members' points; the head adds the headship; a seat and its head add theirs.
	VT_CHECK_EQ(StandingScore(P, &T, 10, 0, At40, Rules), Base + 10u * Rules.HousePointsPerMember);
	VT_CHECK_EQ(StandingScore(P, &T, 10, static_cast<uint8>(Office::HeadOfHouse), At40, Rules),
				Base + 10u * Rules.HousePointsPerMember + Rules.HeadOfHousePoints);
	const uint8 Both = static_cast<uint8>(static_cast<uint8>(Office::Seat) | static_cast<uint8>(Office::HeadOfSeat));
	VT_CHECK_EQ(StandingScore(P, &T, 0, Both, At40, Rules), Base + Rules.SeatPoints + Rules.HeadOfSeatPoints);
	// The house is capped; a skill counts by its best; traits by charm and will only.
	VT_CHECK_EQ(StandingScore(P, &T, 1000, 0, At40, Rules), Base + Rules.HouseMembersCap * Rules.HousePointsPerMember);
	T.Skills[static_cast<uint32>(Skill::Craft)] = 200;
	T.Skills[static_cast<uint32>(Skill::Lore)] = 100;
	VT_CHECK_EQ(StandingScore(P, &T, 0, 0, At40, Rules), Base + 200u * Rules.SkillPointsPerMille / 1000u);
	T.Traits[static_cast<uint32>(Trait::Vigour)] = 255;
	VT_CHECK_EQ(StandingScore(P, &T, 0, 0, At40, Rules), Base + 200u * Rules.SkillPointsPerMille / 1000u);
	T.Traits[static_cast<uint32>(Trait::Charm)] = 255;
	T.Traits[static_cast<uint32>(Trait::Will)] = 255;
	VT_CHECK(StandingScore(P, &T, 0, 0, At40, Rules) > Base + 200u * Rules.SkillPointsPerMille / 1000u);
	// Age bands: youth, prime, maturity, old age.
	VT_CHECK_EQ(StandingScore(P, nullptr, 0, 0, TicksPerYear * 20, Rules), Rules.AgeBandPoints[0]);
	VT_CHECK_EQ(StandingScore(P, nullptr, 0, 0, TicksPerYear * 50, Rules), Rules.AgeBandPoints[2]);
	VT_CHECK_EQ(StandingScore(P, nullptr, 0, 0, TicksPerYear * 70, Rules), Rules.AgeBandPoints[3]);
}

VAELEN_TEST(Standing, RanksAndTiersOrderTheLivingAdults)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(5);
	const StandingStats S = W.Stats(Region);
	VT_CHECK(S.Ranked > 500);
	VT_CHECK_EQ(S.Stale, 0u);
	// Tiers by share: about 5 percent elite, 15 percent notable, the rest common.
	VT_CHECK(S.PerTier[2] * 1000 >= S.Ranked * 45 && S.PerTier[2] * 1000 <= S.Ranked * 55);
	VT_CHECK(S.PerTier[1] * 1000 >= S.Ranked * 140 && S.PerTier[1] * 1000 <= S.Ranked * 160);
	VT_CHECK_EQ(S.PerTier[0] + S.PerTier[1] + S.PerTier[2], S.Ranked);
	// Every ranked person is a living adult of the region; scores fall with rank and tiers with score.
	uint32 Bad = 0;
	std::vector<std::pair<uint8, uint32>> ByRank; // rank, score
	uint32 EliteMin = 0xffffffffu;
	uint32 NotableMax = 0;
	uint32 NotableMin = 0xffffffffu;
	uint32 CommonMax = 0;
	W.Instance.Components()
		.GetPool(W.Standing.Standing)
		.ForEach(
			[&](EntityHandle H, const PersonStanding& St)
			{
				const PersonInfo* P = W.Instance.Components().GetPool(W.Persons.Person).TryGet(H);
				Bad += P == nullptr || P->State != static_cast<uint8>(LifeState::Alive) || P->Region != Region ||
							   AgeYears(*P, W.Instance.Now()) < 16
						   ? 1u
						   : 0u;
				ByRank.push_back({St.Rank, St.Score});
				if (St.Tier_ == static_cast<uint8>(Tier::Elite))
				{
					EliteMin = std::min(EliteMin, St.Score);
				}
				else if (St.Tier_ == static_cast<uint8>(Tier::Notable))
				{
					NotableMax = std::max(NotableMax, St.Score);
					NotableMin = std::min(NotableMin, St.Score);
				}
				else
				{
					CommonMax = std::max(CommonMax, St.Score);
				}
			});
	VT_CHECK_EQ(Bad, 0u);
	std::sort(ByRank.begin(), ByRank.end());
	uint32 Inversions = 0;
	for (usize i = 1; i < ByRank.size(); ++i)
	{
		Inversions += ByRank[i].second < ByRank[i - 1].second ? 1u : 0u;
	}
	VT_CHECK_EQ(Inversions, 0u);
	VT_CHECK(ByRank.front().first == 0 && ByRank.back().first == 255);
	VT_CHECK(EliteMin >= NotableMax && NotableMin >= CommonMax);
	// The elite: heads of houses and seats sit above the common; the council's head is in it.
	std::vector<uint32> Elite;
	EliteOf(W.Instance, W.Persons, W.Standing, Region, Elite);
	VT_CHECK_EQ(static_cast<uint32>(Elite.size()), S.PerTier[2]);
	uint32 EliteWithOffice = 0;
	for (const uint32 E : Elite)
	{
		const PersonStanding* St = StandingOf(W.Instance, W.Persons, W.Standing, E);
		VT_REQUIRE(St != nullptr);
		VT_CHECK_EQ(St->Tier_, static_cast<uint8>(Tier::Elite));
		EliteWithOffice += St->Offices != 0 ? 1u : 0u;
	}
	VT_CHECK(EliteWithOffice * 10 >= Elite.size() * 9);
	std::vector<OrganizationInfo> Seated;
	OrganizationsOf(W.Instance, W.Organizations, Region, Seated);
	VT_REQUIRE(!Seated.empty() && Seated[0].Head != 0);
	const PersonStanding* CouncilHead = StandingOf(W.Instance, W.Persons, W.Standing, Seated[0].Head);
	VT_REQUIRE(CouncilHead != nullptr);
	VT_CHECK_EQ(CouncilHead->Offices & static_cast<uint8>(Office::HeadOfSeat), static_cast<uint8>(Office::HeadOfSeat));
	VT_CHECK(std::find(Elite.begin(), Elite.end(), Seated[0].Head) != Elite.end());
	std::vector<uint32> Top;
	EliteOf(W.Instance, W.Persons, W.Standing, Region, Top, 3);
	VT_CHECK_EQ(static_cast<uint32>(Top.size()), 3u);
	VT_CHECK(StandingOf(W.Instance, W.Persons, W.Standing, Top[0])->Rank == 255);
	VAELEN_LOG_INFO(LogStanding,
					"region %u: %u ranked (%u elite, %u notable, %u common), %u with an office, elite score from %u, "
					"common up to %u",
					Region, S.Ranked, S.PerTier[2], S.PerTier[1], S.PerTier[0], S.WithOffice, EliteMin, CommonMax);
	VT_CHECK(StandingOf(W.Instance, W.Persons, W.Standing, 0xfffffff0u) == nullptr);
}

VAELEN_TEST(Standing, StandingsFollowTheLivesAndTheGrain)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	uint32 Failures = 0;
	for (uint32 Year = 1; Year <= 40; ++Year)
	{
		W.Ages.Run(1);
		const StandingStats S = W.Stats(Region);
		const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
		// Adults as the yearly tick saw them: the year's first tick, not its end.
		const uint64 YearTick = W.Instance.Now() - TicksPerYear;
		uint32 Adults = 0;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					Adults += P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) &&
									  AgeYears(P, YearTick) >= 16
								  ? 1u
								  : 0u;
				});
		if (S.Stale != 0 || S.Ranked != Adults || L.Alive == 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u ranked for %u adults, %u stale", Year, S.Ranked, Adults, S.Stale);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	// Demoted: the standings go with the persons; promoted again: ranked again the same year.
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Stats().Ranked, 0u);
	uint32 Standings = 0;
	W.Instance.Components()
		.GetPool(W.Standing.Standing)
		.ForEach([&](EntityHandle, const PersonStanding&) { ++Standings; });
	VT_CHECK_EQ(Standings, 0u);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	VT_CHECK(W.Stats(Region).Ranked > 500);
	// Rules: nobody elite when the share is zero; everyone common when both are.
	StandingRules Flat;
	Flat.ElitePerMille = 0;
	Flat.NotablePerMille = 0;
	Run X(AelvorSeed, Flat);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, X.Busiest()));
	X.Ages.Run(2);
	const StandingStats SX = X.Stats();
	VT_CHECK(SX.Ranked > 0);
	VT_CHECK_EQ(SX.PerTier[2] + SX.PerTier[1], 0u);
	VT_CHECK_EQ(SX.PerTier[0], SX.Ranked);
	// Without a detailed region the system does nothing.
	Run Q(11);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(64), 60));
	const Hash64 Digest = ComputeStateDigest(Q.Instance);
	RandomStream Stream(1);
	TickContext Tc;
	Tc.Tick = Q.Instance.Now();
	Tc.Entities = &Q.Instance.Entities();
	Tc.Components = &Q.Instance.Components();
	Tc.Random = &Stream;
	Tc.Events = &Q.Instance.Events();
	Q.Ranks->Tick(Tc);
	VT_CHECK_EQ(ComputeStateDigest(Q.Instance), Digest);
}

VAELEN_TEST(Standing, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(20);
	B.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	A.Ages.Run(20);
	R.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	// Frozen: the busiest region of AELVOR 128 for 100 years.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(100);
	const StandingStats S = W.Stats();
	VAELEN_LOG_INFO(LogStanding, "frozen: standing128=%016llx ranked=%u elite=%u (%u notable, %u with an office)",
					static_cast<unsigned long long>(S.Digest), S.Ranked, S.PerTier[2], S.PerTier[1], S.WithOffice);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_STANDING_FROZEN_128});
	VT_CHECK_EQ(S.Ranked, uint32{VAELEN_STANDING_RANKED_128});
	VT_CHECK_EQ(S.PerTier[2], uint32{VAELEN_STANDING_ELITE_128});
	VT_CHECK_EQ(S.Stale, 0u);
}
