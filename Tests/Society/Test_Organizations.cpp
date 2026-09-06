// VAELEN - Tests/Society
// Phase 05.01: organisations - councils and temples, seats, heads, coarse counts.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Organizations.h"

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

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.01): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 100 years with
// lives, families, traits, the bridge and organisations.
#define VAELEN_ORGANIZATIONS_FROZEN_128 0xe4725ff3ae419103ull
#define VAELEN_ORGANIZATIONS_TOTAL_128 2u
#define VAELEN_ORGANIZATIONS_MEMBERS_128 71u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogOrganizations);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, OrganizationRules InRules = OrganizationRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, InRules);
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
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
		const RegionFaith* Faith(uint32 Region) const
		{
			const RegionFaith* Found = nullptr;
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
		OrganizationStats Stats() const { return MeasureOrganizations(Instance, Ages.Types(), Persons, Organizations); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
	};

	uint8 PietyOf(const Run& W, uint32 Person)
	{
		uint8 Out = 0;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == Person)
					{
						const PersonTraits* T = W.Instance.Components().GetPool(W.Traits.Traits).TryGet(H);
						Out = T != nullptr ? T->Traits[static_cast<uint32>(Trait::Piety)] : 0u;
					}
				});
		return Out;
	}

	uint32 Count(const World& W, EventType<OrganizationPayload> Type, uint32 Organization = 0)
	{
		uint32 N = 0;
		for (const Event& E : W.Log().All())
		{
			N += E.Is(Type) && (Organization == 0 || E.Get<OrganizationPayload>().Organization == Organization) ? 1u
																												: 0u;
		}
		return N;
	}
} // namespace

VAELEN_TEST(Organizations, ACouncilOfHousesAndATempleOfTheFaithAreFounded)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1); // promoted, houses founded, seats filled, all in the one yearly tick
	std::vector<OrganizationInfo> Seated;
	OrganizationsOf(W.Instance, W.Organizations, Region, Seated);
	VT_REQUIRE(Seated.size() == 2);
	const OrganizationInfo& Council = Seated[0];
	const OrganizationInfo& Temple = Seated[1];
	VT_CHECK_EQ(Council.Kind, static_cast<uint32>(OrganizationKind::Council));
	VT_CHECK_EQ(Temple.Kind, static_cast<uint32>(OrganizationKind::Temple));
	VT_CHECK_EQ(Council.Index, 1u);
	VT_CHECK_EQ(Temple.Index, 2u);
	VT_CHECK(Council.Disbanded == 0 && Temple.Disbanded == 0);
	VT_CHECK(Council.Founded >= TicksPerYear * 300 && Council.Identity != 0 && Temple.Identity != Council.Identity);
	const RegionFaith* Faith = W.Faith(Region);
	VT_REQUIRE(Faith != nullptr && Faith->Majority != 0);
	VT_CHECK_EQ(Temple.Religion, Faith->Majority);
	VT_CHECK_EQ(Council.Religion, 0u);
	// The council seats the heads of the largest houses; its head is the eldest.
	std::vector<uint32> Members;
	MembersOf(W.Instance, W.Persons, W.Organizations, Council.Index, Members);
	VT_CHECK_EQ(static_cast<uint32>(Members.size()), Council.Seats);
	VT_CHECK_EQ(Council.Members, Council.Seats);
	uint64 EldestBorn = 0xffffffffffffffffull;
	uint32 SmallestSeatedHouse = 0xffffffffu;
	for (const uint32 M : Members)
	{
		const PersonInfo* P = FindPerson(W.Instance, W.Persons, M);
		VT_REQUIRE(P != nullptr);
		VT_CHECK_EQ(P->Region, Region);
		VT_CHECK_EQ(P->State, static_cast<uint8>(LifeState::Alive));
		VT_CHECK(AgeYears(*P, W.Instance.Now()) >= 20);
		std::vector<uint32> House;
		FamilyMembers(W.Instance, W.Persons, P->Family, House);
		SmallestSeatedHouse = std::min(SmallestSeatedHouse, static_cast<uint32>(House.size()));
		bool Head = false;
		W.Instance.Components()
			.GetPool(W.Families.Family)
			.ForEach([&](EntityHandle, const FamilyInfo& F) { Head = Head || (F.Index == P->Family && F.Head == M); });
		VT_CHECK(Head);
		EldestBorn = std::min(EldestBorn, P->Born);
		VT_CHECK_EQ(OrganizationOf(W.Instance, W.Persons, W.Organizations, M), Council.Index);
	}
	const PersonInfo* CouncilHead = FindPerson(W.Instance, W.Persons, Council.Head);
	VT_REQUIRE(CouncilHead != nullptr);
	VT_CHECK_EQ(CouncilHead->Born, EldestBorn);
	VT_CHECK(std::find(Members.begin(), Members.end(), Council.Head) != Members.end());
	// No larger house is left outside the council.
	uint32 LargestOutside = 0;
	W.Instance.Components()
		.GetPool(W.Families.Family)
		.ForEach(
			[&](EntityHandle, const FamilyInfo& F)
			{
				if (F.Head == 0 || F.Region != Region ||
					std::find(Members.begin(), Members.end(), F.Head) != Members.end())
				{
					return;
				}
				const PersonInfo* Head = FindPerson(W.Instance, W.Persons, F.Head);
				if (Head == nullptr || AgeYears(*Head, W.Instance.Now()) < 20 ||
					OrganizationOf(W.Instance, W.Persons, W.Organizations, F.Head) != 0)
				{
					return;
				}
				std::vector<uint32> House;
				FamilyMembers(W.Instance, W.Persons, F.Index, House);
				LargestOutside = std::max(LargestOutside, static_cast<uint32>(House.size()));
			});
	VT_CHECK(LargestOutside <= SmallestSeatedHouse);
	// The temple seats the most pious of the faith; its head is the most pious.
	MembersOf(W.Instance, W.Persons, W.Organizations, Temple.Index, Members);
	VT_CHECK_EQ(static_cast<uint32>(Members.size()), Temple.Seats);
	VT_CHECK(Temple.Seats >= 10 && Temple.Seats <= 64);
	uint8 LeastPious = 255;
	uint8 MostPious = 0;
	for (const uint32 M : Members)
	{
		const PersonInfo* P = FindPerson(W.Instance, W.Persons, M);
		VT_REQUIRE(P != nullptr);
		VT_CHECK_EQ(P->Religion, Temple.Religion);
		LeastPious = std::min(LeastPious, PietyOf(W, M));
		MostPious = std::max(MostPious, PietyOf(W, M));
	}
	VT_CHECK_EQ(PietyOf(W, Temple.Head), MostPious);
	uint8 MostPiousOutside = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				if (P.Region != Region || P.State != static_cast<uint8>(LifeState::Alive) ||
					P.Religion != Temple.Religion || AgeYears(P, W.Instance.Now()) < 20 ||
					W.Instance.Components().GetPool(W.Organizations.Member).TryGet(H) != nullptr)
				{
					return;
				}
				MostPiousOutside = std::max(MostPiousOutside, PietyOf(W, P.Index));
			});
	VT_CHECK(MostPiousOutside <= LeastPious);
	VAELEN_LOG_INFO(
		LogOrganizations,
		"region %u: council of %u houses (head %u), temple of faith %u with %u seats (head %u, piety %u..%u)", Region,
		Council.Members, Council.Head, Temple.Religion, Temple.Members, Temple.Head, LeastPious, MostPious);
	// One founding event each, one head seated each.
	VT_CHECK_EQ(Count(W.Instance, OrganizationFoundedEvent), 2u);
	VT_CHECK_EQ(Count(W.Instance, HeadSeatedEvent, Council.Index), 1u);
	VT_CHECK_EQ(Count(W.Instance, MemberJoinedEvent, Council.Index), Council.Seats);
	VT_CHECK_EQ(OrganizationOf(W.Instance, W.Persons, W.Organizations, 0xfffffff0u), 0u);
	VT_CHECK(OrganizationKindName(OrganizationKind::Council)[0] == 'c' &&
			 OrganizationKindName(OrganizationKind::Count)[0] == '?');
}

VAELEN_TEST(Organizations, SeatsFollowTheLivesForSixtyYears)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	uint32 Failures = 0;
	uint32 Left = 0;
	for (uint32 Year = 1; Year <= 60; ++Year)
	{
		W.Ages.Run(1);
		const OrganizationStats S = W.Stats();
		if (S.Astray != 0 || S.CountMismatch != 0 || S.HeadsAlive != S.Alive || (Year > 2 && S.Alive != 2))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u alive, %u astray, %u mismatched, %u heads", Year, S.Alive, S.Astray,
						 S.CountMismatch, S.HeadsAlive);
		}
		std::vector<OrganizationInfo> Seated;
		OrganizationsOf(W.Instance, W.Organizations, Region, Seated);
		for (const OrganizationInfo& O : Seated)
		{
			if (O.Members > O.Seats)
			{
				++Failures;
				VT_CHECK_MSG(false, "year %u: organisation %u has %u members for %u seats", Year, O.Index, O.Members,
							 O.Seats);
			}
		}
		Left = Count(W.Instance, MemberLeftEvent);
	}
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK(Left > 10); // members died and were released
	VT_CHECK(Count(W.Instance, MemberJoinedEvent) > Left);
	VT_CHECK(Count(W.Instance, HeadSeatedEvent) > 2); // heads died and were replaced
	VT_CHECK_EQ(Count(W.Instance, OrganizationDisbandedEvent), 0u);
	// A member belongs to one organisation and is a living person of the seat's region.
	uint32 Bad = 0;
	W.Instance.Components()
		.GetPool(W.Organizations.Member)
		.ForEach(
			[&](EntityHandle H, const Membership& M)
			{
				const PersonInfo* P = W.Instance.Components().GetPool(W.Persons.Person).TryGet(H);
				Bad += P == nullptr || P->State != static_cast<uint8>(LifeState::Alive) || P->Region != Region ||
							   (M.Organization != 1 && M.Organization != 2) || M.Role > 1
						   ? 1u
						   : 0u;
			});
	VT_CHECK_EQ(Bad, 0u);
}

VAELEN_TEST(Organizations, CoarseSeatsKeepTheirCountAndAPromotionFillsThemAgain)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(20);
	std::vector<OrganizationInfo> Before;
	OrganizationsOf(W.Instance, W.Organizations, Region, Before);
	VT_REQUIRE(Before.size() == 2 && Before[0].Members > 0 && Before[1].Members > 0);
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(10);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	std::vector<OrganizationInfo> Coarse;
	OrganizationsOf(W.Instance, W.Organizations, Region, Coarse);
	VT_REQUIRE(Coarse.size() == 2);
	for (usize i = 0; i < 2; ++i)
	{
		VT_CHECK_EQ(Coarse[i].Index, Before[i].Index);
		VT_CHECK_EQ(Coarse[i].Members, Before[i].Members); // the last count is kept
		VT_CHECK_EQ(Coarse[i].Disbanded, uint64{0});	   // a coarse seat is never disbanded
	}
	uint32 Memberships = 0;
	W.Instance.Components()
		.GetPool(W.Organizations.Member)
		.ForEach([&](EntityHandle, const Membership&) { ++Memberships; });
	VT_CHECK_EQ(Memberships, 0u); // the persons went with the demotion
	const OrganizationStats S = W.Stats();
	VT_CHECK_EQ(S.Astray, 0u);
	VT_CHECK_EQ(S.CountMismatch, 0u); // coarse seats are not judged against persons
	// Promoted again: the same organisations, filled again from the new persons.
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	std::vector<OrganizationInfo> After;
	OrganizationsOf(W.Instance, W.Organizations, Region, After);
	VT_REQUIRE(After.size() == 2);
	VT_CHECK_EQ(After[0].Index, Before[0].Index);
	VT_CHECK(After[0].Members > 0 && After[1].Members > 0);
	VT_CHECK(After[0].Head != 0 && After[1].Head != 0);
	VT_CHECK_EQ(Count(W.Instance, OrganizationFoundedEvent), 2u); // never founded twice
	VT_CHECK_EQ(W.Stats().CountMismatch, 0u);
}

VAELEN_TEST(Organizations, RulesAndEdges)
{
	// Thresholds nobody reaches: no organisation.
	OrganizationRules Never;
	Never.CouncilFromPeople = 0xffffffffu;
	Never.TempleFromBelievers = 0xffffffffu;
	Run X(AelvorSeed, Never);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, X.Busiest()));
	X.Ages.Run(10);
	VT_CHECK_EQ(X.Stats().Total, 0u);
	// Nobody of age: founded empty, disbanded after the rule's years, once.
	OrganizationRules Old;
	Old.MemberFromAge = 200;
	Old.CouncilFromPeople = 1;
	Old.TempleFromBelievers = 0xffffffffu;
	Old.DisbandAfterYears = 3;
	Run Y(AelvorSeed, Old);
	VT_REQUIRE(Y.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = Y.Busiest();
	VT_CHECK(RequestDetail(Y.Instance, Y.Lod, Region));
	Y.Ages.Run(2);
	OrganizationStats S = Y.Stats();
	VT_CHECK_EQ(S.Total, 1u);
	VT_CHECK_EQ(S.Alive, 1u);
	VT_CHECK_EQ(S.Members, 0u);
	Y.Ages.Run(1); // the third empty year: disbanded
	S = Y.Stats();
	VT_CHECK_EQ(S.Total, 1u);
	VT_CHECK_EQ(S.Alive, 0u);
	VT_CHECK_EQ(Count(Y.Instance, OrganizationDisbandedEvent), 1u);
	Y.Ages.Run(1); // the region still qualifies: a new council, the old one stays in the world
	S = Y.Stats();
	VT_CHECK_EQ(S.Total, 2u);
	VT_CHECK_EQ(S.Alive, 1u);
	VT_CHECK_EQ(Count(Y.Instance, OrganizationFoundedEvent), 2u);
	VT_CHECK_EQ(Count(Y.Instance, OrganizationDisbandedEvent), 1u);
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
	Q.Orgs->Tick(Tc);
	VT_CHECK_EQ(ComputeStateDigest(Q.Instance), Digest);
}

VAELEN_TEST(Organizations, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(30);
	B.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);
	VT_CHECK(A.Stats().Total > 0);
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	VT_CHECK_EQ(R.Stats().Members, A.Stats().Members);
	A.Ages.Run(30);
	R.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	// Frozen: the busiest region of AELVOR 128 for 100 years.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(100);
	const OrganizationStats S = W.Stats();
	VAELEN_LOG_INFO(LogOrganizations,
					"frozen: organizations128=%016llx total=%u members=%u (%u councils, %u temples, %u alive)",
					static_cast<unsigned long long>(S.Digest), S.Total, S.Members, S.PerKind[0], S.PerKind[1], S.Alive);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_ORGANIZATIONS_FROZEN_128});
	VT_CHECK_EQ(S.Total, uint32{VAELEN_ORGANIZATIONS_TOTAL_128});
	VT_CHECK_EQ(S.Members, uint32{VAELEN_ORGANIZATIONS_MEMBERS_128});
	VT_CHECK_EQ(S.Astray, 0u);
	VT_CHECK_EQ(S.CountMismatch, 0u);
}
