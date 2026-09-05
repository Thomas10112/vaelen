// VAELEN - Tests/Sim
// Phase 03.03: languages and deterministic naming.
//
// STATUS: VALIDATED (Phase 03)

#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (03.03).
#define VAELEN_NAMING_FROZEN_128 0x871cdd11bea18906ull
#define VAELEN_NAMING_NAMES_128 154u
#define VAELEN_NAMING_CULTURE1_128 "Oldegedim"
#define VAELEN_NAMING_REGION_128 "Thuthanyo"

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogNaming);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint64 Year = 8640;

	NameText Make(const char* Text)
	{
		NameText T;
		std::strncpy(T.Chars, Text, NameText::Capacity - 1);
		return T;
	}

	struct NameWorld
	{
		explicit NameWorld(uint64 Seed, LanguageRules InRules = LanguageRules{})
			: Instance(Config(Seed)), Rules(InRules)
		{
			Setup = WorldSetup::Declare(Instance);
			Population = PopulationTypes::Declare(Instance);
			Hist = HistoryTypes::Declare(Instance);
			Types = LanguageTypes::Declare(Instance);
			Growth = std::make_unique<PopulationSystem>(Instance, Setup, Population, PopRules);
			Move = std::make_unique<MigrationSystem>(Instance, Setup, Population, PopRules);
			Eras = std::make_unique<EraSystem>(Instance, Hist, EraRules{});
			Languages = std::make_unique<LanguageSystem>(Instance, Setup, Population, Types, Rules);
			Languages->NameEras(Hist.Era);
			Instance.Systems().Add(Growth.get());
			Instance.Systems().Add(Move.get());
			Instance.Systems().Add(Eras.get());
			Instance.Systems().Add(Languages.get());
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool Start(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			if (!GenerateWorld(Instance, Setup, Gen))
			{
				return false;
			}
			InitializeHistory(Instance, Hist);
			return SeedCultures(Instance, Setup, Population, PopRules, Instance.Now()) > 0;
		}
		World Instance;
		WorldSetup Setup;
		PopulationTypes Population;
		HistoryTypes Hist;
		LanguageTypes Types;
		PopulationRules PopRules;
		LanguageRules Rules;
		std::unique_ptr<PopulationSystem> Growth;
		std::unique_ptr<MigrationSystem> Move;
		std::unique_ptr<EraSystem> Eras;
		std::unique_ptr<LanguageSystem> Languages;
	};

	bool SamePhonology(const Phonology& A, const Phonology& B)
	{
		return std::memcmp(&A, &B, sizeof(Phonology)) == 0;
	}

	void LogLines(const char* Title, const std::string& Text, usize MaxLines)
	{
		std::string Slice;
		usize Lines = 0;
		for (usize i = 0; i < Text.size() && Lines < MaxLines; ++i)
		{
			Slice += Text[i];
			if (Text[i] == '\n')
			{
				++Lines;
			}
		}
		VAELEN_LOG_INFO(LogNaming, "%s (%zu of %zu lines):\n%s", Title, Lines,
						static_cast<usize>(std::count(Text.begin(), Text.end(), '\n')), Slice.c_str());
	}
} // namespace

VAELEN_TEST(Naming, PhonologyIsDerivedNormalisedAndDriftsInSmallSteps)
{
	uint32 Changed = 0;
	uint32 Distinct = 0;
	Phonology Previous;
	for (uint32 i = 0; i < 64; ++i)
	{
		const Hash64 Identity = HashUInt64(1000u + i);
		const Phonology P = DerivePhonology(Identity);
		VT_CHECK(IsNormalised(P));
		VT_CHECK(SamePhonology(P, DerivePhonology(Identity)));
		if (i > 0 && !SamePhonology(P, Previous))
		{
			++Distinct;
		}
		Previous = P;
		const Phonology Q = MutatePhonology(P, HashUInt64(i));
		VT_CHECK(IsNormalised(Q));
		VT_CHECK(SamePhonology(Q, MutatePhonology(P, HashUInt64(i))));
		if (!SamePhonology(P, Q))
		{
			++Changed;
		}
		// A drift is small: at most two inventory bits or one weight differ.
		const int Bits =
			std::popcount(P.Onsets ^ Q.Onsets) + std::popcount(P.Codas ^ Q.Codas) + std::popcount(P.Vowels ^ Q.Vowels);
		int Weights = 0;
		for (uint32 K = 0; K < Phonology::ShapeCount; ++K)
		{
			Weights += P.Weight[K] != Q.Weight[K] ? 1 : 0;
		}
		VT_CHECK(Bits + Weights <= 2);
	}
	VT_CHECK(Distinct >= 60);
	VT_CHECK(Changed >= 56);
	// An empty phonology is repaired deterministically.
	Phonology Empty;
	Empty.Weight[0] = 0;
	Empty.MinSyllables = 0;
	Empty.MaxSyllables = 9;
	VT_CHECK(!IsNormalised(Empty));
	const Phonology Fixed = NormalisePhonology(Empty, 7);
	VT_CHECK(IsNormalised(Fixed));
	VT_CHECK(SamePhonology(Fixed, NormalisePhonology(Empty, 7)));
	VT_CHECK_EQ(Fixed.MinSyllables, uint8{1});
	VT_CHECK_EQ(Fixed.MaxSyllables, uint8{4});
	// Sizes are part of the snapshot contract.
	VT_CHECK_EQ(sizeof(Phonology), usize{20});
	VT_CHECK_EQ(sizeof(LanguageInfo), usize{56});
	VT_CHECK_EQ(sizeof(NameInfo), usize{48});
}

VAELEN_TEST(Naming, NamesArePronounceableByConstructionUniqueEnoughAndDeterministic)
{
	uint32 Generated = 0;
	uint32 MinDistinct = 128;
	uint32 SaltChanged = 0;
	uint32 Shortest = 99;
	uint32 Longest = 0;
	for (uint32 L = 0; L < 16; ++L)
	{
		const Phonology P = DerivePhonology(HashUInt64(500u + L));
		for (uint32 S = 0; S < static_cast<uint32>(NameScope::Count); ++S)
		{
			const NameScope Scope = static_cast<NameScope>(S);
			std::vector<std::string> Seen;
			for (uint32 Key = 1; Key <= 128; ++Key)
			{
				const NameText T = GenerateName(P, Scope, Key, 0);
				++Generated;
				VT_CHECK_MSG(IsPronounceable(T), "%s", T.Chars);
				VT_CHECK(NameEquals(T, GenerateName(P, Scope, Key, 0)));
				const uint32 Length = NameLength(T);
				Shortest = Length < Shortest ? Length : Shortest;
				Longest = Length > Longest ? Length : Longest;
				VT_CHECK(Length >= 2 && Length <= 23);
				if (Scope == NameScope::River || Scope == NameScope::Lake)
				{
					VT_CHECK(Length >= 4); // stem plus suffix
				}
				const NameText Salted = GenerateName(P, Scope, Key, 1);
				VT_CHECK_MSG(IsPronounceable(Salted), "%s", Salted.Chars);
				SaltChanged += NameEquals(T, Salted) ? 0u : 1u;
				Seen.push_back(T.Chars);
			}
			std::sort(Seen.begin(), Seen.end());
			const uint32 Distinct = static_cast<uint32>(std::unique(Seen.begin(), Seen.end()) - Seen.begin());
			MinDistinct = Distinct < MinDistinct ? Distinct : MinDistinct;
		}
	}
	VAELEN_LOG_INFO(LogNaming, "%u names: length %u-%u, least distinct per scope %u/128, salt changed %u/%u", Generated,
					Shortest, Longest, MinDistinct, SaltChanged, Generated);
	VT_CHECK(MinDistinct >= 96);
	VT_CHECK(SaltChanged * 10 >= Generated * 9);
	// Two languages name the same key differently.
	const Phonology A = DerivePhonology(HashUInt64(1));
	const Phonology B = DerivePhonology(HashUInt64(2));
	uint32 Same = 0;
	for (uint32 Key = 1; Key <= 64; ++Key)
	{
		Same += NameEquals(GenerateName(A, NameScope::Region, Key, 0), GenerateName(B, NameScope::Region, Key, 0)) ? 1u
																												   : 0u;
	}
	VT_CHECK(Same <= 2);
	// The invariant rejects what the builder never produces.
	VT_CHECK(IsPronounceable(Make("Tha")));
	VT_CHECK(IsPronounceable(Make("Vorendrath")));
	VT_CHECK(!IsPronounceable(Make("")));
	VT_CHECK(!IsPronounceable(Make("A")));
	VT_CHECK(!IsPronounceable(Make("tha")));
	VT_CHECK(!IsPronounceable(Make("ThA")));
	VT_CHECK(!IsPronounceable(Make("Xrtka")));
	VT_CHECK(!IsPronounceable(Make("Aaao")));
	VT_CHECK(!IsPronounceable(Make("Ab1")));
	VT_CHECK(!IsPronounceable(Make("Bnn")));
	VT_CHECK(!IsPronounceable(Make("Ab cd")));
	NameText Hidden = Make("Tha");
	Hidden.Chars[5] = 'x';
	VT_CHECK(!IsPronounceable(Hidden));
	// An unnormalised phonology still yields a pronounceable name.
	Phonology Broken;
	Broken.Weight[0] = 0;
	VT_CHECK(IsPronounceable(GenerateName(Broken, NameScope::Person, 3, 0)));
}

VAELEN_TEST(Naming, LanguagesFollowCulturesAndNameTheWorld)
{
	NameWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	W.Instance.TickMany(Year * 500);
	const PopulationStats People = MeasurePopulation(W.Instance, W.Population);
	const NamingStats Names = MeasureNames(W.Instance, W.Types);
	VAELEN_LOG_INFO(LogNaming,
					"year 500: %u cultures, %u languages, %u names (cultures %u, languages %u, regions %u, rivers %u, "
					"lakes %u, eras %u), max generation %u, max salt %u, duplicates %u",
					People.Cultures, Names.Languages, Names.Names, Names.PerScope[0], Names.PerScope[1],
					Names.PerScope[2], Names.PerScope[3], Names.PerScope[4], Names.PerScope[5], Names.MaxGeneration,
					Names.MaxSalt, Names.Duplicates);
	VT_CHECK_EQ(Names.Languages, People.Cultures);
	VT_CHECK_EQ(Names.Duplicates, 0u);
	VT_CHECK_EQ(Names.PerScope[static_cast<uint32>(NameScope::Culture)], People.Cultures);
	VT_CHECK_EQ(Names.PerScope[static_cast<uint32>(NameScope::Language)], Names.Languages);
	VT_CHECK(Names.PerScope[static_cast<uint32>(NameScope::Region)] >= People.SettledRegions);
	VT_CHECK(Names.PerScope[static_cast<uint32>(NameScope::River)] > 0);
	VT_CHECK_EQ(Names.PerScope[static_cast<uint32>(NameScope::Person)], 0u);
	VT_CHECK_EQ(Names.MaxGeneration, 3u); // 500 years / 150 years per drift, root languages

	// Every language belongs to exactly one culture and inherits along the split.
	std::vector<CultureInfo> Cultures;
	W.Instance.Components()
		.GetPool(W.Population.Culture)
		.ForEach([&](EntityHandle, const CultureInfo& C) { Cultures.push_back(C); });
	std::vector<LanguageInfo> Languages;
	W.Instance.Components()
		.GetPool(W.Types.Language)
		.ForEach([&](EntityHandle, const LanguageInfo& L) { Languages.push_back(L); });
	auto LanguageOfCulture = [&](uint32 Culture) -> const LanguageInfo*
	{
		for (const LanguageInfo& L : Languages)
		{
			if (L.Culture == Culture)
			{
				return &L;
			}
		}
		return nullptr;
	};
	uint32 Children = 0;
	uint32 DriftEvents = 0;
	uint32 FoundedEvents = 0;
	uint32 NamedEvents = 0;
	for (const CultureInfo& C : Cultures)
	{
		const LanguageInfo* L = LanguageOfCulture(C.Index);
		VT_REQUIRE(L != nullptr);
		VT_CHECK(IsNormalised(L->Sounds));
		VT_CHECK(L->Founded >= C.Founded);
		if (C.Parent != 0)
		{
			const LanguageInfo* P = LanguageOfCulture(C.Parent);
			VT_REQUIRE(P != nullptr);
			VT_CHECK_EQ(L->Parent, P->Index);
			++Children;
		}
		else
		{
			VT_CHECK_EQ(L->Parent, 0u);
			VT_CHECK_EQ(L->Generation, 3u);
		}
	}
	VT_CHECK_EQ(Children, People.Cultures - W.PopRules.SeedCultures);
	uint32 Generations = 0;
	for (const LanguageInfo& L : Languages)
	{
		Generations += L.Generation;
	}
	for (const Event& E : W.Instance.Log().All())
	{
		DriftEvents += E.Is(LanguageDriftedEvent) ? 1u : 0u;
		FoundedEvents += E.Is(LanguageFoundedEvent) ? 1u : 0u;
		if (E.Is(NamedEvent))
		{
			++NamedEvents;
			VT_CHECK(E.Subject.IsValid());
			const EntityHandle H = W.Instance.Entities().Find(E.Subject);
			const NameInfo* N = NameOf(W.Instance, W.Types, H);
			VT_CHECK(N != nullptr && N->Language == E.Get<NamePayload>().Language &&
					 N->Scope == E.Get<NamePayload>().Scope && N->Key == E.Get<NamePayload>().Key);
		}
	}
	VT_CHECK_EQ(DriftEvents, Generations);
	VT_CHECK_EQ(FoundedEvents, Names.Languages);
	VT_CHECK_EQ(NamedEvents, Names.Names);

	// Every settled region carries a name in its majority's language family.
	uint32 SettledNamed = 0;
	W.Instance.Components()
		.GetPool(W.Setup.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionPopulation* P = W.Instance.Components().GetPool(W.Population.Population).TryGet(H);
				const NameInfo* N = NameOf(W.Instance, W.Types, H);
				if (P != nullptr && P->Majority != 0)
				{
					VT_CHECK_MSG(N != nullptr, "settled region without a name");
					++SettledNamed;
				}
				if (N != nullptr)
				{
					VT_CHECK_EQ(N->Scope, static_cast<uint32>(NameScope::Region));
					VT_CHECK_EQ(N->Key, uint64{R.Index});
					VT_CHECK(IsPronounceable(N->Text));
				}
			});
	VT_CHECK_EQ(SettledNamed, People.SettledRegions);
	// Eras: every closed era and the open one are named except at most the newest.
	uint32 EraCount = 0;
	W.Instance.Components().GetPool(W.Hist.Era).ForEach([&](EntityHandle, const EraInfo&) { ++EraCount; });
	VT_CHECK(Names.PerScope[static_cast<uint32>(NameScope::Era)] + 1 >= EraCount);
	VT_CHECK(Names.PerScope[static_cast<uint32>(NameScope::Era)] >= 4);

	std::string Text;
	ExportNames(W.Instance, W.Types, NameScope::Culture, Text);
	LogLines("cultures", Text, 20);
	ExportNames(W.Instance, W.Types, NameScope::Region, Text);
	LogLines("regions", Text, 12);
	ExportNames(W.Instance, W.Types, NameScope::River, Text);
	LogLines("rivers", Text, 8);
	ExportNames(W.Instance, W.Types, NameScope::Lake, Text);
	LogLines("lakes", Text, 4);
	ExportNames(W.Instance, W.Types, NameScope::Era, Text);
	LogLines("eras", Text, 6);
}

VAELEN_TEST(Naming, DeterministicSnapshotSafeAndRulesMatter)
{
	NameWorld A(11);
	NameWorld B(11);
	VT_REQUIRE(A.Start(64) && B.Start(64));
	A.Instance.TickMany(Year * 100);
	B.Instance.TickMany(Year * 100);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	std::string NamesA;
	std::string NamesB;
	ExportNames(A.Instance, A.Types, NameScope::Region, NamesA);
	ExportNames(B.Instance, B.Types, NameScope::Region, NamesB);
	VT_CHECK(NamesA == NamesB && !NamesA.empty());
	// Names survive a snapshot byte for byte and the restored world continues identically.
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	NameWorld R(11);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	std::string NamesR;
	ExportNames(R.Instance, R.Types, NameScope::Region, NamesR);
	VT_CHECK(NamesR == NamesA);
	A.Instance.TickMany(Year * 100 + 3);
	R.Instance.TickMany(Year * 100 + 3);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(MeasureNames(R.Instance, R.Types).Duplicates, 0u);
	// A different seed names differently.
	NameWorld C(12);
	VT_REQUIRE(C.Start(64));
	C.Instance.TickMany(Year * 100);
	std::string NamesC;
	ExportNames(C.Instance, C.Types, NameScope::Culture, NamesC);
	std::string CulturesA;
	ExportNames(A.Instance, A.Types, NameScope::Culture, CulturesA);
	VT_CHECK(NamesC != CulturesA);
	// Rules: without drift no language changes; without river names no river is named.
	LanguageRules Still;
	Still.DriftTicks = 0;
	Still.NameRivers = 0;
	Still.NameLakes = 0;
	Still.NameEras = 0;
	NameWorld S(11, Still);
	VT_REQUIRE(S.Start(64));
	S.Instance.TickMany(Year * 200);
	const NamingStats Stats = MeasureNames(S.Instance, S.Types);
	VT_CHECK_EQ(Stats.MaxGeneration, 0u);
	VT_CHECK_EQ(Stats.PerScope[static_cast<uint32>(NameScope::River)], 0u);
	VT_CHECK_EQ(Stats.PerScope[static_cast<uint32>(NameScope::Lake)], 0u);
	VT_CHECK_EQ(Stats.PerScope[static_cast<uint32>(NameScope::Era)], 0u);
	VT_CHECK(Stats.PerScope[static_cast<uint32>(NameScope::Region)] > 0);
	// A drowned world has no culture, no language and no name.
	NameWorld Drowned(12);
	WorldGenConfig Flood;
	Flood.Width = 32;
	Flood.Height = 32;
	Flood.SeaLevel = Fix64::FromInt(100000).Raw;
	VT_REQUIRE(GenerateWorld(Drowned.Instance, Drowned.Setup, Flood));
	InitializeHistory(Drowned.Instance, Drowned.Hist);
	Drowned.Instance.TickMany(Year * 3);
	VT_CHECK_EQ(MeasureNames(Drowned.Instance, Drowned.Types).Names, 0u);
	VT_CHECK_EQ(MeasureNames(Drowned.Instance, Drowned.Types).Languages, 0u);
}

VAELEN_TEST(Naming, FrozenNamesAreReproducedByEveryCompilerAndPlatform)
{
	NameWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	W.Instance.TickMany(Year * 500);
	const Hash64 D = ComputeStateDigest(W.Instance);
	const NamingStats S = MeasureNames(W.Instance, W.Types);
	std::string Culture1;
	std::string Region;
	W.Instance.Components()
		.GetPool(W.Types.Name)
		.ForEach(
			[&](EntityHandle, const NameInfo& N)
			{
				if (N.Scope == static_cast<uint32>(NameScope::Culture) && N.Key == 1)
				{
					Culture1 = N.Text.Chars;
				}
				if (N.Scope == static_cast<uint32>(NameScope::Region) && N.Key == 1)
				{
					Region = N.Text.Chars;
				}
			});
	VAELEN_LOG_INFO(LogNaming, "frozen: naming128=%016llx names=%u culture1=%s region1=%s",
					static_cast<unsigned long long>(D), S.Names, Culture1.c_str(), Region.c_str());
	VT_CHECK_EQ(D, Hash64{VAELEN_NAMING_FROZEN_128});
	VT_CHECK_EQ(S.Names, uint32{VAELEN_NAMING_NAMES_128});
	VT_CHECK_STREQ(Culture1.c_str(), VAELEN_NAMING_CULTURE1_128);
	VT_CHECK_STREQ(Region.c_str(), VAELEN_NAMING_REGION_128);
}
