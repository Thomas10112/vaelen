// VAELEN - Tests/View
// Phase 14 task 14.06: the first screen, composed kernel-side.
//
// What is held here: the page is flat and fits 4 KiB; it is composed from
// three views and no world at all; the eight verbs carry the kernel's costs
// and the refusals the page can foresee; Press turns an offered verb into an
// intent and refuses an unoffered one before any world is asked; Lines writes
// what the widget draws; the last row is the page's own digest, recomputed
// from its bytes rather than read back; and two pages are frozen - the
// unplayed 128/120 Run and the 96-map round-robin after thirty days.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/View/Panel.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Player/Commands.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

// The two pages, frozen. As macros for the reason Test_ViewGate gives: MSVC
// C4127 forbids `if (constexpr != 0)`. 0 means "not frozen yet", and then the
// test prints what it saw instead of asserting.
#define VAELEN_PANEL_FROZEN_EMPTY 0x964aab6a9d4c3b03ull
#define VAELEN_PANEL_FROZEN_PLAYED 0xb119285dd7db9a5cull

using namespace Vaelen;
using namespace Vaelen::Run;
using namespace Vaelen::View;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPanel);

	Options Small()
	{
		Options O;
		O.Size = 96;
		O.PreHistory = 240;
		O.Years = 60;
		O.Play = true;
		return O;
	}

	Player::StartRules Anywhere()
	{
		Player::StartRules R;
		R.PreferOre = 0;
		R.ToAge = 25;
		R.WantBound = 0;
		return R;
	}

	double Ms(std::chrono::steady_clock::time_point T0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	}

	/// The three views of one frame, and the page of them.
	void PageOf(const Aelvor& A, WorldGen::RegionGraphCache& Ways, ChronicleView& Told, PanelView& Out)
	{
		WorldView Frame;
		LifeView Life;
		TakeView(A.Instance(), A.Sources(), Frame);
		TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
		TakeChronicleView(A.Instance(), A.Sources(), Told);
		TakePanel(Frame, Life, Told, Out);
	}

	std::string RowAt(const PanelView& V, uint32 i)
	{
		return std::string(V.Text + V.Rows[i].Begin, V.Rows[i].Length);
	}

	uint32 Rows(const PanelView& V, RowKind Kind)
	{
		uint32 n = 0;
		for (uint32 i = 0; i < V.RowCount && i < PanelRows; ++i)
		{
			n += V.Rows[i].Kind == static_cast<uint32>(Kind) ? 1u : 0u;
		}
		return n;
	}

	/// The whole page as one string, the way Lines writes it for a widget.
	std::string Drawn(const PanelView& V)
	{
		std::string Out(PanelTextBytes, '\0');
		const uint32 N = Lines(V, Out.data(), PanelTextBytes);
		Out.resize(N);
		return Out;
	}
} // namespace

VAELEN_TEST(Panel, ThePageIsFlatAndFitsFourKiB)
{
	VT_CHECK(std::is_trivially_copyable<PanelView>::value);
	VT_CHECK(std::is_standard_layout<PanelView>::value);
	VT_CHECK(std::is_trivially_copyable<VerbView>::value);
	VT_CHECK(std::is_trivially_copyable<RowView>::value);
	VT_CHECK_EQ(sizeof(PanelView), usize{4064});
	VT_CHECK(sizeof(PanelView) <= 4096);
	VT_CHECK_EQ(sizeof(VerbView), usize{20});
	VT_CHECK_EQ(sizeof(RowView), usize{16});
	PanelView A;
	PanelView B;
	VT_CHECK_EQ(MeasurePanel(A).Digest, MeasurePanel(B).Digest);
	VT_CHECK_EQ(MeasurePanel(A).Bytes, static_cast<uint32>(sizeof(PanelView)));
	VT_CHECK_EQ(MeasurePanel(A).Rows, 0u);
}

VAELEN_TEST(Panel, ThreeViewsAndNoWorldMakeAPage)
{
	// The claim of the task: the page is a function of the three views. No
	// World, no ViewSources, nothing from Take.h - three default views, which
	// is a world nobody has looked at, still compose a page.
	WorldView Frame;
	LifeView Life;
	ChronicleView Told;
	PanelView V;
	TakePanel(Frame, Life, Told, V);
	VT_CHECK_EQ(V.Person, 0u);
	VT_CHECK_EQ(V.Offered, 0u);
	VT_REQUIRE(V.RowCount >= 2);
	VT_CHECK_EQ(RowAt(V, 1), "nobody is played");
	VT_CHECK_EQ(Rows(V, RowKind::Verb), PanelVerbs);
	VT_CHECK_EQ(Rows(V, RowKind::Digest), 1u);
	VT_CHECK_EQ(Rows(V, RowKind::Chronicle), 0u);
	for (const VerbView& Slot : V.Verbs)
	{
		VT_CHECK_EQ(Slot.Offered, 0u);
		VT_CHECK_EQ(Slot.Foreseen, static_cast<uint32>(Player::Refusal::NoPlayer));
	}
	// Every verb is refused, and nothing was asked of any world to say so.
	Player::PlayerCommand C;
	VT_CHECK(Press(V, Player::Intent::Work, 0, 1, C) == Player::Refusal::NoPlayer);
	VT_CHECK_EQ(C.Kind, uint8{0});
	// And the page says the same thing twice: taking it again is the same bytes.
	PanelView Again;
	TakePanel(Frame, Life, Told, Again);
	VT_CHECK(std::memcmp(&V, &Again, sizeof(PanelView)) == 0);
}

VAELEN_TEST(Panel, TheEmptyPageOfTheUnplayedRunIsFrozen)
{
	// AELVOR at 128/120, nobody played: the world 14.03 froze, as a page.
	const auto T0 = std::chrono::steady_clock::now();
	Aelvor A(Options{});
	VT_REQUIRE(A.Begin());
	const double Built = Ms(T0);
	WorldGen::RegionGraphCache Ways;
	ChronicleView Told;
	PanelView V;
	const auto T1 = std::chrono::steady_clock::now();
	PageOf(A, Ways, Told, V);
	const double Took = Ms(T1);

	const PanelStats S = MeasurePanel(V);
	VT_CHECK_EQ(S.NonAscii, 0u);
	VT_CHECK_EQ(S.Truncated, 0u);
	VT_CHECK(S.Rows <= PanelRows);
	VT_CHECK(S.Bytes <= 4096);
	VT_CHECK_EQ(S.Offered, 0u);
	VT_CHECK_EQ(S.Digest, Hash64{V.Digest});
	VT_CHECK_EQ(V.Person, 0u);
	VT_CHECK_EQ(V.Year, 420u); // 300 years of prehistory and 120 of the world
	VT_CHECK_EQ(Rows(V, RowKind::Chronicle), 0u);

	// The EMPTY PLAY is the same page: the Play wiring with nobody taken up -
	// what Tools/Atlas --empty --panel prints, and what CTest Atlas.PanelEmpty
	// matches against the frozen digest - shows what the unplayed world shows.
	{
		Options Played;
		Played.Play = true;
		Aelvor B(Played);
		VT_REQUIRE(B.Begin());
		WorldGen::RegionGraphCache Theirs;
		ChronicleView Nothing_;
		PanelView Empty;
		PageOf(B, Theirs, Nothing_, Empty);
		VT_CHECK_MSG(std::memcmp(&Empty, &V, sizeof(PanelView)) == 0,
					 "the empty play is the unplayed page: digest %016llx against %016llx",
					 static_cast<unsigned long long>(MeasurePanel(Empty).Digest),
					 static_cast<unsigned long long>(S.Digest));
	}

#if VAELEN_PANEL_FROZEN_EMPTY != 0x0ull
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_PANEL_FROZEN_EMPTY});
#endif
	VAELEN_LOG_INFO(LogPanel,
					"the empty page: %u rows, %u bytes of text, digest %016llx (built in %.0f ms, page in %.2f ms)\n%s",
					V.RowCount, V.Used, static_cast<unsigned long long>(S.Digest), Built, Took, Drawn(V).c_str());
}

VAELEN_TEST(Panel, ThePlayedPageOfThirtyDaysIsFrozen)
{
	// The 96-map round-robin of Run.Door, thirty days of it: the page a
	// screenshot of the first screen must show, byte for byte.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	Door D(A, Anywhere());
	const uint32 Who = D.TakeUp();
	VT_REQUIRE(Who != 0);
	for (uint32 Day = 0; Day < 30; ++Day)
	{
		Player::PlayerCommand C;
		C.Kind = static_cast<uint8>(1 + Day % 8u); // Wait, Work, Rest, Eat, Move, Speak, Give, Take, again
		C.Amount = 1 + Day % 3u;
		D.Mean(C);
		D.Day();
	}
	WorldGen::RegionGraphCache Ways;
	ChronicleView Told;
	PanelView V;
	const auto T0 = std::chrono::steady_clock::now();
	PageOf(A, Ways, Told, V);
	const double Took = Ms(T0);

	const PanelStats S = MeasurePanel(V);
	VT_CHECK_EQ(V.Person, Who);
	VT_CHECK_EQ(S.NonAscii, 0u);
	VT_CHECK_EQ(S.Truncated, 0u);
	VT_CHECK(S.Rows <= PanelRows);
	VT_CHECK(S.Bytes <= 4096);
	VT_CHECK_EQ(S.Digest, Hash64{V.Digest});
	VT_CHECK(Rows(V, RowKind::Chronicle) > 0);
	// The digest row is last, and it prints the digest.
	VT_REQUIRE(V.RowCount >= 1);
	VT_CHECK_EQ(V.Rows[V.RowCount - 1].Kind, static_cast<uint32>(RowKind::Digest));
	char Hex[32] = {};
	for (uint32 i = 0; i < 16; ++i)
	{
		Hex[i] = "0123456789abcdef"[(V.Digest >> ((15 - i) * 4)) & 0xfu];
	}
	VT_CHECK_EQ(RowAt(V, V.RowCount - 1), std::string("digest ") + Hex);

#if VAELEN_PANEL_FROZEN_PLAYED != 0x0ull
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_PANEL_FROZEN_PLAYED});
#endif
	VAELEN_LOG_INFO(
		LogPanel, "the played page: %u rows, %u bytes of text, %u verbs offered, digest %016llx (page in %.2f ms)\n%s",
		V.RowCount, V.Used, S.Offered, static_cast<unsigned long long>(S.Digest), Took, Drawn(V).c_str());
}

VAELEN_TEST(Panel, TheVerbsAreTheKernelsAndPressIsTheOtherHalf)
{
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const uint32 Who = A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0);
	// One day first: the hours of a day are the day system's, and a page taken
	// before the first tick sees none - which the page reads, rightly, as
	// every verb costing more than the day has (Costly), the same answer the
	// order system would give at that instant.
	A.Day();
	WorldGen::RegionGraphCache Ways;
	ChronicleView Told;
	PanelView V;
	WorldView Frame;
	LifeView Life;
	TakeView(A.Instance(), A.Sources(), Frame);
	TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
	TakeChronicleView(A.Instance(), A.Sources(), Told);
	TakePanel(Frame, Life, Told, V);

	// Cost, key and Offered, verb by verb, against the kernel's own rules.
	const Player::OrderRules Rules;
	uint32 Offered = 0;
	for (uint32 i = 0; i < PanelVerbs; ++i)
	{
		const VerbView& Slot = V.Verbs[i];
		VT_CHECK_EQ(Slot.Verb, i + 1);
		VT_CHECK_EQ(Slot.Cost, Rules.HoursOf[i + 1]);
		VT_CHECK_EQ(Slot.Cost, Life.Cost[i + 1]);
		VT_CHECK_EQ(static_cast<uint32>(Slot.Key), static_cast<uint32>('1' + i));
		const bool Foreseen = Slot.Foreseen != 0;
		VT_CHECK_EQ(Slot.Offered, !Foreseen && Slot.Cost <= Life.Left ? 1u : 0u);
		Offered += Slot.Offered;
		// A Speak, Give or Take is foreseen NoOne exactly when nobody is here;
		// a Move TooFar exactly when no neighbour is detailed.
		const Player::Intent Verb = static_cast<Player::Intent>(Slot.Verb);
		if (Verb == Player::Intent::Move && Life.NearCount == 0)
		{
			VT_CHECK_EQ(Slot.Foreseen, static_cast<uint32>(Player::Refusal::TooFar));
		}
		if ((Verb == Player::Intent::Speak || Verb == Player::Intent::Give || Verb == Player::Intent::Take) &&
			Life.CompanyThere == 0)
		{
			VT_CHECK_EQ(Slot.Foreseen, static_cast<uint32>(Player::Refusal::NoOne));
		}
	}
	VT_CHECK_EQ(V.Offered, Offered);
	VT_CHECK(Offered > 0);

	// Press: an offered verb becomes an intent the door takes, unstamped.
	Player::PlayerCommand C;
	VT_CHECK(Press(V, Player::Intent::Work, 0, 2, C) == Player::Refusal::None);
	VT_CHECK_EQ(C.Kind, static_cast<uint8>(Player::Intent::Work));
	VT_CHECK_EQ(C.Amount, 2u);
	VT_CHECK_EQ(C.Issued, uint64{0});
	VT_CHECK_EQ(C.Why, uint8{0});
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	// And a verb the eight do not hold is not a verb.
	VT_CHECK(Press(V, Player::Intent::None, 0, 1, C) == Player::Refusal::Unknown);
	VT_CHECK(Press(V, Player::Intent::Count, 0, 1, C) == Player::Refusal::Unknown);

	// A full queue greys every verb, with Full foreseen - and Press answers it
	// without asking the world, which the queue's own counts prove.
	const Player::PlayerOrders* Q = Player::OrdersOf(A.Instance(), A.Handles().Order);
	VT_REQUIRE(Q != nullptr);
	for (uint32 i = Q->Held; i < Player::MostOrders; ++i)
	{
		Player::PlayerCommand Fill;
		Fill.Kind = static_cast<uint8>(Player::Intent::Wait);
		Fill.Issued = A.Now();
		VT_CHECK(A.Submit(Fill) == Player::Refusal::None);
	}
	TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
	VT_REQUIRE_EQ(Life.Held, MostWaiting);
	TakePanel(Frame, Life, Told, V);
	VT_CHECK_EQ(V.Offered, 0u);
	VT_CHECK_EQ(V.Held, MostWaiting);
	for (const VerbView& Slot : V.Verbs)
	{
		VT_CHECK_EQ(Slot.Offered, 0u);
		VT_CHECK_EQ(Slot.Foreseen, static_cast<uint32>(Player::Refusal::Full));
	}
	const uint32 Refused = Q->Refused;
	const uint32 Taken = Q->Taken;
	VT_CHECK(Press(V, Player::Intent::Work, 0, 1, C) == Player::Refusal::Full);
	VT_CHECK_EQ(Q->Refused, Refused); // the page refused it; the world never saw it
	VT_CHECK_EQ(Q->Taken, Taken);
	// The page's own words for it.
	for (uint32 i = 0; i < V.RowCount; ++i)
	{
		if (V.Rows[i].Kind == static_cast<uint32>(RowKind::Verb))
		{
			VT_CHECK_MSG(RowAt(V, i).find(Player::RefusalName(Player::Refusal::Full)) != std::string::npos, "%s",
						 RowAt(V, i).c_str());
		}
	}
}

VAELEN_TEST(Panel, LinesWriteWhatTheWidgetDraws)
{
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	VT_REQUIRE(A.TakeUp(Anywhere()) != 0);
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Work);
	C.Amount = 1;
	C.Issued = A.Now();
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	A.Day();
	WorldGen::RegionGraphCache Ways;
	ChronicleView Told;
	PanelView V;
	PageOf(A, Ways, Told, V);

	// Every row is printable ASCII, terminated where its Length says, and
	// packed one after another with nothing between.
	VT_REQUIRE(V.RowCount > 0);
	for (uint32 i = 0; i < V.RowCount; ++i)
	{
		const RowView& R = V.Rows[i];
		VT_CHECK(R.Begin + R.Length < PanelTextBytes);
		VT_CHECK(V.Text[R.Begin + R.Length] == '\0');
		VT_CHECK(R.Length > 0);
		VT_CHECK(R.Kind > 0 && R.Kind < static_cast<uint32>(RowKind::Count));
		if (i > 0)
		{
			VT_CHECK_EQ(V.Rows[i - 1].Begin + V.Rows[i - 1].Length + 1, R.Begin);
		}
		for (uint32 n = 0; n < R.Length; ++n)
		{
			const unsigned char c = static_cast<unsigned char>(V.Text[R.Begin + n]);
			VT_REQUIRE(c >= 0x20 && c <= 0x7e);
		}
	}
	for (uint32 i = V.Used; i < PanelTextBytes; ++i)
	{
		VT_REQUIRE(V.Text[i] == '\0');
	}

	// Lines is those rows, joined by newlines, and nothing else.
	const std::string Page = Drawn(V);
	VT_CHECK_EQ(static_cast<uint32>(Page.size()), V.Used);
	usize At = 0;
	for (uint32 i = 0; i < V.RowCount; ++i)
	{
		const std::string Row = RowAt(V, i);
		VT_CHECK_EQ(Page.compare(At, Row.size(), Row), 0);
		At += Row.size();
		VT_CHECK(At < Page.size() && Page[At] == '\n');
		++At;
	}
	VT_CHECK_EQ(At, Page.size());
	VT_CHECK_EQ(static_cast<uint32>(std::count(Page.begin(), Page.end(), '\n')), V.RowCount);

	// A buffer too small writes a terminator and nothing else; a buffer of
	// nothing writes nothing.
	char Tiny[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
	VT_CHECK_EQ(Lines(V, Tiny, 8), 0u);
	VT_CHECK(Tiny[0] == '\0');
	VT_CHECK_EQ(Lines(V, nullptr, 64), 0u);
	VT_CHECK_EQ(Lines(V, Tiny, 0), 0u);

	// The digest row is the page's own, of every byte before it: changing one
	// byte of the page moves it.
	PanelView Moved = V;
	VT_REQUIRE(Moved.Rows[0].Length > 0);
	Moved.Text[Moved.Rows[0].Begin] = Moved.Text[Moved.Rows[0].Begin] == 'A' ? 'B' : 'A';
	VT_CHECK(MeasurePanel(Moved).Digest != MeasurePanel(V).Digest);
	VT_CHECK_EQ(MeasurePanel(Moved).Digest != Hash64{Moved.Digest},
				true); // the field now lies, and the measure says so
}

VAELEN_TEST(Panel, ThePageOutlivesTheWorld)
{
	PanelView V;
	Hash64 Before = 0;
	{
		std::unique_ptr<Aelvor> A = std::make_unique<Aelvor>(Small());
		VT_REQUIRE(A->Begin());
		VT_REQUIRE(A->TakeUp(Anywhere()) != 0);
		A->Day();
		WorldGen::RegionGraphCache Ways;
		ChronicleView Told;
		PageOf(*A, Ways, Told, V);
		Before = MeasurePanel(V).Digest;
		VT_REQUIRE(V.Person != 0);
	} // the world is gone here
	VT_CHECK_EQ(MeasurePanel(V).Digest, Before);
	VT_CHECK_EQ(MeasurePanel(V).Digest, Hash64{V.Digest});
	VT_CHECK(Drawn(V).size() == V.Used);
	Player::PlayerCommand C;
	VT_CHECK(Press(V, Player::Intent::Wait, 0, 1, C) == Player::Refusal::None);
}
