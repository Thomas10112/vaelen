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
#include "Vaelen/Sim/PreHistory.h"
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
#define VAELEN_PANEL_FROZEN_EMPTY 0xd7e7149e65ceb69aull
#define VAELEN_PANEL_FROZEN_PLAYED 0x777351768a3a4fc6ull

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

	/// Puts a name into a view's fixed bytes, the way the kernel's own Put does.
	void Named(char (&To)[LifeNameBytes], const char* From)
	{
		usize n = 0;
		for (; From[n] != '\0' && n + 1 < LifeNameBytes; ++n)
		{
			To[n] = From[n];
		}
		To[n] = '\0';
	}

	/// The row of a kind, or "" when the page wrote none.
	std::string RowOf(const PanelView& V, RowKind Kind, uint32 Which = 0)
	{
		uint32 Seen = 0;
		for (uint32 i = 0; i < V.RowCount && i < PanelRows; ++i)
		{
			if (V.Rows[i].Kind == static_cast<uint32>(Kind) && Seen++ == Which)
			{
				return RowAt(V, i);
			}
		}
		return std::string();
	}

	/// The row of one verb.
	std::string VerbRow(const PanelView& V, Player::Intent Verb)
	{
		for (uint32 i = 0; i < V.RowCount && i < PanelRows; ++i)
		{
			if (V.Rows[i].Kind == static_cast<uint32>(RowKind::Verb) && V.Rows[i].Verb == static_cast<uint32>(Verb))
			{
				return RowAt(V, i);
			}
		}
		return std::string();
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
		// The keys 14.09 binds, in Intent order, read from the page itself.
		VT_CHECK_EQ(static_cast<uint32>(Slot.Key), static_cast<uint32>("TWREMSGK"[i]));
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
	// The proof that the world was never asked is the queue's own count of what
	// is IN it: a Submit that reached the world would add one (Player::Submit
	// answers Full itself at 8, so the count is the honest witness either way,
	// and Refused/Taken move only when the order system ticks - they would not
	// have moved for a real Submit, which is why they prove nothing).
	const uint32 Held = Q->Held;
	const uint32 Refused = Q->Refused;
	const uint32 Taken = Q->Taken;
	VT_CHECK(Press(V, Player::Intent::Work, 0, 1, C) == Player::Refusal::Full);
	VT_CHECK_EQ(Q->Held, Held); // the page refused it; nothing was queued
	VT_CHECK_EQ(Q->Refused, Refused);
	VT_CHECK_EQ(Q->Taken, Taken);
	// And a Press on a page nobody composed is not a verb at all.
	PanelView Never;
	VT_CHECK(Press(Never, Player::Intent::Work, 0, 1, C) == Player::Refusal::Unknown);
	VT_CHECK(Press(Never, Player::Intent::None, 0, 1, C) == Player::Refusal::Unknown);
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

	// All of the page or none of it: a buffer one byte short writes nothing,
	// a buffer of exactly the page writes the page.
	char Tiny[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
	VT_CHECK_EQ(Lines(V, Tiny, 8), 0u);
	VT_CHECK(Tiny[0] == '\0');
	VT_CHECK_EQ(Lines(V, nullptr, 64), 0u);
	VT_CHECK_EQ(Lines(V, Tiny, 0), 0u);
	std::string Exact(V.Used + 1, 'x');
	VT_CHECK_EQ(Lines(V, Exact.data(), V.Used + 1), V.Used);
	VT_CHECK_EQ(std::string(Exact.c_str()), Page);
	std::string Short(V.Used, 'x');
	VT_CHECK_MSG(Lines(V, Short.data(), V.Used) == 0u, "one byte short is no page at all");
	VT_CHECK(Short[0] == '\0');

	// The digest row is the page's own, of every byte before it: changing one
	// byte of the page moves it.
	PanelView Moved = V;
	VT_REQUIRE(Moved.Rows[0].Length > 0);
	Moved.Text[Moved.Rows[0].Begin] = Moved.Text[Moved.Rows[0].Begin] == 'A' ? 'B' : 'A';
	VT_CHECK(MeasurePanel(Moved).Digest != MeasurePanel(V).Digest);
	VT_CHECK_EQ(MeasurePanel(Moved).Digest != Hash64{Moved.Digest},
				true); // the field now lies, and the measure says so
}

VAELEN_TEST(Panel, EveryRowSaysWhatTheViewsSay)
{
	// No world at all: the three views are written here, field by field, so
	// that every row the page can write IS written and read back word for
	// word - including the ones a played 96-map never reaches (bound to,
	// last:, the neighbours, the company that does not fit, a verb today
	// cannot pay for). If a row ever takes its number from the wrong field,
	// this is where it shows, and not only as a moved digest.
	WorldView Frame;
	Frame.People = 4096;
	Frame.Regions.resize(3);
	Frame.Regions[0].Index = 12;
	Frame.Regions[1].Index = 27;
	Frame.Regions[1].People = 412;
	Frame.Regions[2].Index = 31;
	Frame.Regions[2].People = 88;

	LifeView Life;
	Life.Tick = 301 * History::TicksPerYear + 11 * 24;
	Life.Year = 301;
	Life.Day = 11;
	Life.Person = 7;
	Life.Region = 12;
	Life.Alive = 1;
	Life.Years = 25;
	Named(Life.Name, "Eikha");
	Named(Life.RegionName, "Iakhas");
	Life.StartRegion = 12;
	Life.Holder = 9;
	Life.StartYear = 275;
	Life.Bond = 2;
	Named(Life.HolderName, "Ordihumen");
	Life.Awake = 16;
	Life.Spent = 14;
	Life.Left = 2;
	Life.Food = 243;
	Life.Health = 255;
	Life.Rest = 185;
	Life.Held = 1;
	Life.Taken = 3;
	Life.Refused = 2;
	Life.LastRefusal = static_cast<uint32>(Player::Refusal::TooFar);
	Life.Cost[static_cast<uint32>(Player::Intent::Wait)] = 1;
	Life.Cost[static_cast<uint32>(Player::Intent::Work)] = 4;
	Life.Cost[static_cast<uint32>(Player::Intent::Rest)] = 2;
	Life.Cost[static_cast<uint32>(Player::Intent::Eat)] = 1;
	Life.Cost[static_cast<uint32>(Player::Intent::Move)] = 3;
	Life.Cost[static_cast<uint32>(Player::Intent::Speak)] = 1;
	Life.Cost[static_cast<uint32>(Player::Intent::Give)] = 1;
	Life.Cost[static_cast<uint32>(Player::Intent::Take)] = 1;
	Life.Near[0] = 27;
	Life.Near[1] = 31;
	Life.NearCount = 2;
	const char* Here[] = {"Ukit", "Aifus", "Inik", "Aiguk", "Uwum", "Gimfut", "Nobody"};
	for (uint32 i = 0; i < 7; ++i)
	{
		Life.Company[i].Person = 100 + i;
		Named(Life.Company[i].Name, Here[i]);
	}
	Life.CompanyCount = 7;
	Life.CompanyThere = 858;

	ChronicleView Told;
	Told.Person = 7;
	Told.LineCount = 1;
	Told.Lines[0].Kind = static_cast<uint32>(LineKind::Refused);
	Told.Lines[0].Verb = static_cast<uint32>(Player::Intent::Move);
	Told.Lines[0].Refused = static_cast<uint32>(Player::Refusal::TooFar);
	const std::string Said = "Year 301: Eikha could not walked: too far to walk.";
	std::memcpy(Told.Text, Said.data(), Said.size());
	Told.Lines[0].Length = static_cast<uint32>(Said.size());
	Told.Used = static_cast<uint32>(Said.size()) + 1;

	PanelView V;
	TakePanel(Frame, Life, Told, V);

	// Every row, word for word.
	VT_CHECK_EQ(RowOf(V, RowKind::Date), std::string("AELVOR  year 301  day 12  4096 alive in 3 regions"));
	VT_CHECK_EQ(RowOf(V, RowKind::Self, 0), std::string("Eikha of Iakhas, 25"));
	VT_CHECK_EQ(RowOf(V, RowKind::Self, 1), std::string("bound to Ordihumen since year 275"));
	VT_CHECK_EQ(RowOf(V, RowKind::Body), std::string("food 243  health 255  rest 185"));
	VT_CHECK_EQ(RowOf(V, RowKind::Hours, 0), std::string("hours left 2 of 16"));
	VT_CHECK_EQ(RowOf(V, RowKind::Hours, 1), std::string("queue 1 held, 3 taken, 2 refused"));
	VT_CHECK_EQ(RowOf(V, RowKind::Hours, 2), std::string("last: ") + Player::RefusalName(Player::Refusal::TooFar));
	VT_CHECK_EQ(RowOf(V, RowKind::Near), std::string("near: 27(412) 31(88)"));
	VT_CHECK_EQ(RowOf(V, RowKind::Company), std::string("here: Ukit, Aifus, Inik, Aiguk, Uwum, Gimfut and 852 more"));
	VT_CHECK_EQ(RowOf(V, RowKind::Chronicle), std::string("  ") + Said);

	// The verbs: what the day can pay for, what it cannot, and what nobody is
	// there for. Move is offered because a neighbour is near and it fits;
	// Work costs more hours than are LEFT, which is not a refusal but not an
	// offer either; Speak, Give and Take have company.
	VT_CHECK_EQ(VerbRow(V, Player::Intent::Wait), std::string("[T] wait      1h  ok"));
	VT_CHECK_EQ(VerbRow(V, Player::Intent::Work), std::string("[W] work      4h  -  not today"));
	VT_CHECK_EQ(VerbRow(V, Player::Intent::Rest), std::string("[R] rest      2h  ok"));
	VT_CHECK_EQ(VerbRow(V, Player::Intent::Move), std::string("[M] move      3h  -  not today"));
	VT_CHECK_EQ(VerbRow(V, Player::Intent::Speak), std::string("[S] speak     1h  ok"));
	VT_CHECK_EQ(V.Offered, 6u); // everything two hours can pay for: wait, rest, eat, speak, give, take
	Player::PlayerCommand C;
	VT_CHECK(Press(V, Player::Intent::Work, 0, 1, C) == Player::Refusal::Costly);
	VT_CHECK_EQ(C.Kind, uint8{0}); // nothing was filled
	VT_CHECK(Press(V, Player::Intent::Speak, 100, 1, C) == Player::Refusal::None);
	VT_CHECK_EQ(C.Target, 100u);

	// Nobody near and nobody here: the two rows say so, and the three verbs
	// that need somebody are foreseen NoOne, the Move TooFar.
	LifeView Alone = Life;
	Alone.NearCount = 0;
	Alone.CompanyCount = 0;
	Alone.CompanyThere = 0;
	Alone.Left = 16;
	PanelView W;
	TakePanel(Frame, Alone, Told, W);
	VT_CHECK_EQ(RowOf(W, RowKind::Near), std::string("near: nowhere a walk reaches"));
	VT_CHECK_EQ(RowOf(W, RowKind::Company), std::string("here: nobody"));
	VT_CHECK_EQ(VerbRow(W, Player::Intent::Move), std::string("[M] move      3h  -  too far to walk"));
	VT_CHECK_EQ(VerbRow(W, Player::Intent::Give), std::string("[G] give      1h  -  nobody there"));
	VT_CHECK_EQ(VerbRow(W, Player::Intent::Work), std::string("[W] work      4h  ok"));
	VT_CHECK(Press(W, Player::Intent::Give, 1, 1, C) == Player::Refusal::NoOne);

	// A free person has no "bound to" row; a dead one says so and is offered
	// nothing; a whole day that cannot pay for a verb is Costly, not "not
	// today", and the page says the kernel's word for it.
	LifeView Free = Life;
	Free.Bond = 0;
	PanelView F;
	TakePanel(Frame, Free, Told, F);
	VT_CHECK_EQ(RowOf(F, RowKind::Self, 1), std::string());
	VT_CHECK_EQ(Rows(F, RowKind::Self), 1u);
	LifeView Gone = Life;
	Gone.Alive = 0;
	PanelView G;
	TakePanel(Frame, Gone, Told, G);
	VT_CHECK_EQ(RowOf(G, RowKind::Self, 0), std::string("Eikha of Iakhas, 25, dead"));
	VT_CHECK_EQ(G.Offered, 0u);
	VT_CHECK_EQ(VerbRow(G, Player::Intent::Wait), std::string("[T] wait      1h  -  the person is dead"));
	LifeView Tired = Life;
	Tired.Awake = 2;
	Tired.Left = 2;
	PanelView T;
	TakePanel(Frame, Tired, Told, T);
	VT_CHECK_EQ(VerbRow(T, Player::Intent::Work), std::string("[W] work      4h  -  longer than a day"));
	VT_CHECK(Press(T, Player::Intent::Work, 0, 1, C) == Player::Refusal::Costly);

	// And the page is ASCII, fits, and its digest row is its own.
	for (const PanelView* Page : {&V, &W, &F, &G, &T})
	{
		const PanelStats S = MeasurePanel(*Page);
		VT_CHECK_EQ(S.NonAscii, 0u);
		VT_CHECK_EQ(S.Truncated, 0u);
		VT_CHECK_EQ(S.Dropped, 0u);
		VT_CHECK(S.Rows <= PanelRows);
		VT_CHECK_EQ(S.Digest, Hash64{Page->Digest});
	}
	VAELEN_LOG_INFO(LogPanel, "a page written from three views by hand: %u rows, %u bytes\n%s", V.RowCount, V.Used,
					Drawn(V).c_str());
}

VAELEN_TEST(Panel, APageTooLongIsCutAndCounted)
{
	// Nothing the kernel writes is this long, which is why the cut path runs
	// under no other test: sixteen chronicle lines of two hundred letters
	// each is more than the page holds. What must hold anyway: the rows that
	// fit are whole, one is cut and counted, no row runs past the buffer, and
	// the DIGEST row is still written - the page keeps the one row a
	// screenshot is checked by.
	WorldView Frame;
	Frame.People = 1;
	LifeView Life;
	Life.Person = 3;
	Life.Alive = 1;
	Named(Life.Name, "Eikha");
	Named(Life.RegionName, "Iakhas");
	Life.Awake = 16;
	Life.Left = 16;
	ChronicleView Told;
	Told.Person = 3;
	const std::string Long(200, 'a');
	for (uint32 i = 0; i < PanelChronicle; ++i)
	{
		Told.Lines[i].Kind = static_cast<uint32>(LineKind::Acted);
		Told.Lines[i].Begin = Told.Used;
		Told.Lines[i].Length = static_cast<uint32>(Long.size());
		std::memcpy(Told.Text + Told.Used, Long.data(), Long.size());
		Told.Used += static_cast<uint32>(Long.size()) + 1;
	}
	Told.LineCount = PanelChronicle;

	PanelView V;
	TakePanel(Frame, Life, Told, V);
	const PanelStats S = MeasurePanel(V);
	VT_CHECK_MSG(S.Dropped >= 1, "a page of %u rows and %u bytes dropped %u", S.Rows, S.TextBytes, S.Dropped);
	VT_CHECK_EQ(S.Truncated, 0u); // these rows did not fit at all; none was begun and cut
	VT_CHECK(V.Used <= PanelTextBytes);
	VT_CHECK_EQ(S.NonAscii, 0u);
	VT_CHECK(S.Rows <= PanelRows);
	VT_REQUIRE(V.RowCount >= 1);
	VT_CHECK_EQ(V.Rows[V.RowCount - 1].Kind, static_cast<uint32>(RowKind::Digest));
	VT_CHECK_EQ(S.Digest, Hash64{V.Digest});
	for (uint32 i = 0; i < V.RowCount; ++i)
	{
		VT_REQUIRE(V.Rows[i].Begin + V.Rows[i].Length < PanelTextBytes);
		VT_REQUIRE(V.Text[V.Rows[i].Begin + V.Rows[i].Length] == '\0');
	}
	VT_CHECK_EQ(static_cast<uint32>(Drawn(V).size()), V.Used);
	VAELEN_LOG_INFO(LogPanel, "a page dropped: %u rows, %u bytes, %u dropped, %u truncated, digest %016llx", S.Rows,
					S.TextBytes, S.Dropped, S.Truncated, static_cast<unsigned long long>(S.Digest));

	// And one line longer than the whole page is CUT rather than dropped: it
	// begins, runs out, and is counted - the page keeps its digest row either
	// way, which is the row a screenshot is checked by.
	ChronicleView Huge;
	Huge.Person = 3;
	const std::string Endless(3000, 'b');
	Huge.LineCount = 1;
	Huge.Lines[0].Kind = static_cast<uint32>(LineKind::Acted);
	Huge.Lines[0].Length = static_cast<uint32>(Endless.size());
	std::memcpy(Huge.Text, Endless.data(), Endless.size());
	Huge.Used = static_cast<uint32>(Endless.size()) + 1;
	PanelView Cut;
	TakePanel(Frame, Life, Huge, Cut);
	const PanelStats CS = MeasurePanel(Cut);
	VT_CHECK_MSG(CS.Truncated >= 1, "one line of %zu letters is cut: %u truncated, %u dropped", Endless.size(),
				 CS.Truncated, CS.Dropped);
	VT_CHECK(Cut.Used <= PanelTextBytes);
	VT_CHECK_EQ(CS.NonAscii, 0u);
	VT_REQUIRE(Cut.RowCount >= 1);
	VT_CHECK_EQ(Cut.Rows[Cut.RowCount - 1].Kind, static_cast<uint32>(RowKind::Digest));
	VT_CHECK_EQ(CS.Digest, Hash64{Cut.Digest});
	VT_CHECK_EQ(static_cast<uint32>(Drawn(Cut).size()), Cut.Used);
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

VAELEN_TEST(Panel, TheWeatherRowIsComposedFromTheLeavesAndSaysTheSign)
{
	// 18.04: a hand-built life and frame, so the exact row is pinned without
	// a world: winter, day 274 of the year (day 5 of the season), -12.3
	// degrees, a chill of 40, the region's word saying -21 .. 14 and a 54%
	// outlook with a hard winter - which LIES on the land, since day 274 is
	// winter's fifth; "coming" is what the row says in autumn, below.
	WorldView Frame;
	Frame.Year = 12;
	RegionView R;
	R.Index = 3;
	R.Climate = PackRegionClimate(-12, -21, 14, 54, true);
	Frame.Regions.push_back(R);
	LifeView Life;
	Life.Year = 12;
	Life.Day = 274;
	Life.Person = 7;
	Life.Region = 3;
	Life.Alive = 1;
	Life.Season = 4;
	Life.Degrees = -123;
	Life.Chill = 40;
	Life.Winter = 2;
	Named(Life.Name, "Someone");
	Named(Life.RegionName, "Somewhere");
	ChronicleView Told;
	PanelView Page;
	TakePanel(Frame, Life, Told, Page);

	VT_REQUIRE(Page.RowCount > 2u);
	VT_CHECK_EQ(Page.Rows[1].Kind, static_cast<uint32>(RowKind::Weather));
	const std::string Row = RowAt(Page, 1);
	VT_CHECK_MSG(Row == "winter  day 5 of 90  -12.3 deg here  chill 40  coldest -21  warmest 14  outlook 54%  a hard "
						"winter lies on the land",
				 "the weather row reads: %s", Row.c_str());
	VT_CHECK_MSG(RowAt(Page, 3).find("  chill 40") != std::string::npos, "the body row reads: %s",
				 RowAt(Page, 3).c_str());
	VT_CHECK(Page.Rows[Page.RowCount - 1u].Kind == static_cast<uint32>(RowKind::Digest));
	VT_CHECK(sizeof(PanelView) <= 4096u);

	// The signed writers, through the row: -0.5, 0.0, 123.4; and in autumn
	// the same hard winter is COMING, day 21 of that season.
	Life.Degrees = -5;
	Life.Day = 200;
	Life.Season = 3;
	TakePanel(Frame, Life, Told, Page);
	VT_CHECK_MSG(RowAt(Page, 1).find("  -0.5 deg here") != std::string::npos, "%s", RowAt(Page, 1).c_str());
	VT_CHECK_MSG(RowAt(Page, 1).find("a hard winter is coming") != std::string::npos, "%s", RowAt(Page, 1).c_str());
	VT_CHECK_MSG(RowAt(Page, 1).find("autumn  day 21 of 90") != std::string::npos, "%s", RowAt(Page, 1).c_str());
	Life.Season = 4;
	Life.Day = 300;
	Life.Degrees = 0;
	Life.Winter = 1;
	TakePanel(Frame, Life, Told, Page);
	VT_CHECK_MSG(RowAt(Page, 1).find("  0.0 deg here") != std::string::npos, "%s", RowAt(Page, 1).c_str());
	VT_CHECK_MSG(RowAt(Page, 1).find("hard winter") == std::string::npos, "%s", RowAt(Page, 1).c_str());
	Life.Degrees = 1234;
	TakePanel(Frame, Life, Told, Page);
	VT_CHECK_MSG(RowAt(Page, 1).find("  123.4 deg here") != std::string::npos, "%s", RowAt(Page, 1).c_str());
	// Nobody played: the season and the day, and no "here".
	Life.Person = 0;
	Life.Region = 0;
	Life.Winter = 0;
	TakePanel(Frame, Life, Told, Page);
	VT_CHECK_MSG(RowAt(Page, 1) == "winter  day 31 of 90", "%s", RowAt(Page, 1).c_str());
	// And no climate: no weather row at all, the second row is the self.
	Life.Season = 0;
	TakePanel(Frame, Life, Told, Page);
	VT_CHECK_EQ(Page.Rows[1].Kind, static_cast<uint32>(RowKind::Self));
	VT_CHECK(Rows(Page, RowKind::Weather) == 0u);
}
