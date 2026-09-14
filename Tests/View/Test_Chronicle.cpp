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
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/Regions.h"
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

	/// What the kernel's log says about one person, in the view's own terms:
	/// the five kinds LifeTimeline keeps, as {Tick, Kind, Verb, Refused}.
	struct Said
	{
		uint64 Tick = 0;
		uint32 Kind = 0;
		uint32 Verb = 0;
		uint32 Refused = 0;
	};
	void SaidOf(const World& W, uint32 Person, std::vector<Said>& Out)
	{
		Out.clear();
		for (const Event& E : W.Log().All())
		{
			Said S;
			S.Tick = static_cast<uint64>(E.Tick);
			if (E.Is(Player::PlayerActedEvent) || E.Is(Player::PlayerRefusedEvent))
			{
				const Player::ActPayload A = E.Get<Player::ActPayload>();
				if (A.Person != Person)
				{
					continue;
				}
				S.Verb = A.Kind;
				S.Kind = static_cast<uint32>(E.Is(Player::PlayerRefusedEvent) ? LineKind::Refused : LineKind::Acted);
				S.Refused = E.Is(Player::PlayerRefusedEvent) ? A.Amount : 0u;
				Out.push_back(S);
				continue;
			}
			if (E.Is(Population::PersonMovedEvent) || E.Is(Population::PersonBornEvent) ||
				E.Is(Population::PersonDiedEvent))
			{
				if (E.Get<Population::PersonPayload>().Person != Person)
				{
					continue;
				}
				S.Kind = static_cast<uint32>(E.Is(Population::PersonMovedEvent)	 ? LineKind::Walked
											 : E.Is(Population::PersonBornEvent) ? LineKind::Born
																				 : LineKind::Died);
				Out.push_back(S);
			}
		}
	}

	std::string WhyAt(const ChronicleView& V, uint32 i)
	{
		return std::string(V.WhyText + V.Why[i].Begin, V.Why[i].Length);
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
	VT_CHECK_EQ(sizeof(ChronicleView), usize{7816});
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
	std::string PerDay;
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
		if (Day < 12)
		{
			PerDay += (Day > 0 ? " " : "") + std::to_string(Read);
		}
		VT_CHECK(Read >= 1); // at least the day's act or refusal
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
	const ChronicleViewStats S = MeasureChronicleView(Grown);
	VT_CHECK_EQ(S.Lines, ChronicleLines);
	VT_CHECK_EQ(S.NonAscii, 0u);
	VT_CHECK_EQ(S.Truncated, 0u);
	VT_CHECK(S.Bytes <= 8192);
	VT_CHECK_EQ(S.EventsRead, static_cast<uint32>(Grown.Since));
	VT_CHECK(Ascii(Grown.Text, Grown.Used));
	VT_CHECK_EQ(Grown.LastHash, A.Instance().Log().At(A.Instance().Log().Count() - 1).Hash());

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

	// Every line's numbers are the kernel's: the last LineCount events about
	// them in the log, tick, kind, verb and refusal.
	std::vector<Said> Kernel;
	SaidOf(A.Instance(), Who, Kernel);
	VT_REQUIRE(Kernel.size() >= Grown.LineCount);
	VT_CHECK_EQ(static_cast<uint32>(Kernel.size()), Grown.LineCount + Grown.Dropped);
	for (uint32 i = 0; i < Grown.LineCount; ++i)
	{
		const Said& K = Kernel[Kernel.size() - Grown.LineCount + i];
		VT_CHECK_EQ(Grown.Lines[i].Tick, K.Tick);
		VT_CHECK_EQ(Grown.Lines[i].Kind, K.Kind);
		VT_CHECK_EQ(Grown.Lines[i].Verb, K.Verb);
		VT_CHECK_EQ(Grown.Lines[i].Refused, K.Refused);
	}
	uint32 Acted = 0;
	uint32 Refused = 0;
	for (uint32 i = 0; i < Grown.LineCount; ++i)
	{
		Acted += Grown.Lines[i].Kind == static_cast<uint32>(LineKind::Acted) ? 1u : 0u;
		Refused += Grown.Lines[i].Kind == static_cast<uint32>(LineKind::Refused) ? 1u : 0u;
	}
	VT_CHECK_MSG(Acted > 0 && Refused > 0, "the round has both kinds: %u acted, %u refused", Acted, Refused);

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
	VT_CHECK_MSG(Grown.WhyOf != 0, "a year of giving and taking is followed by something");
	VT_CHECK_MSG(Grown.WhyCount == WhyLines, "the thing and its cause: %u why lines", Grown.WhyCount);
	VT_CHECK(Why.size() >= Grown.WhyCount);
	for (uint32 i = 0; i < Grown.WhyCount; ++i)
	{
		const std::string Mine(Grown.WhyText + Grown.Why[i].Begin, Grown.Why[i].Length);
		VT_CHECK_MSG(i < Why.size() && Mine == Why[i], "why %u: view '%s' vs life '%s'", i, Mine.c_str(),
					 i < Why.size() ? Why[i].c_str() : "-");
		VT_CHECK(Grown.WhyText[Grown.Why[i].Begin + Grown.Why[i].Length] == '\0');
	}
	VT_CHECK(WhyAt(Grown, 1).rfind("  because ", 0) == 0);
	VT_CHECK_EQ(Grown.Why[1].Kind, static_cast<uint32>(LineKind::Acted)); // the cause is what they did
	VT_CHECK(Grown.Why[1].Verb != 0); // a Work, Eat, Give or Take: the verbs that move a stock

	// Without the goods (a host that carries no Markets), the lines are the
	// same lines: only a why step of the economy layer could be worded
	// otherwise, and the done section says so.
	{
		ViewSources Plain = A.Sources();
		Plain.HasGoods = false;
		ChronicleView Poorer;
		TakeChronicleView(A.Instance(), Plain, Poorer);
		VT_CHECK_EQ(Poorer.LineCount, Grown.LineCount);
		VT_CHECK_EQ(Poorer.Used, Grown.Used);
		VT_CHECK(std::memcmp(Poorer.Text, Grown.Text, ChronicleTextBytes) == 0);
		VT_CHECK(std::memcmp(Poorer.Lines, Grown.Lines, sizeof(Grown.Lines)) == 0);
		VT_CHECK_EQ(Poorer.WhyOf, Grown.WhyOf);
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

	// And another person is another chronicle: released, the view is empty;
	// taken up again, it is the new life's from its first event, not a
	// continuation of the old one.
	VT_REQUIRE(A.Release());
	VT_CHECK_EQ(TakeChronicleView(A.Instance(), A.Sources(), Grown), 0u);
	VT_CHECK_EQ(Grown.Person, 0u);
	VT_CHECK_EQ(Grown.LineCount, 0u);
	const uint32 Next = A.TakeUp(Anywhere());
	VT_REQUIRE(Next != 0 && Next != Who);
	VT_CHECK_EQ(TakeChronicleView(A.Instance(), A.Sources(), Grown), static_cast<uint32>(A.Instance().Log().Count()));
	VT_CHECK_EQ(Grown.Person, Next);
	SaidOf(A.Instance(), Next, Kernel);
	VT_CHECK_EQ(Grown.LineCount + Grown.Dropped, static_cast<uint32>(Kernel.size()));
	VT_CHECK_EQ(Grown.WhyOf, uint64{0});
	for (uint32 i = 0; i < Grown.LineCount; ++i)
	{
		VT_CHECK_EQ(Grown.Lines[i].Tick, Kernel[Kernel.size() - Grown.LineCount + i].Tick);
	}

	VAELEN_LOG_INFO(
		LogChronicle,
		"chronicle of %s (person %u) after %u days: %u lines held, %u dropped, %u bytes of text, why %u "
		"lines; events read per day min %u max %u mean %.1f (the first days: %s), full on day %u; the fresh take "
		"read %u events in %.2f ms; %u bytes, digest %016llx",
		Life.empty() ? "-" : Life[0].c_str(), Who, Days, Fresh.LineCount, Fresh.Dropped, Fresh.Used, Fresh.WhyCount,
		ReadMin, ReadMax, static_cast<double>(ReadAll) / Days, PerDay.c_str(), FirstDayFull, ReadFresh, FreshMs,
		S.Bytes, static_cast<unsigned long long>(S.Digest));
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
	{
		const Economy::HouseStock* H = House != 0 ? Economy::HouseStockOf(W, T.Families, T.Economy_, House) : nullptr;
		const Economy::RegionStock* S = Economy::StockOf(W, A.Ages(), T.Economy_, Region);
		VT_CHECK_MSG((H == nullptr || H->Amount[0] == 0) && (S == nullptr || S->Amount[0] == 0),
					 "no grain within reach: house %u, region %u", H != nullptr ? H->Amount[0] : 0u,
					 S != nullptr ? S->Amount[0] : 0u);
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

VAELEN_TEST(Chronicle, AWalkIsTwoLinesAndItsOwnWhy)
{
	// The kinds the round-robin never reaches: a Move that is not refused
	// is an Acted line ("walked to X") and, the same tick, a Walked line
	// ("walked from Home to X") caused by it - which makes it the newest
	// thing that happened because of them, so the why is those two lines
	// the other way round. A neighbour is detailed the kernel's way first,
	// as Test_Life does.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const uint32 Who = A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0);
	const Wired& T = A.Handles();
	const Population::PersonInfo* P = Population::FindPerson(A.Instance(), T.Persons, Who);
	VT_REQUIRE(P != nullptr);
	const uint32 Home = P->Region;
	WorldGen::RegionGraphCache Ways;
	const WorldGen::RegionGraph& G = Ways.Of(A.Instance().Map(), A.Ages().World.Regions);
	VT_REQUIRE(Home < G.Neighbours.size() && !G.Neighbours[Home].empty());
	const uint32 There = G.Neighbours[Home][0];
	VT_CHECK(Population::RequestDetail(A.Instance(), T.Lod, There) ||
			 Population::IsDetailed(A.Instance(), A.Ages(), T.Persons, There));
	uint32 Waited = 0;
	for (; Waited < 400 && !Population::IsDetailed(A.Instance(), A.Ages(), T.Persons, There); ++Waited)
	{
		A.Day();
	}
	VT_REQUIRE(Population::IsDetailed(A.Instance(), A.Ages(), T.Persons, There));

	ChronicleView V;
	TakeChronicleView(A.Instance(), A.Sources(), V); // grown from here: the walk lands on an existing view
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Move);
	C.Target = There;
	C.Amount = 1;
	C.Issued = A.Now();
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	A.Day();
	VT_CHECK(TakeChronicleView(A.Instance(), A.Sources(), V) >= 2);
	P = Population::FindPerson(A.Instance(), T.Persons, Who);
	VT_REQUIRE(P != nullptr);
	VT_CHECK_MSG(P->Region == There, "person %u is in %u, not %u", Who, P->Region, There);
	VT_REQUIRE(V.LineCount >= 2);
	const LineView& Went = V.Lines[V.LineCount - 2];
	const LineView& Walked = V.Lines[V.LineCount - 1];
	VT_CHECK_EQ(Went.Kind, static_cast<uint32>(LineKind::Acted));
	VT_CHECK_EQ(Went.Verb, static_cast<uint32>(Player::Intent::Move));
	VT_CHECK_EQ(Walked.Kind, static_cast<uint32>(LineKind::Walked));
	VT_CHECK_EQ(Walked.Verb, 0u);
	VT_CHECK_EQ(Walked.Tick, Went.Tick);
	const std::string WentText = LineAt(V, V.LineCount - 2);
	const std::string WalkedText = LineAt(V, V.LineCount - 1);
	VT_CHECK_MSG(WentText.find(" walked to ") != std::string::npos, "%s", WentText.c_str());
	VT_CHECK_MSG(WalkedText.find(" walked from ") != std::string::npos, "%s", WalkedText.c_str());

	// The why: the walk, because of the order to walk - ExportLife's own
	// why block, and the same lines as the view's.
	VT_REQUIRE(V.WhyCount == WhyLines);
	VT_CHECK_EQ(WhyAt(V, 0), WalkedText);
	VT_CHECK_EQ(V.Why[0].Kind, static_cast<uint32>(LineKind::Walked));
	VT_CHECK_EQ(V.Why[1].Kind, static_cast<uint32>(LineKind::Acted));
	VT_CHECK_EQ(V.Why[1].Verb, static_cast<uint32>(Player::Intent::Move));
	const usize Colon = WentText.find(": ");
	VT_REQUIRE(Colon != std::string::npos);
	VT_CHECK_EQ(WhyAt(V, 1), "  because " + WentText.substr(Colon + 2));
	std::vector<std::string> Life;
	std::vector<std::string> Timeline;
	std::vector<std::string> Why;
	WholeLife(A, Life);
	TimelineOf(Life, Timeline, Why);
	VT_REQUIRE(Why.size() >= 2);
	VT_CHECK_EQ(WhyAt(V, 0), Why[0]);
	VT_CHECK_EQ(WhyAt(V, 1), Why[1]);

	// Grown here equals fresh here, walk included.
	ChronicleView Fresh;
	TakeChronicleView(A.Instance(), A.Sources(), Fresh);
	VT_CHECK(std::memcmp(&V, &Fresh, sizeof(ChronicleView)) == 0);
	VAELEN_LOG_INFO(LogChronicle, "after %u day(s) of waiting for region %u: '%s' / '%s'; why: '%s' / '%s'", Waited,
					There, WentText.c_str(), WalkedText.c_str(), WhyAt(V, 0).c_str(), WhyAt(V, 1).c_str());
}

VAELEN_TEST(Chronicle, AnotherWorldStartsTheViewOver)
{
	// A view is resumed into the world it was taken from and no other: a
	// world whose log is shorter than what was read, and a world of the
	// same seed whose log diverged before the cursor, both start it over -
	// and what it holds then is that world's fresh view, byte for byte.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const uint32 Who = A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0);
	Aelvor B(Small());
	VT_REQUIRE(B.Begin());
	VT_REQUIRE_EQ(B.TakeUp(Anywhere()), Who); // the same seed, the same first moment
	for (uint32 d = 0; d < 5; ++d)
	{
		Player::PlayerCommand C;
		C.Amount = 1;
		C.Kind = static_cast<uint8>(Player::Intent::Wait); // one event a day
		C.Issued = A.Now();
		VT_CHECK(A.Submit(C) == Player::Refusal::None);
		A.Day();
		C.Kind = static_cast<uint8>(Player::Intent::Work); // two: the act and the stock it adds to
		C.Issued = B.Now();
		VT_CHECK(B.Submit(C) == Player::Refusal::None);
		B.Day();
	}
	ChronicleView V;
	TakeChronicleView(A.Instance(), A.Sources(), V);
	VT_REQUIRE(V.LineCount >= 5);
	const ChronicleView OfA = V;

	// The same person, a log at least as long, another world: the last
	// event read is not the same event.
	VT_CHECK(B.Instance().Log().Count() >= V.Since);
	VT_CHECK(B.Instance().Log().At(V.Since - 1).Hash() != V.LastHash);
	TakeChronicleView(B.Instance(), B.Sources(), V);
	ChronicleView FreshB;
	TakeChronicleView(B.Instance(), B.Sources(), FreshB);
	VT_CHECK_MSG(std::memcmp(&V, &FreshB, sizeof(ChronicleView)) == 0, "resumed into B is B's fresh view");
	VT_CHECK(std::memcmp(&V, &OfA, sizeof(ChronicleView)) != 0);

	// A shorter log: a world at its first moment.
	Aelvor C(Small());
	VT_REQUIRE(C.Begin());
	VT_REQUIRE_EQ(C.TakeUp(Anywhere()), Who);
	V = OfA;
	VT_CHECK(C.Instance().Log().Count() < V.Since);
	const uint32 Read = TakeChronicleView(C.Instance(), C.Sources(), V);
	VT_CHECK_EQ(Read, static_cast<uint32>(C.Instance().Log().Count()));
	ChronicleView FreshC;
	TakeChronicleView(C.Instance(), C.Sources(), FreshC);
	VT_CHECK_MSG(std::memcmp(&V, &FreshC, sizeof(ChronicleView)) == 0, "resumed into C is C's fresh view");

	// And back into its own world it resumes, reading nothing.
	V = OfA;
	VT_CHECK_EQ(TakeChronicleView(A.Instance(), A.Sources(), V), 0u);
	VT_CHECK(std::memcmp(&V, &OfA, sizeof(ChronicleView)) == 0);
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
