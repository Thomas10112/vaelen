// VAELEN - Tests/Sim
// Phase 03.05: disasters and omens.
//
// STATUS: VALIDATED (Phase 03)

#include "Vaelen/Sim/Disasters.h"
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
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (03.05).
#define VAELEN_DISASTER_FROZEN_128 0x2ac331b0540c3224ull
#define VAELEN_DISASTER_COUNT_128 250u
#define VAELEN_DISASTER_DEATHS_128 4691ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogDisasters);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint64 Year = 8640;
	constexpr uint32 Kinds = static_cast<uint32>(DisasterKind::Count);

	struct DoomWorld
	{
		explicit DoomWorld(uint64 Seed, DisasterRules InRules = DisasterRules{})
			: Instance(Config(Seed)), Rules(InRules)
		{
			Setup = WorldSetup::Declare(Instance);
			Population = PopulationTypes::Declare(Instance);
			Hist = HistoryTypes::Declare(Instance);
			Languages = LanguageTypes::Declare(Instance);
			Religion = ReligionTypes::Declare(Instance);
			Types = DisasterTypes::Declare(Instance);
			Growth = std::make_unique<PopulationSystem>(Instance, Setup, Population, PopRules);
			Move = std::make_unique<MigrationSystem>(Instance, Setup, Population, PopRules);
			Eras = std::make_unique<EraSystem>(Instance, Hist, EraRules{});
			Naming = std::make_unique<LanguageSystem>(Instance, Setup, Population, Languages, LanguageRules{});
			Faith = std::make_unique<ReligionSystem>(Instance, Setup, Population, Religion, ReligionRules{});
			Faith->NameWith(Languages);
			Listener =
				std::make_unique<FaithListener>(Instance, Setup, Population, Religion, ReligionRules{}, Faith.get());
			Doom = std::make_unique<DisasterSystem>(Instance, Setup, Population, Types, Rules);
			Doom->ShakeFaith(Religion, Faith.get());
			Doom->OpenEras(Eras.get());
			Instance.Systems().Add(Growth.get());
			Instance.Systems().Add(Move.get());
			Instance.Systems().Add(Eras.get());
			Instance.Systems().Add(Naming.get());
			Instance.Systems().Add(Faith.get());
			Instance.Systems().Add(Doom.get());
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
			InitializeFaith(Instance, Religion);
			InitializeDisasters(Instance, Types);
			return SeedCultures(Instance, Setup, Population, PopRules, Instance.Now()) > 0;
		}
		World Instance;
		WorldSetup Setup;
		PopulationTypes Population;
		HistoryTypes Hist;
		LanguageTypes Languages;
		ReligionTypes Religion;
		DisasterTypes Types;
		PopulationRules PopRules;
		DisasterRules Rules;
		std::unique_ptr<PopulationSystem> Growth;
		std::unique_ptr<MigrationSystem> Move;
		std::unique_ptr<EraSystem> Eras;
		std::unique_ptr<LanguageSystem> Naming;
		std::unique_ptr<ReligionSystem> Faith;
		std::unique_ptr<FaithListener> Listener;
		std::unique_ptr<DisasterSystem> Doom;
	};

	void LogStats(const char* Title, const DisasterStats& S)
	{
		VAELEN_LOG_INFO(LogDisasters,
						"%s: %u disasters (drought %u, flood %u, eruption %u, plague %u; severity 1/2/3 = %u/%u/%u), "
						"%llu deaths, %u omens (%u dropped, %u pending), %u regions struck",
						Title, S.Total, S.PerKind[0], S.PerKind[1], S.PerKind[2], S.PerKind[3], S.PerSeverity[0],
						S.PerSeverity[1], S.PerSeverity[2], static_cast<unsigned long long>(S.Deaths), S.Omens,
						S.Dropped, S.Pending, S.RegionsStruck);
	}
} // namespace

VAELEN_TEST(Disasters, HazardsComeFromTheWorld)
{
	DoomWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	const std::vector<RegionHazard> Hazards = ComputeHazards(W.Instance, W.Setup, W.Rules);
	uint32 Regions = 0;
	W.Instance.Components()
		.GetPool(W.Setup.RegionTypes_.Region)
		.ForEach([&](EntityHandle, const RegionInfo&) { ++Regions; });
	VT_CHECK_EQ(Hazards.size(), usize{Regions} + 1);
	uint32 Dry = 0;
	uint32 Wet = 0;
	uint32 Volcanic = 0;
	for (uint32 R = 1; R < Hazards.size(); ++R)
	{
		const RegionHazard& H = Hazards[R];
		VT_CHECK(H.Tiles > 0);
		VT_CHECK(H.MoisturePerMille <= 1000);
		for (uint32 K = 0; K < Kinds; ++K)
		{
			VT_CHECK(H.Risk[K] <= 1000);
		}
		// Each risk exists only where its cause exists.
		VT_CHECK_EQ(H.Risk[0] > 0, H.MoisturePerMille < W.Rules.DroughtMoisture);
		VT_CHECK_EQ(H.Risk[1] > 0, H.RiverTiles > 0);
		VT_CHECK_EQ(H.Risk[2] > 0, H.MountainTiles > 0);
		VT_CHECK_EQ(H.Risk[3], 0u); // plague is per year
		VT_CHECK(H.MountainTiles <= H.Tiles && H.RiverTiles <= H.Tiles);
		Dry += H.Risk[0] > 0 ? 1u : 0u;
		Wet += H.Risk[1] > 0 ? 1u : 0u;
		Volcanic += H.Risk[2] > 0 ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogDisasters, "AELVOR 128: %u regions, %u at drought risk, %u at flood risk, %u at eruption risk",
					Regions, Dry, Wet, Volcanic);
	VT_CHECK(Dry > 0 && Wet > 0 && Volcanic > 0);
	VT_CHECK(Dry < Regions && Volcanic < Regions);
	// Deterministic, and the plague risk follows density.
	const std::vector<RegionHazard> Again = ComputeHazards(W.Instance, W.Setup, W.Rules);
	for (uint32 R = 1; R < Hazards.size(); ++R)
	{
		VT_CHECK(std::equal(Hazards[R].Risk, Hazards[R].Risk + Kinds, Again[R].Risk));
	}
	VT_CHECK_EQ(PlagueRisk(0, 100, W.Rules), 0u);
	VT_CHECK_EQ(PlagueRisk(100, 0, W.Rules), 0u);
	VT_CHECK_EQ(PlagueRisk(20 * 100, 100, W.Rules), 500u); // half the full density
	VT_CHECK_EQ(PlagueRisk(400 * 100, 100, W.Rules), 1000u);
	// Rules move the thresholds.
	DisasterRules Wetter;
	Wetter.DroughtMoisture = 1000;
	const std::vector<RegionHazard> AllDry = ComputeHazards(W.Instance, W.Setup, Wetter);
	uint32 DryNow = 0;
	for (uint32 R = 1; R < AllDry.size(); ++R)
	{
		DryNow += AllDry[R].Risk[0] > 0 ? 1u : 0u;
	}
	VT_CHECK(DryNow > Dry);
	// A world without regions has no hazards.
	DoomWorld Drowned(12);
	WorldGenConfig Flood;
	Flood.Width = 32;
	Flood.Height = 32;
	Flood.SeaLevel = Fix64::FromInt(100000).Raw;
	VT_REQUIRE(GenerateWorld(Drowned.Instance, Drowned.Setup, Flood));
	VT_CHECK(ComputeHazards(Drowned.Instance, Drowned.Setup, Drowned.Rules).empty());
	InitializeHistory(Drowned.Instance, Drowned.Hist);
	InitializeFaith(Drowned.Instance, Drowned.Religion);
	InitializeDisasters(Drowned.Instance, Drowned.Types);
	Drowned.Instance.TickMany(Year * 3);
	VT_CHECK_EQ(MeasureDisasters(Drowned.Instance, Drowned.Types).Omens, 0u);
}

VAELEN_TEST(Disasters, EveryDisasterHasAPlaceAndACauseAndFrequenciesStayInBands)
{
	DoomWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	for (uint32 Century = 1; Century <= 5; ++Century)
	{
		W.Instance.TickMany(Year * 100);
		char Title[32];
		std::snprintf(Title, sizeof(Title), "year %u", Century * 100);
		LogStats(Title, MeasureDisasters(W.Instance, W.Types));
	}
	const DisasterStats S = MeasureDisasters(W.Instance, W.Types);
	const PopulationStats People = MeasurePopulation(W.Instance, W.Population);
	VAELEN_LOG_INFO(LogDisasters, "year 500: %llu people, %u cultures", static_cast<unsigned long long>(People.People),
					People.Cultures);
	// Bands over 500 years on a 99-region continent.
	VT_CHECK(S.Total >= 50 && S.Total <= 500);
	for (uint32 K = 0; K < Kinds; ++K)
	{
		VT_CHECK_MSG(S.PerKind[K] >= 2 && S.PerKind[K] <= 300, "%s struck %u times",
					 DisasterName(static_cast<DisasterKind>(K)), S.PerKind[K]);
	}
	VT_CHECK(S.PerSeverity[0] > 0 && S.PerSeverity[2] > 0);
	VT_CHECK(S.Omens >= S.Total);
	VT_CHECK_EQ(S.Dropped, 0u);
	VT_CHECK(S.RegionsStruck >= 10);
	VT_CHECK(People.People > 10000); // the world survives its disasters
	VT_CHECK_EQ(S.Total, S.PerKind[0] + S.PerKind[1] + S.PerKind[2] + S.PerKind[3]);

	// Every disaster: a place, an omen that precedes it about the same place and kind,
	// deaths within the people it found.
	const EventLog& Log = W.Instance.Log();
	std::vector<EntityHandle> Regions;
	W.Instance.Components()
		.GetPool(W.Setup.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				if (Regions.size() <= R.Index)
				{
					Regions.resize(R.Index + 1u);
				}
				Regions[R.Index] = H;
			});
	const std::vector<RegionHazard> Hazards = ComputeHazards(W.Instance, W.Setup, W.Rules);
	uint32 Records = 0;
	W.Instance.Components()
		.GetPool(W.Types.Disaster)
		.ForEach(
			[&](EntityHandle, const DisasterInfo& D)
			{
				++Records;
				VT_CHECK(D.Region != 0 && D.Region < Regions.size());
				VT_CHECK(D.Kind < Kinds);
				VT_CHECK(D.Severity >= 1 && D.Severity <= 3);
				VT_CHECK(D.Deaths <= D.PeopleBefore);
				VT_CHECK(D.Omen != 0);
				const Event* Omen = FindEvent(Log, PersistentId{D.Omen});
				VT_REQUIRE(Omen != nullptr);
				VT_CHECK(Omen->Is(OmenEvent));
				VT_CHECK(Omen->Tick < D.Struck);
				VT_CHECK_EQ(Omen->Get<OmenPayload>().Region, D.Region);
				VT_CHECK_EQ(Omen->Get<OmenPayload>().Kind, D.Kind);
				VT_CHECK_EQ(Omen->Subject.Value, W.Instance.Entities().GetId(Regions[D.Region]).Value);
				// The physical kinds only strike where the world allows them.
				if (D.Kind != static_cast<uint32>(DisasterKind::Plague))
				{
					VT_CHECK(Hazards[D.Region].Risk[D.Kind] > 0);
				}
				else
				{
					VT_CHECK(D.PeopleBefore > 0);
				}
			});
	VT_CHECK_EQ(Records, S.Total);
	uint32 StruckEvents = 0;
	uint32 OmenEvents = 0;
	for (const Event& E : Log.All())
	{
		OmenEvents += E.Is(OmenEvent) ? 1u : 0u;
		if (E.Is(DisasterStruckEvent))
		{
			++StruckEvents;
			VT_CHECK(E.Cause.IsValid() && E.Subject.IsValid());
			const Event* Cause = FindEvent(Log, E.Cause);
			VT_REQUIRE(Cause != nullptr);
			VT_CHECK(Cause->Is(OmenEvent));
			VT_CHECK_EQ(Cause->Get<OmenPayload>().Region, E.Get<DisasterPayload>().Region);
		}
	}
	VT_CHECK_EQ(StruckEvents, S.Total);
	VT_CHECK_EQ(OmenEvents, S.Omens);
	// Consequences: at least one faith founded by a disaster, at least one era opened by one.
	uint32 FaithsFromDisasters = 0;
	W.Instance.Components()
		.GetPool(W.Religion.Religion)
		.ForEach(
			[&](EntityHandle, const ReligionInfo& Rg)
			{
				if (Rg.Kind == static_cast<uint32>(FoundingKind::Requested))
				{
					const Event* Cause = FindEvent(Log, PersistentId{Rg.FoundingEvent});
					VT_CHECK(Cause != nullptr && Cause->Is(DisasterStruckEvent));
					FaithsFromDisasters += Cause != nullptr && Cause->Is(DisasterStruckEvent) ? 1u : 0u;
				}
			});
	uint32 ErasFromDisasters = 0;
	W.Instance.Components()
		.GetPool(W.Hist.Era)
		.ForEach(
			[&](EntityHandle, const EraInfo& E)
			{
				if (E.Trigger == static_cast<uint32>(EraTrigger::Requested))
				{
					const Event* Cause = FindEvent(Log, PersistentId{E.Cause});
					ErasFromDisasters += Cause != nullptr && Cause->Is(DisasterStruckEvent) ? 1u : 0u;
				}
			});
	VAELEN_LOG_INFO(LogDisasters, "consequences: %u faiths founded after a disaster, %u eras opened by one",
					FaithsFromDisasters, ErasFromDisasters);
	VT_CHECK(FaithsFromDisasters >= 1);
	VT_CHECK(ErasFromDisasters >= 1 && ErasFromDisasters <= 40);
}

VAELEN_TEST(Disasters, RulesShapeTheFrequencies)
{
	// No omens at all.
	DisasterRules Quiet;
	for (uint32 K = 0; K < Kinds; ++K)
	{
		Quiet.OmenPerMille[K] = 0;
	}
	DoomWorld Q(11, Quiet);
	VT_REQUIRE(Q.Start(64));
	Q.Instance.TickMany(Year * 100);
	VT_CHECK_EQ(MeasureDisasters(Q.Instance, Q.Types).Omens, 0u);
	VT_CHECK_EQ(MeasureDisasters(Q.Instance, Q.Types).Total, 0u);
	// Omens that never strike.
	DisasterRules Harmless;
	Harmless.StrikePerMille = 0;
	DoomWorld H(11, Harmless);
	VT_REQUIRE(H.Start(64));
	H.Instance.TickMany(Year * 100);
	VT_CHECK(MeasureDisasters(H.Instance, H.Types).Omens > 0);
	VT_CHECK_EQ(MeasureDisasters(H.Instance, H.Types).Total, 0u);
	// Default against doubled omens: more disasters, fewer people.
	DoomWorld D(11);
	VT_REQUIRE(D.Start(64));
	D.Instance.TickMany(Year * 100);
	DisasterRules Cursed;
	for (uint32 K = 0; K < Kinds; ++K)
	{
		Cursed.OmenPerMille[K] = 1000;
	}
	Cursed.DroughtMoisture = 1000;
	Cursed.PlagueDensity = 1;
	DoomWorld C(11, Cursed);
	VT_REQUIRE(C.Start(64));
	C.Instance.TickMany(Year * 100);
	const DisasterStats Default = MeasureDisasters(D.Instance, D.Types);
	const DisasterStats Doomed = MeasureDisasters(C.Instance, C.Types);
	LogStats("default 64 x 100 years", Default);
	LogStats("cursed 64 x 100 years", Doomed);
	VT_CHECK(Doomed.Total > Default.Total);
	VT_CHECK(Doomed.Deaths > Default.Deaths);
	VT_CHECK(Doomed.Dropped > 0); // more omens than the queue holds
	VT_CHECK(MeasurePopulation(C.Instance, C.Population).People < MeasurePopulation(D.Instance, D.Population).People);
	// Deaths per mille of zero kill nobody.
	DisasterRules Mild;
	for (uint32 K = 0; K < Kinds; ++K)
	{
		for (uint32 Sv = 0; Sv < 3; ++Sv)
		{
			Mild.DeathsPerMille[K][Sv] = 0;
		}
	}
	DoomWorld M(11, Mild);
	VT_REQUIRE(M.Start(64));
	M.Instance.TickMany(Year * 100);
	VT_CHECK(MeasureDisasters(M.Instance, M.Types).Total > 0);
	VT_CHECK_EQ(MeasureDisasters(M.Instance, M.Types).Deaths, 0u);
	// Double initialisation is refused.
	{
		VaelenTest::ScopedAssertCapture Capture;
		VT_CHECK(InitializeDisasters(M.Instance, M.Types).IsNull());
#if VAELEN_ASSERTS_ENABLED
		VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
	}
}

VAELEN_TEST(Disasters, DeterministicAndSnapshotSafe)
{
	DoomWorld A(11);
	DoomWorld B(11);
	VT_REQUIRE(A.Start(64) && B.Start(64));
	A.Instance.TickMany(Year * 80);
	B.Instance.TickMany(Year * 80);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Instance.Log().Digest(), B.Instance.Log().Digest());
	// Find a moment with pending omens and snapshot there.
	uint32 Tries = 0;
	while (MeasureDisasters(A.Instance, A.Types).Pending == 0 && Tries < 50)
	{
		A.Instance.TickMany(Year);
		B.Instance.TickMany(Year);
		++Tries;
	}
	VT_REQUIRE(MeasureDisasters(A.Instance, A.Types).Pending > 0);
	A.Instance.TickMany(3);
	B.Instance.TickMany(3);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	DoomWorld R(11);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(MeasureDisasters(R.Instance, R.Types).Pending, MeasureDisasters(A.Instance, A.Types).Pending);
	A.Instance.TickMany(Year * 40);
	R.Instance.TickMany(Year * 40);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(MeasureDisasters(R.Instance, R.Types).Total, MeasureDisasters(A.Instance, A.Types).Total);
	// A different seed strikes differently.
	DoomWorld C(12);
	VT_REQUIRE(C.Start(64));
	C.Instance.TickMany(Year * 80);
	VT_CHECK(ComputeStateDigest(C.Instance) != ComputeStateDigest(B.Instance));
}

VAELEN_TEST(Disasters, FrozenDisastersAreReproducedByEveryCompilerAndPlatform)
{
	DoomWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	W.Instance.TickMany(Year * 500);
	const Hash64 D = ComputeStateDigest(W.Instance);
	const DisasterStats S = MeasureDisasters(W.Instance, W.Types);
	VAELEN_LOG_INFO(LogDisasters, "frozen: disaster128=%016llx disasters=%u deaths=%llu",
					static_cast<unsigned long long>(D), S.Total, static_cast<unsigned long long>(S.Deaths));
	VT_CHECK_EQ(D, Hash64{VAELEN_DISASTER_FROZEN_128});
	VT_CHECK_EQ(S.Total, uint32{VAELEN_DISASTER_COUNT_128});
	VT_CHECK_EQ(S.Deaths, uint64{VAELEN_DISASTER_DEATHS_128});
}
