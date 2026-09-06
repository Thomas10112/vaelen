// VAELEN - Tests/Sim
// Phase 03.04: religions.
//
// STATUS: VALIDATED (Phase 03)

#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (03.04).
#define VAELEN_RELIGION_FROZEN_128 0x169e51de300cea9full
#define VAELEN_RELIGION_COUNT_128 11u
#define VAELEN_RELIGION_ADHERENTS_128 45682ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogReligion);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint64 Year = 8640;

	struct FaithWorld
	{
		explicit FaithWorld(uint64 Seed, bool WithMigration = true, bool WithEras = true,
							ReligionRules InRules = ReligionRules{})
			: Instance(Config(Seed)), Rules(InRules)
		{
			Setup = WorldSetup::Declare(Instance);
			Population = PopulationTypes::Declare(Instance);
			Hist = HistoryTypes::Declare(Instance);
			Languages = LanguageTypes::Declare(Instance);
			Types = ReligionTypes::Declare(Instance);
			Growth = std::make_unique<PopulationSystem>(Instance, Setup, Population, PopRules);
			Move = std::make_unique<MigrationSystem>(Instance, Setup, Population, PopRules);
			Eras = std::make_unique<EraSystem>(Instance, Hist, EraRules{});
			Naming = std::make_unique<LanguageSystem>(Instance, Setup, Population, Languages, LanguageRules{});
			Faith = std::make_unique<ReligionSystem>(Instance, Setup, Population, Types, Rules);
			Faith->NameWith(Languages);
			Listener = std::make_unique<FaithListener>(Instance, Setup, Population, Types, Rules, Faith.get());
			Instance.Systems().Add(Growth.get());
			if (WithMigration)
			{
				Instance.Systems().Add(Move.get());
			}
			if (WithEras)
			{
				Instance.Systems().Add(Eras.get());
			}
			Instance.Systems().Add(Naming.get());
			Instance.Systems().Add(Faith.get());
			Listener->Listen(Instance.Events());
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
			InitializeFaith(Instance, Types);
			return SeedCultures(Instance, Setup, Population, PopRules, Instance.Now()) > 0;
		}
		World Instance;
		WorldSetup Setup;
		PopulationTypes Population;
		HistoryTypes Hist;
		LanguageTypes Languages;
		ReligionTypes Types;
		PopulationRules PopRules;
		ReligionRules Rules;
		std::unique_ptr<PopulationSystem> Growth;
		std::unique_ptr<MigrationSystem> Move;
		std::unique_ptr<EraSystem> Eras;
		std::unique_ptr<LanguageSystem> Naming;
		std::unique_ptr<ReligionSystem> Faith;
		std::unique_ptr<FaithListener> Listener;
	};

	std::vector<EntityHandle> Handles(FaithWorld& W)
	{
		std::vector<EntityHandle> Out;
		W.Instance.Components()
			.GetPool(W.Setup.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const RegionInfo& R)
				{
					if (Out.size() <= R.Index)
					{
						Out.resize(R.Index + 1u);
					}
					Out[R.Index] = H;
				});
		return Out;
	}

	// Believers never exceed the population; every slot is consistent.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, FaithWorld& W)
	{
		uint32 Failures = 0;
		W.Instance.Components()
			.GetPool(W.Setup.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const RegionInfo& R)
				{
					const RegionFaith* F = W.Instance.Components().GetPool(W.Types.Faith).TryGet(H);
					if (F == nullptr)
					{
						return;
					}
					const RegionPopulation* P = W.Instance.Components().GetPool(W.Population.Population).TryGet(H);
					const uint32 People = P != nullptr ? P->Total : 0u;
					RegionFaith Copy = *F;
					Copy.Recount();
					if (Copy.Majority != F->Majority)
					{
						++Failures;
						VT_CHECK_MSG(false, "region %u: stale majority %u (recount %u)", R.Index, F->Majority,
									 Copy.Majority);
					}
					if (F->Total() > People)
					{
						++Failures;
						VT_CHECK_MSG(false, "region %u: %u believers for %u people", R.Index, F->Total(), People);
					}
					for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
					{
						if ((F->Religion[K] == 0) != (F->Adherents[K] == 0))
						{
							++Failures;
							VT_CHECK_MSG(false, "region %u: slot %u religion %u with %u believers", R.Index, K,
										 F->Religion[K], F->Adherents[K]);
						}
					}
				});
		VT_CHECK_EQ(Failures, 0u);
		return Failures;
	}

} // namespace

VAELEN_TEST(Religion, BookkeepingIsExactAndTenetsAreDerived)
{
	RegionFaith F;
	VT_CHECK_EQ(F.Total(), 0u);
	VT_CHECK(F.Add(3, 100));
	VT_CHECK(F.Add(5, 100));
	VT_CHECK_EQ(F.Majority, 3u); // ties go to the lowest index
	VT_CHECK(F.Add(5, 1));
	VT_CHECK_EQ(F.Majority, 5u);
	VT_CHECK_EQ(F.Total(), 201u);
	VT_CHECK_EQ(F.Remove(5, 500), 101u);
	VT_CHECK_EQ(F.SlotOf(5), RegionFaith::MaxFaiths);
	VT_CHECK_EQ(F.Majority, 3u);
	VT_CHECK(!F.Add(0, 10));
	VT_CHECK(F.Add(7, 1) && F.Add(8, 1) && F.Add(9, 1));
	VT_CHECK(!F.Add(10, 1)); // four slots
	VT_CHECK_EQ(F.Remove(42, 1), 0u);
	F.Adherents[0] = 0xffffffffu;
	VT_CHECK(F.Add(3, 5));
	VT_CHECK_EQ(F.Adherents[0], 0xffffffffu); // saturates
	// Tenets: deterministic, and a schism moves one or two axes visibly.
	uint32 Moved1 = 0;
	uint32 Moved2 = 0;
	for (uint32 i = 0; i < 64; ++i)
	{
		const Tenets T = DeriveTenets(HashUInt64(i));
		VT_CHECK(std::equal(T.Value, T.Value + Tenets::Axes, DeriveTenets(HashUInt64(i)).Value));
		const Tenets S = SchismTenets(T, HashUInt64(1000u + i));
		uint32 Moved = 0;
		for (uint32 A = 0; A < Tenets::Axes; ++A)
		{
			Moved += T.Value[A] != S.Value[A] ? 1u : 0u;
		}
		VT_CHECK(Moved == 1 || Moved == 2);
		Moved1 += Moved == 1 ? 1u : 0u;
		Moved2 += Moved == 2 ? 1u : 0u;
	}
	VT_CHECK(Moved1 > 8 && Moved2 > 8);
	VT_CHECK_EQ(sizeof(ReligionInfo), usize{56});
	VT_CHECK_EQ(sizeof(RegionFaith), usize{40});
	VT_CHECK_EQ(sizeof(FaithState), usize{144});
}

VAELEN_TEST(Religion, NoReligionWithoutAFoundingEventAndFaithFollowsCulturesAndNames)
{
	FaithWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	for (uint32 Century = 1; Century <= 5; ++Century)
	{
		// One tick past the century: believers travelling with the last waves
		// reach their destination when the bus dispatches at the next tick.
		W.Instance.TickMany(Century == 1 ? Year * 100 + 1 : Year * 100);
		VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
		const FaithStats S = MeasureFaith(W.Instance, W.Population, W.Types);
		VAELEN_LOG_INFO(
			LogReligion,
			"year %u: %u religions (%u schisms), %llu believers of %llu people, %u/%u regions with a majority "
			"faith, largest %u with %llu, refused %u",
			Century * 100, S.Religions, S.Schisms, static_cast<unsigned long long>(S.Adherents),
			static_cast<unsigned long long>(S.People), S.ConvertedRegions, S.Regions, S.LargestReligion,
			static_cast<unsigned long long>(S.LargestAdherents), S.Refused);
	}
	const FaithStats S = MeasureFaith(W.Instance, W.Population, W.Types);
	VT_CHECK(S.Religions >= 3);
	VT_CHECK(S.Schisms >= 1);
	VT_CHECK(S.Adherents <= S.People);
	VT_CHECK(S.Adherents * 2 > S.People);		  // most people believe after five centuries
	VT_CHECK(S.ConvertedRegions * 2 > S.Regions); // and most regions have a majority faith
	VT_CHECK_EQ(S.Pending, 0u);

	// Every religion has a founding event that precedes it and a founding event of the
	// right kind; every founding is an event caused by that founding event.
	const EventLog& Log = W.Instance.Log();
	uint32 Founded = 0;
	uint32 Schisms = 0;
	uint32 Conversions = 0;
	std::vector<ReligionInfo> Religions;
	W.Instance.Components()
		.GetPool(W.Types.Religion)
		.ForEach([&](EntityHandle, const ReligionInfo& Rg) { Religions.push_back(Rg); });
	for (const ReligionInfo& Rg : Religions)
	{
		VT_CHECK(Rg.FoundingEvent != 0);
		const Event* Cause = FindEvent(Log, PersistentId{Rg.FoundingEvent});
		VT_REQUIRE(Cause != nullptr);
		VT_CHECK(Cause->Tick <= Rg.Founded);
		if (Rg.Kind == static_cast<uint32>(FoundingKind::Era))
		{
			VT_CHECK(Cause->Is(EraOpenedEvent));
			VT_CHECK_EQ(Rg.Parent, 0u);
		}
		else if (Rg.Kind == static_cast<uint32>(FoundingKind::Schism))
		{
			VT_CHECK(Cause->Is(CultureSplitEvent));
			VT_CHECK(Rg.Parent != 0 && Rg.Parent < Rg.Index);
			VT_CHECK_EQ(Rg.HomeRegion, Cause->Get<CulturePayload>().Region);
		}
		VT_CHECK(Rg.HomeRegion != 0 && Rg.Culture != 0);
	}
	for (const Event& E : Log.All())
	{
		if (E.Is(ReligionFoundedEvent) || E.Is(SchismEvent))
		{
			const ReligionPayload P = E.Get<ReligionPayload>();
			VT_CHECK(E.Cause.IsValid() && E.Subject.IsValid());
			const ReligionInfo* Rg =
				W.Instance.Components().GetPool(W.Types.Religion).TryGet(W.Instance.Entities().Find(E.Subject));
			VT_REQUIRE(Rg != nullptr);
			VT_CHECK_EQ(Rg->Index, P.Religion);
			VT_CHECK_EQ(Rg->FoundingEvent, E.Cause.Value);
			VT_CHECK_EQ(Rg->Founded, E.Tick);
			VT_CHECK_EQ(E.Is(SchismEvent), Rg->Parent != 0);
			Founded += E.Is(ReligionFoundedEvent) ? 1u : 0u;
			Schisms += E.Is(SchismEvent) ? 1u : 0u;
		}
		if (E.Is(RegionConvertedEvent))
		{
			const ConversionPayload P = E.Get<ConversionPayload>();
			VT_CHECK(P.Region != 0 && P.Religion != P.Previous);
			++Conversions;
		}
	}
	VT_CHECK_EQ(Founded + Schisms, S.Religions);
	VT_CHECK_EQ(Schisms, S.Schisms);
	VT_CHECK(Conversions > S.Religions); // faith spread beyond its founding regions

	// Names in the founding culture's language, unique per scope.
	const NamingStats Names = MeasureNames(W.Instance, W.Languages);
	VT_CHECK_EQ(Names.PerScope[static_cast<uint32>(NameScope::Religion)], S.Religions);
	VT_CHECK_EQ(Names.Duplicates, 0u);
	W.Instance.Components()
		.GetPool(W.Types.Religion)
		.ForEach(
			[&](EntityHandle H, const ReligionInfo& Rg)
			{
				const NameInfo* N = NameOf(W.Instance, W.Languages, H);
				VT_REQUIRE(N != nullptr);
				VT_CHECK_EQ(N->Key, uint64{Rg.Index});
				const LanguageInfo* L = nullptr;
				W.Instance.Components()
					.GetPool(W.Languages.Language)
					.ForEach(
						[&](EntityHandle, const LanguageInfo& Lg)
						{
							if (Lg.Index == N->Language)
							{
								L = &Lg;
							}
						});
				VT_REQUIRE(L != nullptr);
				VT_CHECK_EQ(L->Culture, Rg.Culture);
			});
	std::string Text;
	ExportNames(W.Instance, W.Languages, NameScope::Religion, Text);
	VAELEN_LOG_INFO(LogReligion, "religions:\n%s", Text.c_str());
	std::string Picture;
	ExportFaithAscii(W.Instance, W.Setup, W.Population, W.Types, 64, Picture);
	std::string Cultures;
	ExportCultureAscii(W.Instance, W.Setup, W.Population, 64, Cultures);
	VT_CHECK_EQ(Picture.size(), Cultures.size());
	for (usize Row = 0; Row < 32; Row += 8)
	{
		const std::string Slice = Picture.substr(Row * 65, 8 * 65);
		VAELEN_LOG_INFO(LogReligion, "AELVOR faiths after 500 years at 128, rows %zu-%zu:\n%s", Row, Row + 7,
						Slice.c_str());
	}
}

VAELEN_TEST(Religion, SpreadFollowsTheRegionGraphAndRequestsAreBounded)
{
	// No eras: the only faith is the one requested by hand.
	FaithWorld W(AelvorSeed, true, false);
	VT_REQUIRE(W.Start(128));
	W.Instance.TickMany(Year * 150); // let the cultures spread first
	const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Setup.Regions);
	const std::vector<EntityHandle> Regions = Handles(W);
	VT_CHECK_EQ(MeasureFaith(W.Instance, W.Population, W.Types).Religions, 0u);
	// Home: the most populated region.
	uint32 Home = 0;
	uint32 HomePeople = 0;
	for (uint32 R = 1; R < Regions.size(); ++R)
	{
		const RegionPopulation* P = W.Instance.Components().GetPool(W.Population.Population).TryGet(Regions[R]);
		if (P != nullptr && P->Total > HomePeople)
		{
			HomePeople = P->Total;
			Home = R;
		}
	}
	VT_REQUIRE(Home != 0);
	const PersistentId Cause = W.Instance.Log().All().back().Id;
	VT_CHECK(W.Faith->RequestFounding(Home, Cause));
	VT_CHECK(!W.Faith->RequestFounding(Home, Cause));			   // duplicate region
	VT_CHECK(!W.Faith->RequestFounding(0, Cause));				   // no region
	VT_CHECK(!W.Faith->RequestFounding(Home + 1, PersistentId{})); // no cause
	// The queue holds eight requests.
	uint32 Accepted = 1;
	for (uint32 R = 1; R < Regions.size() && R < 40; ++R)
	{
		if (R != Home)
		{
			Accepted += W.Faith->RequestFounding(R, Cause) ? 1u : 0u;
		}
	}
	VT_CHECK_EQ(Accepted, FaithState::MaxPending);
	VT_CHECK_EQ(MeasureFaith(W.Instance, W.Population, W.Types).Pending, FaithState::MaxPending);
	W.Instance.TickMany(Year);
	const FaithStats AfterFounding = MeasureFaith(W.Instance, W.Population, W.Types);
	VT_CHECK_EQ(AfterFounding.Pending, 0u);
	VT_CHECK(AfterFounding.Religions >= 1 && AfterFounding.Religions <= FaithState::MaxPending);
	VT_CHECK(AfterFounding.Refused > 0); // duplicates, the null cause, the overflow, unsettled regions
	// The faith founded at Home is religion 1 (first request).
	const ReligionInfo* First = nullptr;
	W.Instance.Components()
		.GetPool(W.Types.Religion)
		.ForEach(
			[&](EntityHandle, const ReligionInfo& Rg)
			{
				if (Rg.Index == 1)
				{
					First = &Rg;
				}
			});
	VT_REQUIRE(First != nullptr);
	VT_CHECK_EQ(First->HomeRegion, Home);
	VT_CHECK_EQ(First->FoundingEvent, Cause.Value);
	VT_CHECK_EQ(First->Kind, static_cast<uint32>(FoundingKind::Requested));
	// The yearly step alone reaches only the neighbours of a region where the
	// faith is the majority: run it by hand and compare with the state before.
	uint32 Reached = 0;
	for (uint32 Round = 0; Round < 40; ++Round)
	{
		W.Instance.TickMany(1); // deliver the waves of the last tick first
		std::vector<uint8> HadFaith(Regions.size(), 0);
		std::vector<uint8> WasMajority(Regions.size(), 0);
		for (uint32 R = 1; R < Regions.size(); ++R)
		{
			const RegionFaith* F = W.Instance.Components().GetPool(W.Types.Faith).TryGet(Regions[R]);
			if (F != nullptr && F->SlotOf(1) < RegionFaith::MaxFaiths)
			{
				HadFaith[R] = 1;
				WasMajority[R] = F->Majority == 1 ? 1 : 0;
			}
		}
		RandomStream Stream(1);
		TickContext C;
		C.Tick = W.Instance.Now();
		C.Entities = &W.Instance.Entities();
		C.Components = &W.Instance.Components();
		C.Random = &Stream;
		C.Events = &W.Instance.Events();
		W.Faith->Tick(C);
		VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
		Reached = 0;
		for (uint32 R = 1; R < Regions.size(); ++R)
		{
			const RegionFaith* F = W.Instance.Components().GetPool(W.Types.Faith).TryGet(Regions[R]);
			if (F == nullptr || F->SlotOf(1) == RegionFaith::MaxFaiths)
			{
				continue;
			}
			++Reached;
			const RegionPopulation* P = W.Instance.Components().GetPool(W.Population.Population).TryGet(Regions[R]);
			VT_CHECK(P != nullptr && P->Total > 0); // believers only where people live
			if (HadFaith[R] != 0)
			{
				continue;
			}
			bool Adjacent = false;
			for (uint16 N : Graph.Neighbours[R])
			{
				Adjacent = Adjacent || WasMajority[N] != 0;
			}
			VT_CHECK_MSG(Adjacent, "region %u gained faith 1 without a converted neighbour (round %u)", R, Round);
		}
		W.Instance.TickMany(Year); // the world moves on; waves only cross one edge
	}
	VT_CHECK(Reached >= 3);
	VAELEN_LOG_INFO(LogReligion, "faith 1 from region %u reached %u regions in 40 years", Home, Reached);
	// A request in a region nobody lives in is refused at the yearly tick.
	uint32 Empty = 0;
	for (uint32 R = 1; R < Regions.size(); ++R)
	{
		const RegionPopulation* P = W.Instance.Components().GetPool(W.Population.Population).TryGet(Regions[R]);
		if (P != nullptr && P->Total == 0)
		{
			Empty = R;
			break;
		}
	}
	if (Empty != 0)
	{
		const uint32 RefusedBefore = MeasureFaith(W.Instance, W.Population, W.Types).Refused;
		const uint32 ReligionsBefore = MeasureFaith(W.Instance, W.Population, W.Types).Religions;
		VT_CHECK(W.Faith->RequestFounding(Empty, Cause));
		W.Instance.TickMany(Year);
		VT_CHECK_EQ(MeasureFaith(W.Instance, W.Population, W.Types).Refused, RefusedBefore + 1);
		VT_CHECK_EQ(MeasureFaith(W.Instance, W.Population, W.Types).Religions, ReligionsBefore);
	}
	// Without a faith state nothing happens and nothing is requested.
	FaithWorld Bare(5, false, false);
	WorldGenConfig Gen;
	Gen.Width = 32;
	Gen.Height = 32;
	VT_REQUIRE(GenerateWorld(Bare.Instance, Bare.Setup, Gen));
	VT_CHECK(!Bare.Faith->RequestFounding(1, Cause));
	Bare.Instance.TickMany(Year * 2);
	VT_CHECK_EQ(MeasureFaith(Bare.Instance, Bare.Population, Bare.Types).Religions, 0u);
	{
		VaelenTest::ScopedAssertCapture Capture;
		VT_CHECK(InitializeFaith(Bare.Instance, Bare.Types).IsNull() == false);
		VT_CHECK(InitializeFaith(Bare.Instance, Bare.Types).IsNull());
#if VAELEN_ASSERTS_ENABLED
		VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
	}
}

VAELEN_TEST(Religion, DeterministicSnapshotSafeAndRulesMatter)
{
	FaithWorld A(11);
	FaithWorld B(11);
	VT_REQUIRE(A.Start(64) && B.Start(64));
	A.Instance.TickMany(Year * 120);
	B.Instance.TickMany(Year * 120);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK(MeasureFaith(A.Instance, A.Population, A.Types).Religions >= 1);
	// A pending request survives a snapshot taken before the yearly tick.
	const PersistentId Cause = A.Instance.Log().All().back().Id;
	uint32 Target = 0;
	A.Instance.Components()
		.GetPool(A.Setup.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionPopulation* P = A.Instance.Components().GetPool(A.Population.Population).TryGet(H);
				if (Target == 0 && P != nullptr && P->Total > 0)
				{
					Target = R.Index;
				}
			});
	VT_REQUIRE(Target != 0);
	A.Instance.TickMany(3);
	VT_CHECK(A.Faith->RequestFounding(Target, Cause));
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	FaithWorld R(11);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(MeasureFaith(R.Instance, R.Population, R.Types).Pending, 1u);
	A.Instance.TickMany(Year * 40);
	R.Instance.TickMany(Year * 40);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(MeasureFaith(R.Instance, R.Population, R.Types).Pending, 0u);
	VT_CHECK_EQ(CheckInvariants(Ctx, R), 0u);
	// Rules: no era foundings and no schisms leave the world faithless.
	ReligionRules Silent;
	Silent.FoundOnEra = 0;
	Silent.SchismOnSplit = 0;
	FaithWorld S(11, true, true, Silent);
	VT_REQUIRE(S.Start(64));
	S.Instance.TickMany(Year * 120);
	VT_CHECK_EQ(MeasureFaith(S.Instance, S.Population, S.Types).Religions, 0u);
	VT_CHECK_EQ(MeasureFaith(S.Instance, S.Population, S.Types).Adherents, 0u);
	// A faster spread converts more people in the same time.
	ReligionRules Zeal;
	Zeal.ConvertPerMille = 300;
	Zeal.SpreadPerMille = 100;
	FaithWorld Z(11, true, true, Zeal);
	VT_REQUIRE(Z.Start(64));
	Z.Instance.TickMany(Year * 120);
	VT_CHECK(MeasureFaith(Z.Instance, Z.Population, Z.Types).Adherents >
			 MeasureFaith(B.Instance, B.Population, B.Types).Adherents);
}

VAELEN_TEST(Religion, FrozenFaithIsReproducedByEveryCompilerAndPlatform)
{
	FaithWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	W.Instance.TickMany(Year * 500);
	const Hash64 D = ComputeStateDigest(W.Instance);
	const FaithStats S = MeasureFaith(W.Instance, W.Population, W.Types);
	VAELEN_LOG_INFO(LogReligion, "frozen: religion128=%016llx religions=%u adherents=%llu",
					static_cast<unsigned long long>(D), S.Religions, static_cast<unsigned long long>(S.Adherents));
	VT_CHECK_EQ(D, Hash64{VAELEN_RELIGION_FROZEN_128});
	VT_CHECK_EQ(S.Religions, uint32{VAELEN_RELIGION_COUNT_128});
	VT_CHECK_EQ(S.Adherents, uint64{VAELEN_RELIGION_ADHERENTS_128});
}
