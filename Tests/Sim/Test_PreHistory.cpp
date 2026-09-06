// VAELEN - Tests/Sim
// Phase 03.06: the pre-history run and the starting state.
//
// STATUS: VALIDATED (Phase 03)

#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (03.06). One state digest
// per century of the AELVOR 256 reference run.
#define VAELEN_PREHISTORY_FROZEN_256_100 0x8142f69ae490df39ull
#define VAELEN_PREHISTORY_FROZEN_256_200 0x3ed2555f9853634full
#define VAELEN_PREHISTORY_FROZEN_256_300 0x275151ee080d8617ull
#define VAELEN_PREHISTORY_FROZEN_256_400 0x380dfdd33b692ceeull
#define VAELEN_PREHISTORY_FROZEN_256_500 0x779f0e33912acd7bull
#define VAELEN_PREHISTORY_LOG_256_500 0x53a2b57f3ceaad11ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPreHistory);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, PreHistoryRules Rules = PreHistoryRules{})
			: Instance(Config(Seed)), Ages(Instance, Rules)
		{
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
		World Instance;
		PreHistory Ages;
	};

	void LogReport(const char* Title, const PreHistoryReport& R)
	{
		std::string Text;
		ExportPreHistoryText(R, Text);
		VAELEN_LOG_INFO(LogPreHistory, "%s:\n%s", Title, Text.c_str());
	}

	double Seconds(std::chrono::steady_clock::time_point Start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	}
} // namespace

VAELEN_TEST(PreHistory, OneCallRunsEveryPhaseThreeSystem)
{
	Run W(AelvorSeed);
	VT_CHECK(!W.Ages.HasHistory());
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 500));
	VT_CHECK(W.Ages.HasHistory());
	const PreHistoryReport R = ReportPreHistory(W.Instance, W.Ages.Types());
	LogReport("AELVOR 128 after 500 years", R);
	VT_CHECK_EQ(R.Years, 500u);
	VT_CHECK_EQ(R.Tick, TicksPerYear * 500);
	VT_CHECK(R.Regions > 50);
	VT_CHECK(R.Population.People > 10000);
	VT_CHECK(R.Population.Cultures >= 4);
	VT_CHECK_EQ(R.Names.Languages, R.Population.Cultures);
	VT_CHECK_EQ(R.Names.Duplicates, 0u);
	VT_CHECK(R.Names.PerScope[static_cast<uint32>(NameScope::Region)] >= R.Population.SettledRegions);
	VT_CHECK(R.Faith.Religions >= 1);
	VT_CHECK(R.Faith.Adherents <= R.Faith.People);
	VT_CHECK(R.Disasters.Total > 0);
	VT_CHECK(R.Eras >= 5); // one per century at least
	VT_CHECK(R.Records > R.Eras + R.Population.Cultures + R.Faith.Religions + R.Disasters.Total);
	VT_CHECK(R.Events > R.Records);
	VT_CHECK(R.Entities > R.Records);
	// Every Phase 03 system is scheduled, in one deterministic order.
	const std::vector<std::string_view> Order = W.Instance.Systems().GetOrder();
	VT_CHECK_EQ(Order.size(), usize{6});
	uint32 Found = 0;
	for (const std::string_view Name : Order)
	{
		Found += Name == "Population" || Name == "Migration" || Name == "Eras" || Name == "Languages" ||
						 Name == "Religions" || Name == "Disasters"
					 ? 1u
					 : 0u;
	}
	VT_CHECK_EQ(Found, 6u);
	// The text is deterministic and mentions every section.
	std::string Text;
	ExportPreHistoryText(R, Text);
	std::string Again;
	ExportPreHistoryText(ReportPreHistory(W.Instance, W.Ages.Types()), Again);
	VT_CHECK(Text == Again);
	VT_CHECK(Text.find("peoples:") != std::string::npos && Text.find("faiths:") != std::string::npos &&
			 Text.find("disasters:") != std::string::npos && Text.find("chronicle:") != std::string::npos);
	// Misuse: a second generation on a world with history is refused and changes nothing.
	const Hash64 Before = ComputeStateDigest(W.Instance);
	VT_CHECK(!W.Ages.Generate(Run::Square(128), 1));
	VT_CHECK_EQ(ComputeStateDigest(W.Instance), Before);
	// Misuse: an invalid config is refused on a fresh world.
	Run Bad(3);
	WorldGenConfig Broken;
	Broken.Width = 0;
	Broken.Height = 0;
	{
		VaelenTest::ScopedAssertCapture Capture;
		VT_CHECK(!Bad.Ages.Generate(Broken, 1));
	}
	VT_CHECK(!Bad.Ages.HasHistory());
	// A drowned world generates but seeds nobody: refused, no history.
	Run Drowned(12);
	WorldGenConfig Flood = Run::Square(32);
	Flood.SeaLevel = Fix64::FromInt(100000).Raw;
	VT_CHECK(!Drowned.Ages.Generate(Flood, 1));
	VT_CHECK(!Drowned.Ages.HasHistory());
}

VAELEN_TEST(PreHistory, CenturyDigestsAreFrozenAt256)
{
	// The reference run in one call.
	Run A(AelvorSeed);
	const auto Start = std::chrono::steady_clock::now();
	VT_REQUIRE(A.Ages.Generate(Run::Square(256), 500));
	const double Elapsed = Seconds(Start);
	const PreHistoryReport R = ReportPreHistory(A.Instance, A.Ages.Types());
	LogReport("AELVOR 256 after 500 years", R);
	VAELEN_LOG_INFO(LogPreHistory, "baseline: 256 x 500 years in %.3f s [asserts %s]", Elapsed,
					VAELEN_ASSERTS_ENABLED ? "on" : "off");
	// The same run century by century, digests recorded.
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(256), 0, false));
	Hash64 Digests[5] = {};
	for (uint32 Century = 0; Century < 5; ++Century)
	{
		B.Ages.Run(100);
		Digests[Century] = ComputeStateDigest(B.Instance);
	}
	VAELEN_LOG_INFO(LogPreHistory,
					"frozen: prehistory256 100=%016llx 200=%016llx 300=%016llx 400=%016llx 500=%016llx log=%016llx",
					static_cast<unsigned long long>(Digests[0]), static_cast<unsigned long long>(Digests[1]),
					static_cast<unsigned long long>(Digests[2]), static_cast<unsigned long long>(Digests[3]),
					static_cast<unsigned long long>(Digests[4]), static_cast<unsigned long long>(R.LogDigest));
	VT_CHECK_EQ(Digests[4], R.StateDigest);
	VT_CHECK_EQ(B.Instance.Log().Digest(), R.LogDigest);
	VT_CHECK_EQ(Digests[0], Hash64{VAELEN_PREHISTORY_FROZEN_256_100});
	VT_CHECK_EQ(Digests[1], Hash64{VAELEN_PREHISTORY_FROZEN_256_200});
	VT_CHECK_EQ(Digests[2], Hash64{VAELEN_PREHISTORY_FROZEN_256_300});
	VT_CHECK_EQ(Digests[3], Hash64{VAELEN_PREHISTORY_FROZEN_256_400});
	VT_CHECK_EQ(Digests[4], Hash64{VAELEN_PREHISTORY_FROZEN_256_500});
	VT_CHECK_EQ(R.LogDigest, Hash64{VAELEN_PREHISTORY_LOG_256_500});
	// Centuries differ from each other (history moves).
	for (uint32 i = 1; i < 5; ++i)
	{
		VT_CHECK(Digests[i] != Digests[i - 1]);
	}
}

VAELEN_TEST(PreHistory, SnapshotMidHistoryContinuesIdentically)
{
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 250));
	A.Ages.Run(0);
	A.Instance.TickMany(7); // not on a yearly tick: requests and omens are pending
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run C(AelvorSeed);
	VT_CHECK(!C.Ages.HasHistory());
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(C.Ages.HasHistory());
	VT_CHECK(!C.Ages.Generate(Run::Square(128), 1)); // a restored world is not fresh
	VT_CHECK_EQ(ComputeStateDigest(C.Instance), ComputeStateDigest(A.Instance));
	A.Ages.Run(250);
	C.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(C.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(C.Instance.Log().Digest(), A.Instance.Log().Digest());
	std::string TextA;
	std::string TextC;
	ExportPreHistoryText(ReportPreHistory(A.Instance, A.Ages.Types()), TextA);
	ExportPreHistoryText(ReportPreHistory(C.Instance, C.Ages.Types()), TextC);
	VT_CHECK(TextA == TextC);
	// Two fresh runs of the same seed agree with the interrupted one.
	Run D(AelvorSeed);
	VT_REQUIRE(D.Ages.Generate(Run::Square(128), 0, false));
	D.Ages.Run(250);
	D.Instance.TickMany(7);
	D.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(D.Instance), ComputeStateDigest(A.Instance));
}

VAELEN_TEST(PreHistory, RulesSeedsAndSizesMatter)
{
	Run A(11);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	Run B(12);
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance));
	// Rules flow to every system: a quiet world has no disasters and no faiths.
	PreHistoryRules Quiet;
	for (uint32 K = 0; K < static_cast<uint32>(DisasterKind::Count); ++K)
	{
		Quiet.Disasters.OmenPerMille[K] = 0;
	}
	Quiet.Religion.FoundOnEra = 0;
	Quiet.Religion.SchismOnSplit = 0;
	Quiet.Years = 120;
	Run Q(11, Quiet);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(64), 0)); // Rules.Years
	const PreHistoryReport RQ = ReportPreHistory(Q.Instance, Q.Ages.Types());
	VT_CHECK_EQ(RQ.Years, 120u);
	VT_CHECK_EQ(RQ.Disasters.Total, 0u);
	VT_CHECK_EQ(RQ.Faith.Religions, 0u);
	VT_CHECK(RQ.Population.People > 0);
	const PreHistoryReport RA = ReportPreHistory(A.Instance, A.Ages.Types());
	VT_CHECK(RA.Disasters.Total > 0);
	// A non-square world runs too.
	Run N(11);
	WorldGenConfig Wide;
	Wide.Width = 96;
	Wide.Height = 48;
	VT_REQUIRE(N.Ages.Generate(Wide, 60));
	VT_CHECK(ReportPreHistory(N.Instance, N.Ages.Types()).Population.People > 0);
}

VAELEN_TEST(PreHistory, Baseline1024)
{
	Run W(AelvorSeed);
	const auto Start = std::chrono::steady_clock::now();
	VT_REQUIRE(W.Ages.Generate(Run::Square(1024), 500));
	const double Elapsed = Seconds(Start);
	const PreHistoryReport R = ReportPreHistory(W.Instance, W.Ages.Types());
	LogReport("AELVOR 1024 after 500 years", R);
	VAELEN_LOG_INFO(LogPreHistory, "baseline: 1024 x 500 years in %.3f s [asserts %s]", Elapsed,
					VAELEN_ASSERTS_ENABLED ? "on" : "off");
	VT_CHECK(R.Population.People > 100000);
	VT_CHECK(R.Population.Cultures >= 4);
	VT_CHECK(R.Faith.Religions >= 1);
	VT_CHECK(R.Disasters.Total > 0);
	VT_CHECK_EQ(R.Names.Duplicates, 0u);
	VT_CHECK(R.Faith.Adherents <= R.Faith.People);
}
