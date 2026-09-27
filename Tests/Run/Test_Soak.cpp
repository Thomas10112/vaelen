// VAELEN - VaelenRun tests
// Phase 20 task 20.01: the soak - three years of the product's world, played
// a day at a time, and what it costs as it goes.
//
// Every gate in this tree runs a world for years BEFORE play and then plays
// it for a month. Nothing plays it for years. A player will, and what grows
// while they do - the day's cost, the process's memory, the container's size,
// the log the container carries - is exactly what no month can show: the
// 49 `Log().All()` call sites that scan the whole log every tick (ROADMAP,
// the close of Phase 16) cost more on day 1000 than on day 1, or they do not,
// and this is where that is measured rather than argued.
//
// The world is the subsystem's (VaelenWorldSubsystem::Begin): 128 tiles,
// 300+120 years, Play and Stream, the climate the default. The days are
// Run.Door's round-robin of the eight verbs through the door, so the soak
// exercises the same door the engine does, refusals included. LOGGED, not
// asserted, per ADR-0109: every cost is printed with the year it was measured
// in, and what is ASSERTED is what does not depend on the machine - the days
// turned, a world that still holds people, a container that still reads
// back to the world's own digest every year, and the state the three years
// come to, pinned, so that ten compilers must agree on a thousand days of
// play as they agree on a month.
//
// STATUS: PROTOTYPE (Phase 20 task 20.01)
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Take.h"

#include <chrono>
#include <cstdio>
#include <vector>

#if defined(__linux__)
#	include <unistd.h>
#endif

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogRunSoak);

	/// THE PIN: the state digest after three years of the round-robin at 128,
	/// 300+120 years, Play and Stream, the climate on. Measured 2026-09-26 on
	/// gcc debug and release alike; every CI leg must come to it.
	constexpr Hash64 SoakFrozenState = 0x16954db42400b164ull;

	constexpr uint32 SoakYears = 3;
	constexpr uint32 DaysPerYear = 360;

	double Ms(std::chrono::steady_clock::time_point T0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	}

	/// The process's resident set now, in bytes - a soak is about GROWTH, so
	/// the high-water mark getrusage gives would hide the very thing looked
	/// for. Linux reads it from /proc; elsewhere 0, said as such in the line.
	uint64 ResidentBytes()
	{
#if defined(__linux__)
		std::FILE* F = std::fopen("/proc/self/statm", "r");
		if (F == nullptr)
		{
			return 0u;
		}
		unsigned long Total = 0, Resident = 0;
		const int Got = std::fscanf(F, "%lu %lu", &Total, &Resident);
		std::fclose(F);
		if (Got != 2)
		{
			return 0u;
		}
		const long Page = sysconf(_SC_PAGESIZE);
		return static_cast<uint64>(Resident) * static_cast<uint64>(Page > 0 ? Page : 4096);
#else
		return 0u;
#endif
	}

	Options SoakOptions()
	{
		Options O;
		O.Size = 128;
		O.Years = 120;
		O.Play = true;
		O.Stream = true;
		return O; // PreHistory 300 and the climate on: the defaults, as the subsystem's Begin leaves them
	}

	struct YearLine
	{
		double MeanMs = 0;
		double MaxMs = 0;
		double SumMs = 0;
		uint64 Resident = 0;
		uint64 ContainerBytes = 0;
		double ContainerMs = 0;
		uint64 Events = 0;
		uint32 People = 0;
		uint32 Person = 0;
		uint32 Alive = 0;
		uint32 Region = 0;
		uint32 Refused = 0;
		Hash64 State = 0;
	};
} // namespace

VAELEN_TEST(Soak, ThreeYearsOfTheProductsWorldPlayedADayAtATime)
{
	const auto T0 = std::chrono::steady_clock::now();
	Aelvor A(SoakOptions());
	VT_REQUIRE(A.Begin());
	const double Built = Ms(T0);
	const uint64 ResidentAfterBegin = ResidentBytes();

	Player::StartRules Rules;
	Rules.WantBound = 0; // whoever the world offers, as the subsystem asks
	Door D(A, Rules);
	const uint32 First = D.TakeUp();
	VT_CHECK_MSG(First != 0u, "the product's world offers nobody at 128 - nothing to soak");
	VT_REQUIRE(First != 0u);

	// ONE container buffer for the whole soak, built once before the first
	// year: a 26 MiB vector allocated and freed every year would be counted
	// by the next year's resident reading (the allocator keeps what it
	// freed), and the growth this test exists to see would be its own.
	// CLEARED before every build and never freed: BuildCheckpoint APPENDS to
	// Out, as SaveSnapshot does (Checkpoint.h) - the first version of this
	// test learned that from a container that doubled every year and was
	// refused by its own reader.
	std::vector<uint8> Bytes;
	{
		const auto TC = std::chrono::steady_clock::now();
		const CheckpointResult Wrote = BuildCheckpoint(A, D.Stream(), D.Rules(), Bytes);
		VT_REQUIRE(Wrote == CheckpointResult::Ok);
		VAELEN_LOG_INFO(LogRunSoak,
						"soak: day 0 at 128 (Play, Stream, climate): built in %.1f s; container %llu KiB in %.0f ms; "
						"log %llu events; resident %llu MiB after Begin, %llu MiB with the container",
						Built / 1000.0, static_cast<unsigned long long>(Bytes.size() >> 10), Ms(TC),
						static_cast<unsigned long long>(A.Instance().Log().Count()),
						static_cast<unsigned long long>(ResidentAfterBegin >> 20),
						static_cast<unsigned long long>(ResidentBytes() >> 20));
	}
	const uint64 ResidentAtStart = ResidentBytes();

	WorldGen::RegionGraphCache Ways;
	View::WorldView Frame;
	View::LifeView Life;
	YearLine Years[SoakYears];
	for (uint32 Year = 0; Year < SoakYears; ++Year)
	{
		YearLine& Y = Years[Year];
		for (uint32 Day = 0; Day < DaysPerYear; ++Day)
		{
			const uint32 Since = Year * DaysPerYear + Day;
			Player::PlayerCommand C;
			C.Kind = static_cast<uint8>(1 + Since % 8u); // Wait, Work, Rest, Eat, Move, Speak, Give, Take, again
			C.Amount = 1 + Since % 3u;
			D.Mean(C);
			const auto T = std::chrono::steady_clock::now();
			D.Day();
			const double M = Ms(T);
			Y.SumMs += M;
			Y.MaxMs = M > Y.MaxMs ? M : Y.MaxMs;
		}
		Y.MeanMs = Y.SumMs / DaysPerYear;
		Y.Resident = ResidentBytes();
		Y.Events = A.Instance().Log().Count();
		View::TakeView(A.Instance(), A.Sources(), Frame);
		View::TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
		Y.People = Frame.People;
		Y.Person = Life.Person;
		Y.Alive = Life.Alive;
		Y.Region = Life.Region;
		Y.Refused = Life.Refused;
		Y.State = A.StateDigest();

		// THE CONTAINER, every year: what a save costs and weighs at this
		// point of the life, and that it still reads back to the world's own
		// digest - the one promise a soak could quietly break.
		Bytes.clear(); // the capacity stays: no churn for the resident reading to count
		const auto TC = std::chrono::steady_clock::now();
		const CheckpointResult Wrote = BuildCheckpoint(A, D.Stream(), D.Rules(), Bytes);
		Y.ContainerMs = Ms(TC);
		VT_CHECK_MSG(Wrote == CheckpointResult::Ok, "year %u: the container was refused: %s", Year + 1,
					 CheckpointResultToString(Wrote));
		Y.ContainerBytes = static_cast<uint64>(Bytes.size());
		CheckpointView View;
		const CheckpointRefusal Read = ReadCheckpoint(Bytes.data(), Bytes.size(), View);
		VT_CHECK_MSG(Read.Result == CheckpointResult::Ok, "year %u: the container does not read: %s", Year + 1,
					 CheckpointResultToString(Read.Result));
		if (Read.Result == CheckpointResult::Ok)
		{
			VT_CHECK_MSG(ImageTrailer(View) == Y.State,
						 "year %u: the container's trailer %016llx is not the world's %016llx", Year + 1,
						 static_cast<unsigned long long>(ImageTrailer(View)), static_cast<unsigned long long>(Y.State));
		}
		VT_CHECK_MSG(Y.People > 0u, "year %u: nobody is left alive in the world", Year + 1);

		VAELEN_LOG_INFO(
			LogRunSoak,
			"soak: year %u of %u at 128 (Play, Stream, climate): day mean %.1f ms, max %.1f ms; resident %llu MiB%s; "
			"container %llu KiB in %.0f ms; log %llu events; %u alive; played %u (alive %u) in region %u, "
			"%u refused so far; state %016llx",
			Year + 1, SoakYears, Y.MeanMs, Y.MaxMs, static_cast<unsigned long long>(Y.Resident >> 20),
			Y.Resident == 0u ? " (unmeasured on this platform)" : "",
			static_cast<unsigned long long>(Y.ContainerBytes >> 10), Y.ContainerMs,
			static_cast<unsigned long long>(Y.Events), Y.People, Y.Person, Y.Alive, Y.Region, Y.Refused,
			static_cast<unsigned long long>(Y.State));
	}

	VT_CHECK_EQ(D.Days(), SoakYears * DaysPerYear);
	VT_CHECK_EQ(D.Stream().Commands.size(), static_cast<usize>(SoakYears * DaysPerYear));
	// The growth, in one line, so that a reader sees the ratchet or its
	// absence without arithmetic: year 3 against year 1, for the day, the
	// memory, the container and the log.
	const YearLine& Y1 = Years[0];
	const YearLine& Y3 = Years[SoakYears - 1];
	VAELEN_LOG_INFO(
		LogRunSoak,
		"soak: year %u over year 1: day mean x%.2f, resident %+lld MiB (%+lld MiB over the start), "
		"container x%.2f, log x%.2f; %u taking(s) on the tape over %u days",
		SoakYears, Y1.MeanMs > 0.0 ? Y3.MeanMs / Y1.MeanMs : 0.0,
		static_cast<long long>((Y3.Resident >> 20)) - static_cast<long long>((Y1.Resident >> 20)),
		static_cast<long long>((Y3.Resident >> 20)) - static_cast<long long>((ResidentAtStart >> 20)),
		Y1.ContainerBytes > 0u ? static_cast<double>(Y3.ContainerBytes) / static_cast<double>(Y1.ContainerBytes) : 0.0,
		Y1.Events > 0u ? static_cast<double>(Y3.Events) / static_cast<double>(Y1.Events) : 0.0,
		static_cast<uint32>(D.Stream().Takings.size()), SoakYears * DaysPerYear);

	// THE PIN. A thousand days of play through the door, the same on every
	// compiler CI runs, or the phase that opens here has a determinism bug
	// to find before it has a cost to cut.
	VT_CHECK_MSG(Y3.State == SoakFrozenState, "three years come to %016llx, the pin says %016llx",
				 static_cast<unsigned long long>(Y3.State), static_cast<unsigned long long>(SoakFrozenState));
}
