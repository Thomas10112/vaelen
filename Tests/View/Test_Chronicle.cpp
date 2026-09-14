// VAELEN - Tests/View
// Phase 14 task 14.05: the last lines of one played life, as the screen that
// shows them needs them.
//
// What is held here: the view is flat; nobody played is an empty chronicle;
// every line is ExportLife's to the byte and a view grown day by day over a
// year of the eight verbs equals a fresh one taken at the year's end, byte for
// byte; a refused Eat with no grain is a line saying "nothing to do it with"
// with the refusal's code beside it; the view outlives the world.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/SocietyHistory.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;
using namespace Vaelen::View;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogChronicle);

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

	/// ExportLife as Run::Aelvor::Life() words it, every act (MaxActs = 0),
	/// split into its lines.
	void WholeLife(const Aelvor& A, std::vector<std::string>& Out)
	{
		const Wired& T = A.Handles();
		Society::SocietyContext Society{T.Persons, T.Families, T.Organizations};
		Economy::EconomyContext Goods{T.Persons, T.Families, T.Trade, T.Markets, Economy::MarketRules{}, &Society};
		Player::LifeContext Life;
		Life.Persons = T.Persons;
		Life.Families = T.Families;
		Life.Player = T.Played;
		Life.Regard = T.Regard;
		Life.Goods = &Goods;
		std::string Text;
		Player::ExportLife(A.Instance(), A.Ages(), Life, Text, 0);
		Out.clear();
		usize At = 0;
		while (At < Text.size())
		{
			usize End = Text.find('\n', At);
			if (End == std::string::npos)
			{
				End = Text.size();
			}
			Out.push_back(Text.substr(At, End - At));
			At = End + 1;
		}
	}

	/// The lines of ExportLife that are the timeline: after the first (who they
	/// are), before the regard block and the why (each of which is indented or
	/// is "why:").
	void TimelineOf(const std::vector<std::string>& Life, std::vector<std::string>& Out, std::vector<std::string>& Why)
	{
		Out.clear();
		Why.clear();
		bool InWhy = false;
		for (usize i = 1; i < Life.size(); ++i)
		{
			const std::string& L = Life[i];
			if (InWhy)
			{
				Why.push_back(L);
				continue;
			}
			if (L == "why:")
			{
				InWhy = true;
				continue;
			}
			if (L.size() >= 2 && L[0] == ' ' && L[1] == ' ')
			{
				continue; // the regard block
			}
			Out.push_back(L);
		}
	}

	std::string LineAt(const ChronicleView& V, uint32 i)
	{
		return std::string(V.Text + V.Lines[i].Begin, V.Lines[i].Length);
	}

	bool Ascii(const char* S, uint32 N)
	{
		for (uint32 i = 0; i < N; ++i)
		{
			const unsigned char c = static_cast<unsigned char>(S[i]);
			if (c != 0 && (c < 0x20 || c > 0x7e)) // the NUL between lines is the buffer's, not a line's
			{
				return false;
			}
		}
		return true;
	}
} // namespace

VAELEN_TEST(Chronicle, TheViewIsFlatAndItsPaddingIsNamed)
{
	VT_CHECK(std::is_trivially_copyable<ChronicleView>::value);
	VT_CHECK(std::is_standard_layout<ChronicleView>::value);
	VT_CHECK(std::is_trivially_copyable<LineView>::value);
	VT_CHECK_EQ(sizeof(LineView), usize{32});
	VT_CHECK_EQ(sizeof(ChronicleView), usize{7800});
	VT_CHECK(sizeof(ChronicleView) <= 8192);
	ChronicleView A;
	ChronicleView B;
	VT_CHECK_EQ(MeasureChronicleView(A).Digest, MeasureChronicleView(B).Digest);
	VT_CHECK_EQ(MeasureChronicleView(A).Bytes, static_cast<uint32>(sizeof(ChronicleView)));
	VT_CHECK_EQ(MeasureChronicleView(A).Lines, 0u);
}

VAELEN_TEST(Chronicle, NobodyPlayedIsAnEmptyChronicle)
{
	// No Play at all, then Play with nobody taken up: both an empty view with
	// the clock on it and nothing read.
	Options O = Small();
	O.Play = false;
	Aelvor A(O);
	VT_REQUIRE(A.Begin());
	ChronicleView V;
	VT_CHECK_EQ(TakeChronicleView(A.Instance(), A.Sources(), V), 0u);
	VT_CHECK_EQ(V.Person, 0u);
	VT_CHECK_EQ(V.LineCount, 0u);
	VT_CHECK_EQ(V.Since, uint64{0});
	VT_CHECK_EQ(V.Tick, static_cast<uint64>(A.Now()));

	Aelvor B(Small());
	VT_REQUIRE(B.Begin());
	VT_CHECK(B.Sources().HasLife);
	VT_CHECK(B.Sources().HasGoods);
	VT_CHECK_EQ(TakeChronicleView(B.Instance(), B.Sources(), V), 0u);
	VT_CHECK_EQ(V.Person, 0u);
	VT_CHECK_EQ(V.LineCount, 0u);
	VT_CHECK_EQ(V.WhyCount, 0u);
	ChronicleView Empty;
	Empty.Tick = V.Tick;
	Empty.Year = V.Year;
	VT_CHECK(std::memcmp(&V, &Empty, sizeof(ChronicleView)) == 0);
}

VAELEN_TEST(Chronicle, EveryLineIsExportLifesAndAYearGrownEqualsAYearFresh)
{
	// The 96-map round-robin of Run.Door: a year of the eight verbs, one a
	// day, the view taken after every day (grown), then once from nothing
	// (fresh). The two are the same bytes; every line held is a line of
	// ExportLife; the why lines are its why lines.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	Door D(A, Anywhere());
	const uint32 Who = D.TakeUp();
	VT_REQUIRE(Who != 0);
	ChronicleView Grown;
	uint32 ReadMin = ~0u;
	uint32 ReadMax = 0;
	uint64 ReadAll = 0;
	uint32 FirstDayFull = 0;
	constexpr uint32 Days = 360;
	for (uint32 Day = 0; Day < Days; ++Day)
	{
		Player::PlayerCommand C;
		C.Kind = static_cast<uint8>(1 + Day % 8u); // Wait, Work, Rest, Eat, Move, Speak, Give, Take, again
		C.Amount = 1 + Day % 3u;
		D.Mean(C);
		D.Day();
		const uint32 Read = TakeChronicleView(A.Instance(), A.Sources(), Grown);
		ReadMin = Read < ReadMin ? Read : ReadMin;
		ReadMax = Read > ReadMax ? Read : ReadMax;
		ReadAll += Read;
		if (FirstDayFull == 0 && Grown.LineCount == ChronicleLines)
		{
			FirstDayFull = Day + 1;
		}
		VT_CHECK_EQ(Grown.Person, Who);
		VT_CHECK_EQ(Grown.Since, A.Instance().Log().Count());
	}
	VT_CHECK_MSG(Grown.LineCount == ChronicleLines, "a year of verbs is more than %u lines: %u held", ChronicleLines,
				 Grown.LineCount);
	VT_CHECK(Grown.Dropped > 0);
	VT_CHECK_EQ(Grown.Truncated, 0u);

	ChronicleView Fresh;
	const auto T0 = std::chrono::steady_clock::now();
	const uint32 ReadFresh = TakeChronicleView(A.Instance(), A.Sources(), Fresh);
	const double FreshMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	VT_CHECK_EQ(ReadFresh, static_cast<uint32>(A.Instance().Log().Count()));
	VT_CHECK_MSG(std::memcmp(&Grown, &Fresh, sizeof(ChronicleView)) == 0,
				 "a view grown over %u days is the fresh one, byte for byte", Days);
	VT_CHECK_EQ(MeasureChronicleView(Grown).Digest, MeasureChronicleView(Fresh).Digest);

	// The measure.
	const ChronicleStats S = MeasureChronicleView(Grown);
	VT_CHECK_EQ(S.Lines, ChronicleLines);
	VT_CHECK_EQ(S.NonAscii, 0u);
	VT_CHECK_EQ(S.Truncated, 0u);
	VT_CHECK(S.Bytes <= 8192);
	VT_CHECK_EQ(S.EventsRead, static_cast<uint32>(Grown.Since));
	VT_CHECK(Ascii(Grown.Text, Grown.Used > 0 ? Grown.Used - 1 : 0) || Grown.Used == 0);

	// Every byte past what is used is zero, and every line is terminated
	// where its Length says.
	for (uint32 i = Grown.Used; i < ChronicleTextBytes; ++i)
	{
		VT_REQUIRE(Grown.Text[i] == '\0');
	}
	for (uint32 i = 0; i < Grown.LineCount; ++i)
	{
		const LineView& L = Grown.Lines[i];
		VT_CHECK(L.Begin + L.Length < ChronicleTextBytes);
		VT_CHECK(Grown.Text[L.Begin + L.Length] == '\0');
		VT_CHECK(L.Length > 0);
		VT_CHECK(L.Kind > static_cast<uint32>(LineKind::None) && L.Kind < static_cast<uint32>(LineKind::Count));
		VT_CHECK_EQ(L.Year, static_cast<uint32>(L.Tick / History::TicksPerYear));
		if (i > 0)
		{
			VT_CHECK(Grown.Lines[i - 1].Tick <= L.Tick);
			VT_CHECK_EQ(Grown.Lines[i - 1].Begin + Grown.Lines[i - 1].Length + 1, L.Begin);
		}
		const std::string Text = LineAt(Grown, i);
		if (L.Kind == static_cast<uint32>(LineKind::Refused))
		{
			VT_CHECK_MSG(Text.find("could not") != std::string::npos, "%s", Text.c_str());
			VT_CHECK_MSG(Text.find(Player::RefusalName(static_cast<Player::Refusal>(L.Refused))) != std::string::npos,
						 "a refusal carries its name: %s", Text.c_str());
			VT_CHECK(L.Verb != 0);
		}
		else
		{
			VT_CHECK(Text.find("could not") == std::string::npos);
			VT_CHECK_EQ(L.Refused, 0u);
			VT_CHECK((L.Verb != 0) == (L.Kind == static_cast<uint32>(LineKind::Acted)));
		}
	}

	// Word for word ExportLife's: the last LineCount lines of its timeline,
	// and the first WhyLines of its why.
	std::vector<std::string> Life;
	std::vector<std::string> Timeline;
	std::vector<std::string> Why;
	WholeLife(A, Life);
	TimelineOf(Life, Timeline, Why);
	VT_REQUIRE(Timeline.size() >= Grown.LineCount);
	VT_CHECK_EQ(static_cast<uint32>(Timeline.size()), Grown.LineCount + Grown.Dropped);
	const usize Skip = Timeline.size() - Grown.LineCount;
	for (uint32 i = 0; i < Grown.LineCount; ++i)
	{
		const std::string Mine = LineAt(Grown, i);
		VT_CHECK_MSG(Mine == Timeline[Skip + i], "line %u: view '%s' vs life '%s'", i, Mine.c_str(),
					 Timeline[Skip + i].c_str());
	}
	VT_CHECK_MSG(Grown.WhyOf != 0, "a year of giving and walking is followed by something");
	VT_CHECK(Grown.WhyCount >= 1 && Grown.WhyCount <= WhyLines);
	VT_CHECK(Why.size() >= Grown.WhyCount);
	for (uint32 i = 0; i < Grown.WhyCount; ++i)
	{
		const std::string Mine(Grown.WhyText + Grown.Why[i].Begin, Grown.Why[i].Length);
		VT_CHECK_MSG(i < Why.size() && Mine == Why[i], "why %u: view '%s' vs life '%s'", i, Mine.c_str(),
					 i < Why.size() ? Why[i].c_str() : "-");
		VT_CHECK(Grown.WhyText[Grown.Why[i].Begin + Grown.Why[i].Length] == '\0');
	}
	if (Grown.WhyCount == 2)
	{
		VT_CHECK(std::string(Grown.WhyText + Grown.Why[1].Begin, Grown.Why[1].Length).rfind("  because ", 0) == 0);
	}

	// Sixty takes with nothing happening read nothing and move nothing.
	const Hash64 State = A.StateDigest();
	const Hash64 Log = A.LogDigest();
	const Hash64 Before = MeasureChronicleView(Grown).Digest;
	for (uint32 i = 0; i < 60; ++i)
	{
		VT_CHECK_EQ(TakeChronicleView(A.Instance(), A.Sources(), Grown), 0u);
	}
	VT_CHECK_EQ(MeasureChronicleView(Grown).Digest, Before);
	VT_CHECK_EQ(A.StateDigest(), State);
	VT_CHECK_EQ(A.LogDigest(), Log);

	// And another person is another chronicle: released and taken up again,
	// the view starts over rather than mixing two lives.
	VT_REQUIRE(A.Release());
	VT_CHECK_EQ(TakeChronicleView(A.Instance(), A.Sources(), Grown), 0u);
	VT_CHECK_EQ(Grown.Person, 0u);
	VT_CHECK_EQ(Grown.LineCount, 0u);

	VAELEN_LOG_INFO(
		LogChronicle,
		"chronicle of %s (person %u) after %u days: %u lines held, %u dropped, %u bytes of text, why %u "
		"lines; events read per day min %u max %u mean %.1f, full on day %u; the fresh take read %u events in %.2f "
		"ms; %u bytes, digest %016llx",
		Life.empty() ? "-" : Life[0].c_str(), Who, Days, Fresh.LineCount, Fresh.Dropped, Fresh.Used, Fresh.WhyCount,
		ReadMin, ReadMax, static_cast<double>(ReadAll) / Days, FirstDayFull, ReadFresh, FreshMs, S.Bytes,
		static_cast<unsigned long long>(S.Digest));
}

VAELEN_TEST(Chronicle, ARefusedEatWithNoGrainSaysNothing)
{
	// The refusal the row names: an Eat with nothing to eat. The grain within
	// reach is taken away by the kernel's own AddStock, then the Eat is
	// meant, the day turned, and the newest line says so, with the code.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const uint32 Who = A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0);
	A.Day(); // the first tick of a year is where the ration tops everybody up
	World& W = A.Instance();
	const Wired& T = A.Handles();
	const Population::PersonInfo* P = Population::FindPerson(W, T.Persons, Who);
	VT_REQUIRE(P != nullptr);
	const uint32 Region = P->Region;
	const uint32 House = P->Family;
	uint32 Drained = 0;
	if (House != 0)
	{
		const Economy::HouseStock* H = Economy::HouseStockOf(W, T.Families, T.Economy_, House);
		if (H != nullptr && H->Amount[0] > 0)
		{
			Drained += Economy::AddStock(W, A.Ages(), T.Families, T.Economy_, Region, House, Economy::Good::Grain,
										 -static_cast<int32>(H->Amount[0]), W.Now());
		}
	}
	const Economy::RegionStock* R = Economy::StockOf(W, A.Ages(), T.Economy_, Region);
	if (R != nullptr && R->Amount[0] > 0)
	{
		Drained += Economy::AddStock(W, A.Ages(), T.Families, T.Economy_, Region, 0, Economy::Good::Grain,
									 -static_cast<int32>(R->Amount[0]), W.Now());
	}
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Eat);
	C.Amount = 1;
	C.Issued = A.Now();
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	A.Day();
	ChronicleView V;
	TakeChronicleView(W, A.Sources(), V);
	VT_REQUIRE(V.LineCount >= 1);
	const LineView& L = V.Lines[V.LineCount - 1];
	const std::string Text = LineAt(V, V.LineCount - 1);
	VT_CHECK_EQ(L.Kind, static_cast<uint32>(LineKind::Refused));
	VT_CHECK_EQ(L.Verb, static_cast<uint32>(Player::Intent::Eat));
	VT_CHECK_EQ(L.Refused, static_cast<uint32>(Player::Refusal::Nothing));
	VT_CHECK_MSG(Text.find(Player::RefusalName(Player::Refusal::Nothing)) != std::string::npos, "%s", Text.c_str());
	VT_CHECK_MSG(Text.find("could not") != std::string::npos, "%s", Text.c_str());
	VAELEN_LOG_INFO(LogChronicle, "%u grain taken away from person %u's reach, then: %s", Drained, Who, Text.c_str());
}

VAELEN_TEST(Chronicle, TheViewOutlivesTheWorld)
{
	ChronicleView V;
	Hash64 Before = 0;
	{
		std::unique_ptr<Aelvor> A = std::make_unique<Aelvor>(Small());
		VT_REQUIRE(A->Begin());
		VT_REQUIRE(A->TakeUp(Anywhere()) != 0);
		Player::PlayerCommand C;
		C.Kind = static_cast<uint8>(Player::Intent::Work);
		C.Amount = 1;
		C.Issued = A->Now();
		VT_CHECK(A->Submit(C) == Player::Refusal::None);
		A->Day();
		TakeChronicleView(A->Instance(), A->Sources(), V);
		Before = MeasureChronicleView(V).Digest;
		VT_REQUIRE(V.Person != 0);
		VT_REQUIRE(V.LineCount >= 1);
	} // the world is gone here
	VT_CHECK_EQ(MeasureChronicleView(V).Digest, Before);
	VT_CHECK(Ascii(V.Text, V.Lines[V.LineCount - 1].Begin + V.Lines[V.LineCount - 1].Length));
	VT_CHECK(std::strlen(V.Text + V.Lines[0].Begin) == V.Lines[0].Length);
}
