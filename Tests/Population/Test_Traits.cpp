// VAELEN - Tests/Population
// Phase 04.05: traits from identity and upbringing, skills through life, person names.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.05): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 200 years with
// lives and traits.
#define VAELEN_TRAITS_FROZEN_128 0x2c2d67a504110d60ull
#define VAELEN_TRAITS_NAMED_128 1499u
#define VAELEN_TRAITS_SKILLSUM_128 358884ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogTraits);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint32 TraitCount = static_cast<uint32>(Trait::Count);
	constexpr uint32 SkillCount = static_cast<uint32>(Skill::Count);

	struct Run
	{
		explicit Run(uint64 Seed, TraitRules InRules = TraitRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Traits = TraitTypes::Declare(Instance);
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, LifeRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, InRules);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Minds.get());
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
		bool Promote(uint32 Region)
		{
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		TraitTypes Traits;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<TraitSystem> Minds;
	};

	struct PersonRow
	{
		PersonInfo Info;
		PersonTraits Traits;
		bool Named = false;
		NameText Name;
	};

	std::vector<PersonRow> Rows(const Run& W)
	{
		std::vector<PersonRow> Out;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					PersonRow R;
					R.Info = P;
					const PersonTraits* T = W.Instance.Components().GetPool(W.Traits.Traits).TryGet(H);
					if (T != nullptr)
					{
						R.Traits = *T;
					}
					const NameInfo* N = W.Instance.Components().GetPool(W.Ages.Types().Languages.Name).TryGet(H);
					if (N != nullptr)
					{
						R.Named = true;
						R.Name = N->Text;
					}
					Out.push_back(R);
				});
		std::sort(Out.begin(), Out.end(),
				  [](const PersonRow& A, const PersonRow& B) { return A.Info.Index < B.Info.Index; });
		return Out;
	}

	const PersonRow* FindRow(const std::vector<PersonRow>& R, uint32 Index)
	{
		for (const PersonRow& P : R)
		{
			if (P.Info.Index == Index)
			{
				return &P;
			}
		}
		return nullptr;
	}
} // namespace

VAELEN_TEST(Traits, TraitsFromIdentityAreDeterministicSpreadAndHeritable)
{
	// Same identity, same traits; different identities, different traits.
	const PersonTraits A = TraitsFromIdentity(0x1234);
	const PersonTraits B = TraitsFromIdentity(0x1234);
	const PersonTraits C = TraitsFromIdentity(0x1235);
	bool Same = true;
	bool Differ = false;
	for (uint32 K = 0; K < TraitCount; ++K)
	{
		Same = Same && A.Traits[K] == B.Traits[K];
		Differ = Differ || A.Traits[K] != C.Traits[K];
		VT_CHECK_EQ(A.Skills[K < SkillCount ? K : 0], uint8{0});
	}
	VT_CHECK(Same && Differ);
	// Over many identities every trait is a bell around 128 with real tails.
	constexpr uint32 Samples = 4096;
	uint64 Sum[TraitCount] = {};
	uint32 Low[TraitCount] = {};
	uint32 High[TraitCount] = {};
	for (uint32 i = 1; i <= Samples; ++i)
	{
		const PersonTraits T = TraitsFromIdentity(HashUInt64(i));
		for (uint32 K = 0; K < TraitCount; ++K)
		{
			Sum[K] += T.Traits[K];
			Low[K] += T.Traits[K] < 64 ? 1u : 0u;
			High[K] += T.Traits[K] >= 192 ? 1u : 0u;
		}
	}
	for (uint32 K = 0; K < TraitCount; ++K)
	{
		const uint64 Mean = Sum[K] / Samples;
		VT_CHECK_MSG(Mean >= 118 && Mean <= 138, "trait %s: mean %llu", TraitName(static_cast<Trait>(K)),
					 static_cast<unsigned long long>(Mean));
		// The three-byte average puts about 5 percent in each tail.
		VT_CHECK(Low[K] > Samples / 100 && Low[K] < Samples / 8);
		VT_CHECK(High[K] > Samples / 100 && High[K] < Samples / 8);
	}
	// A child is pulled toward its parents by the heritability.
	PersonTraits M;
	PersonTraits F;
	for (uint32 K = 0; K < TraitCount; ++K)
	{
		M.Traits[K] = 250;
		F.Traits[K] = 230;
	}
	TraitRules Rules;
	uint64 Child = 0;
	uint64 Own = 0;
	for (uint32 i = 1; i <= 256; ++i)
	{
		const PersonTraits Kid = TraitsFromParents(HashUInt64(i), M, F, Rules);
		const PersonTraits Alone = TraitsFromIdentity(HashUInt64(i));
		for (uint32 K = 0; K < TraitCount; ++K)
		{
			Child += Kid.Traits[K];
			Own += Alone.Traits[K];
		}
	}
	VT_CHECK(Child * 4 > Own * 5); // the parents' 240 pulls the 128 bell up by half the gap
	Rules.HeritabilityPerMille = 0;
	const PersonTraits Free = TraitsFromParents(7, M, F, Rules);
	const PersonTraits Base = TraitsFromIdentity(7);
	Rules.HeritabilityPerMille = 1000;
	const PersonTraits Copy = TraitsFromParents(7, M, F, Rules);
	for (uint32 K = 0; K < TraitCount; ++K)
	{
		VT_CHECK_EQ(Free.Traits[K], Base.Traits[K]);
		VT_CHECK_EQ(Copy.Traits[K], uint8{240});
	}
	for (uint32 S = 0; S < SkillCount; ++S)
	{
		VT_CHECK(static_cast<uint32>(TraitBehind(static_cast<Skill>(S))) < TraitCount);
		VT_CHECK(SkillName(static_cast<Skill>(S))[0] != '?');
	}
	VT_CHECK(TraitName(Trait::Count)[0] == '?');
}

VAELEN_TEST(Traits, EveryPersonGetsTraitsAndANameAndChildrenTakeAfterTheirParents)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	W.Ages.Run(60);
	const std::vector<PersonRow> R = Rows(W);
	VT_REQUIRE(!R.empty());
	uint32 Unnamed = 0;
	uint32 Unpronounceable = 0;
	uint32 WrongLanguage = 0;
	uint32 Children = 0;
	uint64 ToParents = 0;
	uint64 ToStranger = 0;
	for (usize i = 0; i < R.size(); ++i)
	{
		const PersonRow& P = R[i];
		Unnamed += P.Named ? 0u : 1u;
		if (P.Named)
		{
			Unpronounceable += IsPronounceable(P.Name) ? 0u : 1u;
			const NameInfo* N = nullptr;
			W.Instance.Components()
				.GetPool(W.Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& Q)
					{
						if (Q.Index == P.Info.Index)
						{
							N = W.Instance.Components().GetPool(W.Ages.Types().Languages.Name).TryGet(H);
						}
					});
			VT_REQUIRE(N != nullptr);
			WrongLanguage += N->Language == P.Info.Language || P.Info.Language == 0 ? 0u : 1u;
			VT_CHECK_EQ(N->Scope, static_cast<uint32>(NameScope::Person));
		}
		const PersonRow* M = FindRow(R, P.Info.Mother);
		const PersonRow* F = FindRow(R, P.Info.Father);
		if (M == nullptr || F == nullptr)
		{
			continue;
		}
		++Children;
		const PersonRow& Stranger = R[(i * 7919u + 13u) % R.size()];
		for (uint32 K = 0; K < TraitCount; ++K)
		{
			const int32 Parents = (int32{M->Traits.Traits[K]} + int32{F->Traits.Traits[K]}) / 2;
			const int32 Mine = int32{P.Traits.Traits[K]};
			const int32 Other = int32{Stranger.Traits.Traits[K]};
			ToParents += static_cast<uint64>(Mine > Parents ? Mine - Parents : Parents - Mine);
			ToStranger += static_cast<uint64>(Mine > Other ? Mine - Other : Other - Mine);
		}
	}
	VT_CHECK_EQ(Unnamed, 0u);
	VT_CHECK_EQ(Unpronounceable, 0u);
	VT_CHECK_EQ(WrongLanguage, 0u);
	VT_CHECK(Children > 100);
	VT_CHECK(ToParents * 3 < ToStranger * 2); // children sit far closer to their parents than to strangers
	const TraitStats S = MeasureTraits(W.Instance, W.Ages.Types(), W.Persons, W.Traits, Region);
	VT_CHECK(S.WithTraits > 0 && S.Unnamed == 0 && S.Named == S.WithTraits);
	VT_CHECK(S.TraitMin < 64 && S.TraitMax > 192);
	for (uint32 K = 0; K < TraitCount; ++K)
	{
		const uint64 Mean = S.TraitSum[K] / S.WithTraits;
		VT_CHECK_MSG(Mean >= 110 && Mean <= 146, "trait %s: mean %llu", TraitName(static_cast<Trait>(K)),
					 static_cast<unsigned long long>(Mean));
	}
	// A few names, for the record.
	uint32 Shown = 0;
	for (const PersonRow& P : R)
	{
		if (P.Named && P.Info.State == static_cast<uint8>(LifeState::Alive) && Shown < 6)
		{
			++Shown;
			VAELEN_LOG_INFO(LogTraits,
							"person %u: %s, %s of culture %u, vigour %u wit %u will %u charm %u boldness %u piety %u",
							P.Info.Index, P.Name.Chars, P.Info.Sex == static_cast<uint8>(Sex::Female) ? "woman" : "man",
							P.Info.Culture, P.Traits.Traits[0], P.Traits.Traits[1], P.Traits.Traits[2],
							P.Traits.Traits[3], P.Traits.Traits[4], P.Traits.Traits[5]);
		}
	}
	VT_CHECK(NameLength(PersonName(W.Instance, W.Ages.Types().Languages, W.Persons, R.front().Info.Index)) > 1);
	VT_CHECK_EQ(NameLength(PersonName(W.Instance, W.Ages.Types().Languages, W.Persons, 0)), 0u);
	VT_CHECK_EQ(NameLength(PersonName(W.Instance, W.Ages.Types().Languages, W.Persons, 0xfffffff0u)), 0u);
}

VAELEN_TEST(Traits, SkillsGrowThroughLifeAndFadeInOldAge)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	W.Ages.Run(80);
	const std::vector<PersonRow> R = Rows(W);
	const TraitRules Rules;
	// Mean skill by age band among the living.
	struct Band
	{
		uint32 From;
		uint32 To;
		uint64 Sum = 0;
		uint32 Count = 0;
	};
	Band Bands[5] = {{0, Rules.SkillFrom},
					 {Rules.SkillFrom, Rules.ApprenticeTo},
					 {Rules.ApprenticeTo, 30},
					 {30, Rules.SkillTo},
					 {Rules.DeclineFrom + 15, 200}};
	uint32 Talented = 0;
	uint32 OverCap = 0;
	for (const PersonRow& P : R)
	{
		if (P.Info.State != static_cast<uint8>(LifeState::Alive))
		{
			continue;
		}
		const uint32 Age = AgeYears(P.Info, W.Instance.Now());
		for (Band& B : Bands)
		{
			if (Age >= B.From && Age < B.To)
			{
				for (uint32 S = 0; S < SkillCount; ++S)
				{
					B.Sum += P.Traits.Skills[S];
				}
				++B.Count;
			}
		}
		for (uint32 S = 0; S < SkillCount; ++S)
		{
			const uint32 Cap =
				128u + uint32{P.Traits.Traits[static_cast<uint32>(TraitBehind(static_cast<Skill>(S)))]} / 2u;
			OverCap += P.Traits.Skills[S] > Cap ? 1u : 0u;
			Talented += P.Traits.Skills[S] >= 128 ? 1u : 0u;
		}
	}
	for (const Band& B : Bands)
	{
		VT_REQUIRE(B.Count > 0);
		VAELEN_LOG_INFO(LogTraits, "ages %u-%u: %u persons, mean skill %llu", B.From, B.To, B.Count,
						static_cast<unsigned long long>(B.Sum / (uint64{B.Count} * SkillCount)));
	}
	VT_CHECK_EQ(Bands[0].Sum, uint64{0}); // no skill before the learning age
	VT_CHECK(Bands[1].Sum / Bands[1].Count < Bands[2].Sum / Bands[2].Count);
	VT_CHECK(Bands[2].Sum / Bands[2].Count < Bands[3].Sum / Bands[3].Count);
	VT_CHECK(Bands[4].Sum / Bands[4].Count < Bands[3].Sum / Bands[3].Count); // the old fade
	VT_CHECK_EQ(OverCap, 0u);
	VT_CHECK(Talented > 0);
	// The children of skilled parents learn faster: among 8 to 15 year olds,
	// those whose parents know a trade know more of it.
	uint64 Taught = 0;
	uint32 TaughtCount = 0;
	uint64 Untaught = 0;
	uint32 UntaughtCount = 0;
	for (const PersonRow& P : R)
	{
		const uint32 Age = AgeYears(P.Info, W.Instance.Now());
		if (P.Info.State != static_cast<uint8>(LifeState::Alive) || Age < 12 || Age >= Rules.ApprenticeTo)
		{
			continue;
		}
		const PersonRow* M = FindRow(R, P.Info.Mother);
		const PersonRow* F = FindRow(R, P.Info.Father);
		if (M == nullptr || F == nullptr)
		{
			continue;
		}
		const uint32 Best = std::max(M->Traits.Skills[0], F->Traits.Skills[0]);
		if (Best >= 128)
		{
			Taught += P.Traits.Skills[0];
			++TaughtCount;
		}
		else if (Best < 64)
		{
			Untaught += P.Traits.Skills[0];
			++UntaughtCount;
		}
	}
	VAELEN_LOG_INFO(LogTraits, "apprentices: %u taught (mean farming %llu), %u untaught (mean %llu)", TaughtCount,
					static_cast<unsigned long long>(TaughtCount > 0 ? Taught / TaughtCount : 0u), UntaughtCount,
					static_cast<unsigned long long>(UntaughtCount > 0 ? Untaught / UntaughtCount : 0u));
	VT_CHECK(TaughtCount > 0 && UntaughtCount > 0);
	VT_CHECK(Taught * UntaughtCount > Untaught * TaughtCount);
	// Rules: no names when refused; no growth without the system.
	TraitRules Nameless;
	Nameless.NamePersons = 0;
	Run X(AelvorSeed, Nameless);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(X.Promote(X.Busiest()));
	X.Ages.Run(5);
	const TraitStats SX = MeasureTraits(X.Instance, X.Ages.Types(), X.Persons, X.Traits, 0);
	VT_CHECK(SX.WithTraits > 0);
	VT_CHECK_EQ(SX.Named, 0u);
	VT_CHECK_EQ(SX.Unnamed, SX.WithTraits);
}

VAELEN_TEST(Traits, DeterministicAndSnapshotSafe)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_REQUIRE(A.Promote(Region));
	VT_REQUIRE(B.Promote(Region));
	A.Ages.Run(30);
	B.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(MeasureTraits(A.Instance, A.Ages.Types(), A.Persons, A.Traits, 0).Digest,
				MeasureTraits(B.Instance, B.Ages.Types(), B.Persons, B.Traits, 0).Digest);
	const std::vector<PersonRow> RA = Rows(A);
	const std::vector<PersonRow> RB = Rows(B);
	VT_REQUIRE(RA.size() == RB.size());
	uint32 NameDiffs = 0;
	for (usize i = 0; i < RA.size(); ++i)
	{
		NameDiffs += NameEquals(RA[i].Name, RB[i].Name) ? 0u : 1u;
	}
	VT_CHECK_EQ(NameDiffs, 0u);
	// Another seed: other names.
	Run C(AelvorSeed + 1);
	VT_REQUIRE(C.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(C.Promote(C.Busiest()));
	C.Ages.Run(1);
	const std::vector<PersonRow> RC = Rows(C);
	uint32 SameName = 0;
	const usize Common = std::min(RA.size(), RC.size());
	for (usize i = 0; i < Common; ++i)
	{
		SameName += NameEquals(RA[i].Name, RC[i].Name) ? 1u : 0u;
	}
	VT_CHECK(SameName * 10 < Common);
	// A snapshot between yearly ticks continues identically, traits and names included.
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	A.Ages.Run(30);
	R.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(MeasureTraits(R.Instance, R.Ages.Types(), R.Persons, R.Traits, 0).Digest,
				MeasureTraits(A.Instance, A.Ages.Types(), A.Persons, A.Traits, 0).Digest);
	VT_CHECK(IsConsistent(R.Instance, R.Ages.Types(), R.Persons, Region));
}

VAELEN_TEST(Traits, FrozenTraitsAreReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	W.Ages.Run(200);
	const TraitStats S = MeasureTraits(W.Instance, W.Ages.Types(), W.Persons, W.Traits, Region);
	uint64 SkillSum = 0;
	for (uint32 K = 0; K < SkillCount; ++K)
	{
		SkillSum += S.SkillSum[K];
	}
	VAELEN_LOG_INFO(LogTraits, "frozen: traits128=%016llx named=%u skills=%llu",
					static_cast<unsigned long long>(S.Digest), S.Named, static_cast<unsigned long long>(SkillSum));
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_TRAITS_FROZEN_128});
	VT_CHECK_EQ(S.Named, uint32{VAELEN_TRAITS_NAMED_128});
	VT_CHECK_EQ(SkillSum, uint64{VAELEN_TRAITS_SKILLSUM_128});
	VT_CHECK_EQ(S.Unnamed, 0u);
	// The persons digest of the lives-only world is untouched by traits and names
	// (they live in their own components).
	const DetailStats D = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	VT_CHECK_EQ(D.Inconsistent, 0u);
}
