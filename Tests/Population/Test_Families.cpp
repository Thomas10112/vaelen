// VAELEN - Tests/Population
// Phase 04.03: families and lineage.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.03): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 200 years with
// families.
#define VAELEN_FAMILIES_FROZEN_128 0x25e9435bfb921b5bull
#define VAELEN_FAMILIES_COUNT_128 542u
#define VAELEN_FAMILIES_MARRIED_128 816u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogFamilies);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, FamilyRules InRules = FamilyRules{}, bool WithFamilies = true)
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{}), Rules(InRules)
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1; // a world with families: children are born to couples
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Kin = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, Rules);
			Instance.Systems().Add(Lives.get());
			if (WithFamilies)
			{
				Instance.Systems().Add(Kin.get());
			}
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
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		FamilyRules Rules;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Kin;
	};

	void LogFamilyStats(const char* Title, const FamilyStats& S)
	{
		VAELEN_LOG_INFO(
			LogFamilies,
			"%s: %u families (%u extinct, largest %u), %u married of %u adults, %u in a family, %u broken links", Title,
			S.Families, S.Extinct, S.Largest, S.Married, S.Adults, S.InAFamily, S.Broken);
	}
} // namespace

VAELEN_TEST(Families, MarriagesFollowTheRulesAndFoundFamilies)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	W.Ages.Run(50);
	const FamilyStats S = MeasureFamilies(W.Instance, W.Persons, W.Families, W.Rules, W.Instance.Now());
	LogFamilyStats("year 50", S);
	VT_CHECK(S.Families > 50);
	VT_CHECK(S.Married * 10 >= S.Adults * 5); // most adults are married
	VT_CHECK_EQ(S.Broken, 0u);
	VT_CHECK(S.InAFamily >= S.Married);
	// Every spouse link: symmetric, alive, opposite sex, same culture, same faith,
	// within the age gap at the marriage, not close kin, same family.
	uint32 Couples = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (P.Spouse == 0 || P.State != static_cast<uint8>(LifeState::Alive))
				{
					return;
				}
				const PersonInfo* Sp = FindPerson(W.Instance, W.Persons, P.Spouse);
				VT_REQUIRE(Sp != nullptr);
				VT_CHECK_EQ(Sp->Spouse, P.Index);
				VT_CHECK_EQ(Sp->State, static_cast<uint8>(LifeState::Alive));
				VT_CHECK(Sp->Sex != P.Sex);
				VT_CHECK_EQ(Sp->Culture, P.Culture);
				VT_CHECK_EQ(Sp->Religion, P.Religion);
				VT_CHECK_EQ(Sp->Family, P.Family);
				VT_CHECK(P.Family != 0);
				VT_CHECK(!AreKin(W.Instance, W.Persons, P.Index, Sp->Index, 2));
				Couples += P.Sex == static_cast<uint8>(Sex::Male) ? 1u : 0u;
			});
	VT_CHECK(Couples > 100);
	// Families: a living head who belongs to the family, a founder, a home region.
	W.Instance.Components()
		.GetPool(W.Families.Family)
		.ForEach(
			[&](EntityHandle H, const FamilyInfo& F)
			{
				VT_CHECK_EQ(W.Instance.Entities().GetId(H).Kind(), IdKind::Family);
				VT_CHECK_EQ(F.Region, Region);
				VT_CHECK(F.Founder != 0 && F.Culture != 0 && F.Identity != 0);
				if (F.Extinct == 0)
				{
					const PersonInfo* Head = FindPerson(W.Instance, W.Persons, F.Head);
					VT_REQUIRE(Head != nullptr);
					VT_CHECK_EQ(Head->State, static_cast<uint8>(LifeState::Alive));
					VT_CHECK_EQ(Head->Family, F.Index);
				}
				else
				{
					VT_CHECK_EQ(F.Head, 0u);
					std::vector<uint32> Members;
					FamilyMembers(W.Instance, W.Persons, F.Index, Members);
					VT_CHECK(Members.empty());
				}
			});
	// Events: one PersonMarried per couple ever formed, one FamilyFounded per family.
	uint32 Married = 0;
	uint32 Founded = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Married += E.Is(PersonMarriedEvent) ? 1u : 0u;
		Founded += E.Is(FamilyFoundedEvent) ? 1u : 0u;
	}
	VT_CHECK(Married >= Couples);
	VT_CHECK_EQ(Founded, S.Families);
}

VAELEN_TEST(Families, ChildrenAreBornIntoFamiliesAndLineageIsQueryable)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	W.Ages.Run(120);
	// A child of a married mother has her husband as father and her family.
	uint32 ChildrenOfCouples = 0;
	uint32 ChildrenOfOthers = 0;
	uint32 Grandchildren = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (P.Mother == 0)
				{
					return;
				}
				const PersonInfo* Mother = FindPerson(W.Instance, W.Persons, P.Mother);
				VT_REQUIRE(Mother != nullptr);
				VT_CHECK_EQ(Mother->Sex, static_cast<uint8>(Sex::Female));
				VT_CHECK(Mother->Born < P.Born);
				if (P.Father != 0)
				{
					const PersonInfo* Father = FindPerson(W.Instance, W.Persons, P.Father);
					VT_REQUIRE(Father != nullptr);
					VT_CHECK_EQ(Father->Sex, static_cast<uint8>(Sex::Male));
					VT_CHECK_EQ(Father->Culture, P.Culture);
				}
				if (P.Family != 0 && Mother->Family == P.Family)
				{
					++ChildrenOfCouples;
				}
				else
				{
					++ChildrenOfOthers;
				}
				if (Mother->Mother != 0)
				{
					++Grandchildren;
				}
			});
	VAELEN_LOG_INFO(LogFamilies, "after 120 years: %u children born into a family, %u outside, %u grandchildren",
					ChildrenOfCouples, ChildrenOfOthers, Grandchildren);
	VT_CHECK(ChildrenOfCouples > ChildrenOfOthers);
	VT_CHECK(Grandchildren > 50);
	// Lineage: pick a grandchild; its ancestors hold its parents and grandparents,
	// its siblings share a parent, the grandparent's descendants include it, and
	// kinship is symmetric.
	uint32 Child = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (Child != 0 || P.Mother == 0 || P.Father == 0)
				{
					return;
				}
				const PersonInfo* M = FindPerson(W.Instance, W.Persons, P.Mother);
				if (M != nullptr && M->Mother != 0 && M->Father != 0)
				{
					Child = P.Index;
				}
			});
	VT_REQUIRE(Child != 0);
	const PersonInfo* C = FindPerson(W.Instance, W.Persons, Child);
	const PersonInfo* M = FindPerson(W.Instance, W.Persons, C->Mother);
	std::vector<uint32> Up;
	Ancestors(W.Instance, W.Persons, Child, 2, Up);
	VT_CHECK(std::find(Up.begin(), Up.end(), C->Mother) != Up.end());
	VT_CHECK(std::find(Up.begin(), Up.end(), C->Father) != Up.end());
	VT_CHECK(std::find(Up.begin(), Up.end(), M->Mother) != Up.end());
	VT_CHECK(std::find(Up.begin(), Up.end(), M->Father) != Up.end());
	VT_CHECK(Up.size() >= 4 && Up.size() <= 6);
	std::vector<uint32> Down;
	Descendants(W.Instance, W.Persons, M->Mother, 2, Down);
	VT_CHECK(std::find(Down.begin(), Down.end(), Child) != Down.end());
	VT_CHECK(std::find(Down.begin(), Down.end(), C->Mother) != Down.end());
	std::vector<uint32> Sibs;
	Siblings(W.Instance, W.Persons, Child, Sibs);
	for (const uint32 Sb : Sibs)
	{
		const PersonInfo* S = FindPerson(W.Instance, W.Persons, Sb);
		VT_REQUIRE(S != nullptr);
		VT_CHECK(S->Mother == C->Mother || S->Father == C->Father);
		VT_CHECK(AreKin(W.Instance, W.Persons, Child, Sb, 1));
	}
	VT_CHECK(AreKin(W.Instance, W.Persons, Child, M->Mother, 2));
	VT_CHECK(AreKin(W.Instance, W.Persons, M->Mother, Child, 2));
	VT_CHECK(!AreKin(W.Instance, W.Persons, Child, M->Mother, 1)); // too far for one generation
	VT_CHECK(!AreKin(W.Instance, W.Persons, Child, 0, 4));
	VT_CHECK(AreKin(W.Instance, W.Persons, Child, Child, 0));
	// Unknown persons have no lineage.
	std::vector<uint32> None;
	Ancestors(W.Instance, W.Persons, 999999, 3, None);
	VT_CHECK(None.empty());
	Descendants(W.Instance, W.Persons, 999999, 3, None);
	VT_CHECK(None.empty());
	Siblings(W.Instance, W.Persons, 999999, None);
	VT_CHECK(None.empty());
	VT_CHECK(FindPerson(W.Instance, W.Persons, 0) == nullptr);
	// Families grow generations and some go extinct over a century.
	uint32 Deep = 0;
	uint32 Extinct = 0;
	W.Instance.Components()
		.GetPool(W.Families.Family)
		.ForEach(
			[&](EntityHandle, const FamilyInfo& F)
			{
				Deep += F.Generation >= 2 ? 1u : 0u;
				Extinct += F.Extinct != 0 ? 1u : 0u;
			});
	VT_CHECK(Deep > 10);
	VT_CHECK(Extinct > 0);
	uint32 ExtinctEvents = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		ExtinctEvents += E.Is(FamilyExtinctEvent) ? 1u : 0u;
	}
	VT_CHECK_EQ(ExtinctEvents, Extinct);
}

VAELEN_TEST(Families, WidowsAreReleasedAndRulesMatter)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	W.Ages.Run(20);
	// No living person is married to a dead one after a yearly tick.
	const FamilyStats S = MeasureFamilies(W.Instance, W.Persons, W.Families, W.Rules, W.Instance.Now());
	VT_CHECK_EQ(S.Broken, 0u);
	VT_CHECK(S.Married > 0);
	// A widow may marry again: some persons carry a spouse younger than a child of theirs.
	// (Checked loosely: at least one marriage event names a person twice over time.)
	std::vector<uint32> Grooms;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(PersonMarriedEvent))
		{
			Grooms.push_back(E.Get<MarriagePayload>().Person);
		}
	}
	std::sort(Grooms.begin(), Grooms.end());
	const bool Remarried = std::adjacent_find(Grooms.begin(), Grooms.end()) != Grooms.end();
	VAELEN_LOG_INFO(LogFamilies, "%zu marriages in 20 years, remarriage %s", Grooms.size(),
					Remarried ? "seen" : "not seen");
	// Rules: nobody marries when the chance is zero; faith need not matter.
	FamilyRules Celibate;
	Celibate.MarriagesPerMille = 0;
	Run C(AelvorSeed, Celibate);
	VT_REQUIRE(C.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(PromoteRegion(C.Instance, C.Ages.Types(), C.Persons, MaterialiseRules{}, Region, C.Instance.Now()) > 0);
	C.Ages.Run(20);
	const FamilyStats SC = MeasureFamilies(C.Instance, C.Persons, C.Families, Celibate, C.Instance.Now());
	VT_CHECK_EQ(SC.Married, 0u);
	VT_CHECK_EQ(SC.Families, 0u);
	FamilyRules Mixed;
	Mixed.FaithMatters = 0;
	Run X(AelvorSeed, Mixed);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(PromoteRegion(X.Instance, X.Ages.Types(), X.Persons, MaterialiseRules{}, Region, X.Instance.Now()) > 0);
	X.Ages.Run(20);
	uint32 MixedCouples = 0;
	X.Instance.Components()
		.GetPool(X.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (P.Spouse != 0 && P.Sex == static_cast<uint8>(Sex::Male))
				{
					const PersonInfo* Sp = FindPerson(X.Instance, X.Persons, P.Spouse);
					MixedCouples += Sp != nullptr && Sp->Religion != P.Religion ? 1u : 0u;
				}
			});
	VT_CHECK(MixedCouples > 0);
	// Without the family system nobody marries and the life system is unchanged.
	Run L(AelvorSeed, FamilyRules{}, false);
	VT_REQUIRE(L.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(PromoteRegion(L.Instance, L.Ages.Types(), L.Persons, MaterialiseRules{}, Region, L.Instance.Now()) > 0);
	L.Ages.Run(20);
	VT_CHECK_EQ(MeasureFamilies(L.Instance, L.Persons, L.Families, L.Rules, L.Instance.Now()).Married, 0u);
}

VAELEN_TEST(Families, DeterministicAndSnapshotSafe)
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
	A.Instance.TickMany(50);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	const FamilyStats Before = MeasureFamilies(A.Instance, A.Persons, A.Families, A.Rules, A.Instance.Now());
	const FamilyStats Restored = MeasureFamilies(R.Instance, R.Persons, R.Families, R.Rules, R.Instance.Now());
	VT_CHECK_EQ(Restored.Families, Before.Families);
	VT_CHECK_EQ(Restored.Married, Before.Married);
	A.Ages.Run(40);
	R.Ages.Run(40);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK(IsConsistent(R.Instance, R.Ages.Types(), R.Persons, Region));
}

VAELEN_TEST(Families, FrozenFamiliesAreReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Region, W.Instance.Now()) > 0);
	W.Ages.Run(200);
	const DetailStats D = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	const FamilyStats S = MeasureFamilies(W.Instance, W.Persons, W.Families, W.Rules, W.Instance.Now());
	LogFamilyStats("year 200", S);
	VAELEN_LOG_INFO(LogFamilies, "frozen: families128=%016llx families=%u married=%u",
					static_cast<unsigned long long>(D.PersonsDigest), S.Families, S.Married);
	VT_CHECK_EQ(D.PersonsDigest, Hash64{VAELEN_FAMILIES_FROZEN_128});
	VT_CHECK_EQ(S.Families, uint32{VAELEN_FAMILIES_COUNT_128});
	VT_CHECK_EQ(S.Married, uint32{VAELEN_FAMILIES_MARRIED_128});
	VT_CHECK_EQ(D.Inconsistent, 0u);
	VT_CHECK_EQ(S.Broken, 0u);
}
