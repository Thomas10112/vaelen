// VAELEN - Tests/Sim
// Phase 03.08: the Phase 03 gate - a long history with every invariant checked.
//
// STATUS: VALIDATED (Phase 03)

#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (03.08): AELVOR 256 after
// 1000, 1500 and 2000 years (the first five centuries are frozen in 03.06).
#define VAELEN_GATE_FROZEN_256_1000 0x3ee019d12e58c774ull
#define VAELEN_GATE_FROZEN_256_1500 0xa39530dc59e041e0ull
#define VAELEN_GATE_FROZEN_256_2000 0x2ce95360ad09dda9ull
#define VAELEN_GATE_LOG_256_2000 0x1b884d94518bfe79ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogHistoryGate);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{}) { Instance.Build(); }
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

	// Every invariant Phase 03 promises, checked on the live state. Returns the
	// number of failures (each also reported through the harness).
	uint32 CheckInvariants(VaelenTest::Context& Ctx, Run& W, uint32 Year)
	{
		uint32 Failures = 0;
		const PreHistoryTypes& T = W.Ages.Types();
		const World& World_ = W.Instance;
		// Regions that sent a migration wave in the last tick.
		std::vector<uint8> LeftLastTick(1024, 0);
		const std::vector<Event>& Events = World_.Log().All();
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick + 1 < World_.Now())
			{
				break;
			}
			if (E.Is(MigrationWaveEvent) && E.Get<RegionPeople>().Reserved < LeftLastTick.size())
			{
				LeftLastTick[E.Get<RegionPeople>().Reserved] = 1;
			}
		}
		// Population: counts consistent, capacity respected within a wave, majority right.
		World_.Components()
			.GetPool(T.Population.Population)
			.ForEach(
				[&](EntityHandle H, const RegionPopulation& P)
				{
					RegionPopulation Copy = P;
					Copy.Recount();
					if (Copy.Total != P.Total || Copy.Majority != P.Majority)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: region population bookkeeping stale", Year);
					}
					if (P.Total > P.Capacity + P.Capacity / 10 + 1000)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: %u people for %u capacity", Year, P.Total, P.Capacity);
					}
					// Faith never exceeds the people, except where a wave of the last
					// tick is still to be delivered (the bus dispatches next tick).
					const RegionFaith* F = World_.Components().GetPool(T.Religion.Faith).TryGet(H);
					const WorldGen::RegionInfo* Info =
						World_.Components().GetPool(T.World.RegionTypes_.Region).TryGet(H);
					const bool WaveLeft =
						Info != nullptr && Info->Index < LeftLastTick.size() && LeftLastTick[Info->Index] != 0;
					if (F != nullptr && F->Total() > P.Total && !WaveLeft)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: %u believers for %u people", Year, F->Total(), P.Total);
					}
				});
		// Names: unique per scope.
		const NamingStats N = MeasureNames(World_, T.Languages);
		if (N.Duplicates != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u duplicate names", Year, N.Duplicates);
		}
		// Languages: exactly one per culture.
		const PopulationStats P = MeasurePopulation(World_, T.Population);
		if (N.Languages != P.Cultures)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u languages for %u cultures", Year, N.Languages, P.Cultures);
		}
		// Religions: every founding event in the log; queues bounded.
		const FaithStats F = MeasureFaith(World_, T.Population, T.Religion);
		if (F.Pending > FaithState::MaxPending)
		{
			++Failures;
		}
		World_.Components()
			.GetPool(T.Religion.Religion)
			.ForEach(
				[&](EntityHandle, const ReligionInfo& Rg)
				{
					if (Rg.FoundingEvent == 0 || FindEvent(World_.Log(), PersistentId{Rg.FoundingEvent}) == nullptr)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: religion %u without a founding event", Year, Rg.Index);
					}
				});
		// Disasters: every record has a place and an omen.
		World_.Components()
			.GetPool(T.Disasters.Disaster)
			.ForEach(
				[&](EntityHandle, const DisasterInfo& D)
				{
					if (D.Region == 0 || D.Omen == 0 || FindEvent(World_.Log(), PersistentId{D.Omen}) == nullptr)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: disaster %u without a place or an omen", Year, D.Index);
					}
				});
		// Eras: contiguous from tick 0, exactly one open.
		std::vector<EraInfo> Eras;
		World_.Components().GetPool(T.History.Era).ForEach([&](EntityHandle, const EraInfo& E) { Eras.push_back(E); });
		uint32 Open = 0;
		for (const EraInfo& E : Eras)
		{
			Open += E.End == 0 ? 1u : 0u;
			for (const EraInfo& Next : Eras)
			{
				if (Next.Index == E.Index + 1 && E.End != Next.Start)
				{
					++Failures;
					VT_CHECK_MSG(false, "year %u: gap between era %u and %u", Year, E.Index, Next.Index);
				}
			}
		}
		if (Open != 1 || (Eras.empty() ? true : Eras.front().Start != 0 && Eras.size() > 0 && Eras[0].Index != 1))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u open eras", Year, Open);
		}
		return Failures;
	}

	double Seconds(std::chrono::steady_clock::time_point Start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	}
} // namespace

VAELEN_TEST(HistoryGate, TwoThousandYearsAt256HoldEveryInvariantAndFreeze)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(256), 0, false));
	const auto Start = std::chrono::steady_clock::now();
	uint32 Failures = 0;
	Hash64 At1000 = 0;
	Hash64 At1500 = 0;
	for (uint32 Decade = 1; Decade <= 200; ++Decade)
	{
		W.Ages.Run(10);
		Failures += CheckInvariants(Ctx, W, Decade * 10);
		if (Failures > 20)
		{
			break; // enough
		}
		if (Decade % 50 == 0)
		{
			const PreHistoryReport R = ReportPreHistory(W.Instance, W.Ages.Types());
			std::string Text;
			ExportPreHistoryText(R, Text);
			VAELEN_LOG_INFO(LogHistoryGate, "AELVOR 256 after %u years:\n%s", Decade * 10, Text.c_str());
			if (Decade == 100)
			{
				At1000 = R.StateDigest;
			}
			if (Decade == 150)
			{
				At1500 = R.StateDigest;
			}
		}
	}
	const double Elapsed = Seconds(Start);
	VT_CHECK_EQ(Failures, 0u);
	const PreHistoryReport R = ReportPreHistory(W.Instance, W.Ages.Types());
	VAELEN_LOG_INFO(
		LogHistoryGate,
		"gate: 2000 years at 256 in %.1f s [asserts %s]; frozen: 1000=%016llx 1500=%016llx 2000=%016llx log=%016llx",
		Elapsed, VAELEN_ASSERTS_ENABLED ? "on" : "off", static_cast<unsigned long long>(At1000),
		static_cast<unsigned long long>(At1500), static_cast<unsigned long long>(R.StateDigest),
		static_cast<unsigned long long>(R.LogDigest));
	// The world is alive and layered after two millennia.
	VT_CHECK(R.Population.People * 2 > R.Population.Capacity);
	VT_CHECK(R.Population.Cultures >= 4);
	VT_CHECK(R.Faith.Religions >= 2);
	VT_CHECK(R.Disasters.Total > 400);
	VT_CHECK(R.Eras >= 20);
	VT_CHECK(R.Records > 1000);
	VT_CHECK_EQ(R.Names.Duplicates, 0u);
	// The chronicle still resolves completely.
	const ChronicleStats C = CheckChronicle(W.Instance, W.Ages.Types());
	VT_CHECK_EQ(C.Resolved, C.Records);
	VT_CHECK_EQ(C.EraConsistent, C.Records);
	VT_CHECK_EQ(C.Described, C.Records);
	VT_CHECK_EQ(At1000, Hash64{VAELEN_GATE_FROZEN_256_1000});
	VT_CHECK_EQ(At1500, Hash64{VAELEN_GATE_FROZEN_256_1500});
	VT_CHECK_EQ(R.StateDigest, Hash64{VAELEN_GATE_FROZEN_256_2000});
	VT_CHECK_EQ(R.LogDigest, Hash64{VAELEN_GATE_LOG_256_2000});
}

VAELEN_TEST(HistoryGate, ASnapshotAtYearThousandContinuesToTheSameMillennium)
{
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(256), 1000));
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VAELEN_LOG_INFO(LogHistoryGate, "snapshot at year 1000: %zu bytes", Image.size());
	Run B(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(ComputeStateDigest(B.Instance), Hash64{VAELEN_GATE_FROZEN_256_1000});
	A.Ages.Run(1000);
	B.Ages.Run(1000);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), Hash64{VAELEN_GATE_FROZEN_256_2000});
	VT_CHECK_EQ(ComputeStateDigest(B.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(B.Instance.Log().Digest(), A.Instance.Log().Digest());
	// The restored world re-saves the same bytes.
	std::vector<uint8> Again;
	SaveSnapshot(B.Instance, Again);
	std::vector<uint8> Direct;
	SaveSnapshot(A.Instance, Direct);
	VT_CHECK(Again == Direct);
}
