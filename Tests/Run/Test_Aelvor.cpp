// VAELEN - Tests/Run
// Phase 14.03: the Run holds the Atlas wiring, and the frozen pair says so.
//
// The claim that matters most is the first test: a Run with nothing set is the
// Atlas at 128/120, frame abc5a5767c6cf9dd and ground 8f7f4948f49b6e86 - the
// pair the engine and the headless kernel agreed on under ADR-0135. If either
// moves, this task STOPS and an ADR says why; that rule is in the roadmap row
// and this is the test that enforces it. The second test is the idle claim
// behind every Play option: the Phase 10 wiring with nobody taken up leaves
// the frame and the ground where they were.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Panel.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>

using namespace Vaelen;
using namespace Vaelen::Run;
using namespace Vaelen::View;

// The ADR-0135 pair. Atlas.Frozen128 holds it for the tool; this holds it for
// the module a host uses. As macros, for the reason Test_ViewGate gives: MSVC
// C4127 forbids `if (constexpr != 0)`.
#define VAELEN_RUN_FROZEN_FRAME 0xabc5a5767c6cf9ddull
#define VAELEN_RUN_FROZEN_GROUND 0x8f7f4948f49b6e86ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogRun);

	struct Pair
	{
		Hash64 Frame = 0;
		Hash64 Ground = 0;
	};

	Pair Digests(const Aelvor& A)
	{
		WorldView Frame;
		MapView Ground;
		TakeView(A.Instance(), A.Sources(), Frame);
		TakeMapView(A.Instance(), A.Sources(), Ground);
		return Pair{MeasureView(Frame).Digest, MeasureMapView(Ground).Digest};
	}

	/// A world small enough to build several of in one suite, old enough for
	/// 05.04 to have bound somebody to be.
	Options Small()
	{
		Options O;
		O.Size = 96;
		O.PreHistory = 240;
		O.Years = 60;
		return O;
	}

	Player::StartRules Anywhere()
	{
		Player::StartRules R;
		R.PreferOre = 0;
		R.ToAge = 25;
		// Whoever is there: a bound start is 10.02's rule and the phase gate's
		// claim at 256; a world of 96 tiles need not hold a bound person of
		// twenty-five in its busiest region, and these tests are about the
		// door, not the start.
		R.WantBound = 0;
		return R;
	}

	double Ms(std::chrono::steady_clock::time_point T0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	}
} // namespace

VAELEN_TEST(Aelvor, TheDefaultRunIsTheAtlasAndTheFrozenPairSaysSo)
{
	Aelvor A(Options{});
	VT_REQUIRE(A.Begin());
	VT_CHECK_MSG(!A.Begin(), "a Run begins once");
	VT_CHECK_EQ(A.Given().Size, 128u);
	VT_CHECK_EQ(A.Given().PreHistory, 300u);
	VT_CHECK_EQ(A.Given().Years, 120u);
	const Pair P = Digests(A);
	VT_CHECK_EQ(P.Frame, Hash64{VAELEN_RUN_FROZEN_FRAME});
	VT_CHECK_EQ(P.Ground, Hash64{VAELEN_RUN_FROZEN_GROUND});
	// Without Play, every played call is a no-op that says so.
	VT_CHECK_EQ(A.Played(), 0u);
	VT_CHECK_EQ(A.TakeUp(Anywhere()), 0u);
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Wait);
	VT_CHECK(A.Submit(C) == Player::Refusal::NoPlayer);
	VT_CHECK(!A.Release());
	VT_CHECK(A.Life().empty());
	VT_CHECK(A.Detail() != 0);
	VT_CHECK_EQ(A.Founded(), 0u);
	VAELEN_LOG_INFO(LogRun, "AELVOR 128/120 through Run::Aelvor: frame %016llx, ground %016llx, detail on region %u",
					static_cast<unsigned long long>(P.Frame), static_cast<unsigned long long>(P.Ground), A.Detail());
}

VAELEN_TEST(Aelvor, AnEmptyPlayLeavesTheFrameAndTheGroundWhereTheyWere)
{
	// The Play types are declared after everything the Atlas declares and its
	// systems act only on a played person, so a played world in which nobody
	// is played is, to the view, the world it was. This is the claim every
	// Play option rests on and the risk the roadmap names.
	Aelvor Idle(Small());
	Options P = Small();
	P.Play = true;
	Aelvor Played(P);
	VT_REQUIRE(Idle.Begin());
	VT_REQUIRE(Played.Begin());
	VT_CHECK_EQ(Played.Played(), 0u);
	const Pair I = Digests(Idle);
	const Pair E = Digests(Played);
	VT_CHECK_EQ(E.Frame, I.Frame);
	VT_CHECK_EQ(E.Ground, I.Ground);
	VT_CHECK_EQ(Played.Now(), Idle.Now());
	VT_CHECK_EQ(Played.Detail(), Idle.Detail());
	VAELEN_LOG_INFO(LogRun, "an empty play at 96: frame %016llx and ground %016llx, the same with nobody played",
					static_cast<unsigned long long>(E.Frame), static_cast<unsigned long long>(E.Ground));
}

VAELEN_TEST(Aelvor, SomebodyIsTakenUpAndLetGo)
{
	Options O = Small();
	O.Play = true;
	Aelvor A(O);
	VT_REQUIRE(A.Begin());
	VT_CHECK_EQ(A.Header().Size, 96u);
	VT_CHECK_EQ(A.Header().PreHistory, 240u);
	VT_CHECK_EQ(A.Header().Years, 60u);
	VT_CHECK_EQ(A.Header().Seed, AelvorSeed);
	// The bound start first - 10.02's rule - reported rather than required at
	// this size; then whoever is there, which is required.
	Player::StartRules Bound = Anywhere();
	Bound.WantBound = 1;
	uint32 Who = A.TakeUp(Bound);
	VAELEN_LOG_INFO(LogRun, "a bound start at 96 after 300 years: %s",
					Who != 0 ? "offered" : "nobody fits, taking whoever is there");
	Who = Who != 0 ? Who : A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0); // the world offered nobody at all
	VT_CHECK_EQ(A.Played(), Who);
	VT_CHECK(A.PlayedAlive());
	VT_CHECK_MSG(A.TakeUp(Anywhere()) == 0, "somebody is already played; Release first");
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Wait);
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	VT_CHECK(A.Release());
	VT_CHECK_EQ(A.Played(), 0u);
	VT_CHECK(!A.PlayedAlive());
	VT_CHECK(!A.Release());
	VT_CHECK(A.Submit(C) == Player::Refusal::NoPlayer);
	const uint32 Again = A.TakeUp(Anywhere());
	VT_CHECK_MSG(Again != 0, "and somebody can be taken up again");
	VAELEN_LOG_INFO(LogRun, "taken up: person %u, let go, then %u", Who, Again);
}

VAELEN_TEST(Aelvor, ADayAt256WithTheColonyIsMeasured)
{
	// LOGGED, not asserted (ADR-0109): the number the editor's day budget is
	// read against, from the machine that runs the tests and not from a guess.
	Options O;
	O.Size = 256;
	O.PreHistory = 300;
	O.Years = 20;
	O.Colony = true;
	O.Play = true;
	const auto T0 = std::chrono::steady_clock::now();
	Aelvor A(O);
	VT_REQUIRE(A.Begin());
	const double Built = Ms(T0);
	Player::StartRules OnTheOre;
	OnTheOre.WantBound = 0;
	OnTheOre.ToAge = 25;
	Door D(A, OnTheOre);
	const uint32 Who = D.TakeUp();
	VT_CHECK_MSG(Who != 0, "a colony at 256 has somebody to be");
	constexpr uint32 Days = 30;
	double Sum = 0;
	double Max = 0;
	for (uint32 i = 0; i < Days; ++i)
	{
		Player::PlayerCommand C;
		C.Kind = static_cast<uint8>(Player::Intent::Work);
		C.Amount = 1;
		D.Mean(C);
		const auto T = std::chrono::steady_clock::now();
		D.Day();
		const double M = Ms(T);
		Sum += M;
		Max = M > Max ? M : Max;
	}
	VT_CHECK_EQ(D.Days(), Days);
	VT_CHECK_EQ(A.Founded(), A.Detail());

	// 14.06: the page of this day at 256, measured rather than frozen - the
	// two frozen pages are the worlds above, and a 256 world is built by no CI
	// leg but this one. What is asserted here is what the row asks of every
	// page: it fits, it is ASCII, and its own digest is what its last row says.
	WorldGen::RegionGraphCache Ways;
	WorldView Frame;
	LifeView Life;
	ChronicleView Told;
	PanelView Page;
	const auto TP = std::chrono::steady_clock::now();
	TakeView(A.Instance(), A.Sources(), Frame);
	TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
	TakeChronicleView(A.Instance(), A.Sources(), Told);
	TakePanel(Frame, Life, Told, Page);
	const double Drawn = Ms(TP);
	const PanelStats PS = MeasurePanel(Page);
	VT_CHECK_EQ(PS.NonAscii, 0u);
	VT_CHECK_EQ(PS.Truncated, 0u);
	VT_CHECK(PS.Rows <= PanelRows);
	VT_CHECK(PS.Bytes <= 4096);
	VT_CHECK_EQ(PS.Digest, Hash64{Page.Digest});
	VT_CHECK_EQ(Page.Person, Who);
	VAELEN_LOG_INFO(
		LogRun,
		"run: day mean %.1f ms, max %.1f ms at 256 with the colony (%u days played by person %u on region %u; "
		"the world built in %.1f s); panel: bytes %u, lines %u, truncated %u, digest %016llx, taken in %.2f ms",
		Sum / Days, Max, Days, Who, A.Founded(), Built / 1000.0, PS.TextBytes, PS.Rows, PS.Truncated,
		static_cast<unsigned long long>(PS.Digest), Drawn);
}
