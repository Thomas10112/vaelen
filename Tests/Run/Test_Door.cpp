// VAELEN - Tests/Run
// Phase 14.03: two inputs reach the simulation and both are recorded, and a
// replay of the record is the same world.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Regions.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <string>
#include <algorithm>
#include <string_view>

using namespace Vaelen;
using namespace Vaelen::Player;
using namespace Vaelen::Run;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogDoor);

	Options Small()
	{
		Options O;
		O.Size = 96;
		O.PreHistory = 240;
		O.Years = 60;
		O.Play = true;
		return O;
	}

	StartRules Anywhere()
	{
		StartRules R;
		R.PreferOre = 0;
		R.ToAge = 25;
		// Whoever is there: a bound start is 10.02's rule and the phase gate's
		// claim at 256; a world of 96 tiles need not hold a bound person of
		// twenty-five in its busiest region, and these tests are about the
		// door, not the start.
		R.WantBound = 0;
		return R;
	}

	Hash64 Of(const std::string& S)
	{
		return HashBytes(S.data(), S.size());
	}
} // namespace

VAELEN_TEST(Door, TheTwoInputsAreRecordedAndIssuedIsTheWorldsClock)
{
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	Door D(A, Anywhere());
	VT_CHECK(SameWorld(D.Stream().Header, A.Header()));
	const uint64 T0 = A.Now();
	const uint32 Who = D.TakeUp();
	VT_REQUIRE(Who != 0);
	VT_REQUIRE_EQ(D.Stream().Takings.size(), 1u);
	VT_CHECK_EQ(D.Stream().Takings[0].Tick, T0);
	VT_CHECK_EQ(D.Stream().Takings[0].Person, Who);
	VT_CHECK_MSG(D.TakeUp() == 0 && D.Stream().Takings.size() == 1, "nothing is recorded when nobody is taken up");

	// Issued: whatever the caller wrote, the record carries the world's tick.
	PlayerCommand C;
	C.Kind = static_cast<uint8>(Intent::Wait);
	C.Issued = 0;
	VT_CHECK(D.Mean(C) == Refusal::None);
	C.Issued = 12345;
	VT_CHECK(D.Mean(C) == Refusal::None);
	VT_REQUIRE_EQ(D.Stream().Commands.size(), 2u);
	VT_CHECK_EQ(D.Stream().Commands[0].Command.Issued, T0);
	VT_CHECK_EQ(D.Stream().Commands[1].Command.Issued, T0);
	VT_CHECK_EQ(D.Stream().Commands[0].Tick, T0);
	VT_CHECK_EQ(D.Stream().Commands[1].Tick, T0);
	VT_CHECK(D.Stream().Commands[1].Verdict == Refusal::None);

	// The day: recorded at the tick it is turned on, then twenty-four ticks.
	VT_CHECK_EQ(D.Day(), T0 + 24u);
	VT_CHECK_EQ(D.Day(), T0 + 48u);
	VT_CHECK_EQ(D.Day(), T0 + 72u);
	VT_CHECK_EQ(D.Days(), 3u);
	VT_REQUIRE_EQ(D.Stream().Days.size(), 3u);
	VT_CHECK_EQ(D.Stream().Days[0].Tick, T0);
	VT_CHECK_EQ(D.Stream().Days[1].Tick, T0 + 24u);
	VT_CHECK_EQ(D.Stream().Days[2].Tick, T0 + 48u);
	VT_CHECK_EQ(A.Now(), T0 + 72u);
	// An unknown kind is refused at the door and recorded as refused.
	C.Kind = 200;
	VT_CHECK(D.Mean(C) == Refusal::Unknown);
	VT_CHECK(D.Stream().Commands.back().Verdict == Refusal::Unknown);
	VT_CHECK_EQ(D.Stream().Commands.back().Tick, T0 + 72u);
	VAELEN_LOG_INFO(LogDoor, "recorded: %u command(s), %u taking(s), %u day(s) from tick %llu", 3u, 1u, 3u,
					static_cast<unsigned long long>(T0));
}

VAELEN_TEST(Door, AYearPlayedReplaysToTheSameWorldAndMinusItsLastDayItDoesNot)
{
	// The claim of the phase, at the module's scale: a year of the eight verbs
	// in a round, five days of nothing after it, encoded, decoded, and
	// replayed into a fresh world of the same seed - the same state, the same
	// history, the same life word for word. Then the same stream short of its
	// last day turn, which is a different world: the record is load-bearing.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	Door D(A, Anywhere());
	const uint32 Who = D.TakeUp();
	VT_REQUIRE(Who != 0);
	constexpr uint32 Days = 360;
	for (uint32 Day = 0; Day < Days; ++Day)
	{
		PlayerCommand C;
		C.Kind = static_cast<uint8>(1 + Day % 8u); // Wait, Work, Rest, Eat, Move, Speak, Give, Take, again
		C.Amount = 1 + Day % 3u;
		D.Mean(C);
		D.Day();
	}
	for (uint32 i = 0; i < 5; ++i)
	{
		D.Day();
	}
	const InputStream& Tape = D.Stream();
	VT_CHECK_EQ(Tape.Commands.size(), static_cast<usize>(Days));
	VT_CHECK_EQ(Tape.Days.size(), static_cast<usize>(Days + 5));
	VT_CHECK(!Tape.Takings.empty());
	const Hash64 State = A.StateDigest();
	const Hash64 Log = A.LogDigest();
	const std::string Story = A.Life();
	VT_CHECK(!Story.empty());

	// Through the text form, as a host would keep it.
	const std::string Text = EncodeStream(Tape);
	InputStream Back;
	StreamReport Rep;
	const StreamHeader Expect = A.Header();
	VT_REQUIRE(DecodeStream(Text, Back, Rep, &Expect));
	VT_CHECK_EQ(Rep.BadLines, 0u);
	VT_CHECK_EQ(Rep.Records, static_cast<uint32>(StreamRecords(Tape)));

	Aelvor F(Small());
	VT_REQUIRE(F.Begin());
	const ReplayReport R = Replay(F, Back, Anywhere());
	VT_CHECK_EQ(R.Refused, 0u);
	VT_CHECK_EQ(R.Wrong, 0u);
	VT_CHECK_EQ(R.Days, Days + 5);
	VT_CHECK_EQ(R.Answered, Days);
	VT_CHECK_EQ(R.Left, 0u);
	VT_CHECK_EQ(R.Takings, static_cast<uint32>(Tape.Takings.size()));
	VT_CHECK_EQ(R.ByKind[0], 0u);
	for (usize k = 1; k < IntentCount; ++k)
	{
		VT_CHECK_EQ(R.ByKind[k], Days / 8u);
	}
	VT_CHECK_MSG(R.State == State, "the world came out the same, down to the last grain");
	VT_CHECK_MSG(R.Log == Log, "with the same history in it");
	VT_CHECK_MSG(R.Life == Of(Story), "and the same life, word for word");
	VT_CHECK(F.Life() == Story);
	VT_CHECK_EQ(F.Now(), A.Now());

	// Minus its last day turn.
	Back.Days.pop_back();
	Aelvor G(Small());
	VT_REQUIRE(G.Begin());
	const ReplayReport T = Replay(G, Back, Anywhere());
	VT_CHECK_EQ(T.Refused, 0u);
	VT_CHECK_EQ(T.Days, Days + 4);
	VT_CHECK_EQ(T.Answered, Days);
	VT_CHECK_MSG(T.State != State, "a day not turned is a different world: the record is load-bearing");
	VT_CHECK_EQ(G.Now() + 24u, A.Now());
	VAELEN_LOG_INFO(
		LogDoor,
		"a year of person %u: %zu record(s), %zu bytes; replayed %u answered, %u wrong, %u day(s), state %016llx "
		"log %016llx life %016llx; minus one day %016llx",
		Who, StreamRecords(Tape), Text.size(), R.Answered, R.Wrong, R.Days, static_cast<unsigned long long>(R.State),
		static_cast<unsigned long long>(R.Log), static_cast<unsigned long long>(R.Life),
		static_cast<unsigned long long>(T.State));
}

VAELEN_TEST(Door, ACommandMeantBeforeAnybodyIsPlayedIsNotARecord)
{
	// A host whose input fires before its first TakeUp on the same tick: the
	// door answers NoPlayer and the world is untouched. Were that recorded,
	// the text form's tie rule (takings before commands) would hand it to the
	// replay AFTER the taking - accepted, and executed. So it is not a record,
	// and the replay is the same world.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	Door D(A, Anywhere());
	PlayerCommand C;
	C.Kind = static_cast<uint8>(Intent::Work);
	C.Amount = 1;
	VT_CHECK(D.Mean(C) == Refusal::NoPlayer);
	VT_CHECK_MSG(D.Stream().Commands.empty(), "NoPlayer touched nothing and is not a record");
	const uint32 Who = D.TakeUp();
	VT_REQUIRE(Who != 0);
	VT_CHECK(D.Mean(C) == Refusal::None);
	for (uint32 i = 0; i < 10; ++i)
	{
		D.Day();
		D.Mean(C);
	}
	VT_CHECK_EQ(D.Stream().Commands.size(), 11u);
	const std::string Text = EncodeStream(D.Stream());
	InputStream Back;
	StreamReport Rep;
	VT_REQUIRE(DecodeStream(Text, Back, Rep));
	Aelvor F(Small());
	VT_REQUIRE(F.Begin());
	const ReplayReport R = Replay(F, Back, Anywhere());
	VT_CHECK_EQ(R.Wrong, 0u);
	VT_CHECK_EQ(R.Answered, 11u);
	VT_CHECK_EQ(R.Days, 10u);
	VT_CHECK_MSG(R.State == A.StateDigest(), "the same world, with the refused command in neither");
	VT_CHECK(R.Log == A.LogDigest());
}

VAELEN_TEST(Door, AStreamOfAnotherWorldOrIntoAnUnplayedRunIsRefused)
{
	Options O;
	O.Size = 64;
	O.PreHistory = 30;
	O.Years = 5;
	O.Play = true;
	Aelvor A(O);
	InputStream S;
	S.Header = A.Header();
	VT_CHECK_MSG(Replay(A, S).Refused == 1, "not before Begin()");
	VT_CHECK_MSG(A.Day() == A.Now() && A.Now() == 0, "and no day turns before Begin()");
	VT_REQUIRE(A.Begin());
	S.Header.Seed ^= 1;
	VT_CHECK_MSG(Replay(A, S).Refused == 1, "another seed is another world");
	S.Header = A.Header();
	S.Header.Size = 65;
	VT_CHECK_MSG(Replay(A, S).Refused == 1, "another size is another world");
	S.Header = A.Header();
	const ReplayReport Empty = Replay(A, S);
	VT_CHECK_EQ(Empty.Refused, 0u);
	VT_CHECK_EQ(Empty.Days, 0u);
	VT_CHECK_EQ(Empty.Answered, 0u);
	VT_CHECK_MSG(Empty.State == A.StateDigest(), "an empty stream replays to the world as it is");
	Options N = O;
	N.Play = false;
	Aelvor B(N);
	VT_REQUIRE(B.Begin());
	S.Header = B.Header();
	VT_CHECK_MSG(Replay(B, S).Refused == 1, "a Run without Play has no door");
}

VAELEN_TEST(Door, ALookIsARecordedInputAndAStreamWithoutOneStillReads)
{
	// Phase 15 task 15.06. Where the host is looking is an INPUT: it goes
	// through the door, it is stamped with the world's clock, it is written to
	// the stream and a replay puts it back. What the world DOES with it is
	// 15.07's warden - this task is the road, not the traffic.
	Options O;
	O.Size = 128;
	O.Years = 100;
	O.Play = true;
	Aelvor A(O);
	VT_REQUIRE(A.Begin());
	Player::StartRules Rules;
	Rules.WantBound = 0;
	Door D(A, Rules);
	VT_REQUIRE(D.TakeUp() != 0);

	// The door stamps the tick, whatever the host thinks the time is - the same
	// rule Mean follows for Issued (ADR-0138).
	const uint64 At = A.Now();
	D.Look(Attention{7u, 2u, 4u});
	VT_REQUIRE(D.Stream().Looks.size() == 1);
	VT_CHECK_MSG(D.Stream().Looks[0].Tick == At, "the world's clock, not the host's");
	VT_CHECK(D.Stream().Looks[0].Region == 7u);
	VT_CHECK(D.Stream().Looks[0].Reach == 2u);
	VT_CHECK_MSG(A.Attending().Region == 7u, "and the world was told at once");
	VT_CHECK_MSG(A.Attending().Most == 4u, "including what the host will pay for, which is not recorded");

	// Through the DOOR and not the Run: a day turned outside the door is a day
	// the stream never heard about, and then the replay never reaches the ticks
	// the later looks were stamped with. The first draft of this test called
	// Aelvor::Day directly and lost two of its three looks that way, which is
	// the same lesson Door.h states at the top of the file.
	D.Day();
	D.Look(Attention{9u, 1u, 4u});
	D.Day();
	D.Look(Attention{0u, 0u, 4u}); // the camera left, which is as much an input
	VT_REQUIRE(D.Stream().Looks.size() == 3);

	// The text form round trips, and the look lines carry no Most.
	const std::string Text = Player::EncodeStream(D.Stream());
	Player::InputStream Back;
	Player::StreamReport Report;
	VT_REQUIRE(Player::DecodeStream(Text, Back, Report));
	VT_CHECK_MSG(Report.BadLines == 0, "every line of it is a record this build knows");
	VT_CHECK(Back.Looks.size() == 3);
	VT_CHECK_MSG(Player::EncodeStream(Back) == Text, "encode, decode, encode: the same bytes");
	for (usize i = 0; i < Back.Looks.size(); ++i)
	{
		VT_CHECK(Back.Looks[i].Tick == D.Stream().Looks[i].Tick);
		VT_CHECK(Back.Looks[i].Region == D.Stream().Looks[i].Region);
		VT_CHECK(Back.Looks[i].Reach == D.Stream().Looks[i].Reach);
	}

	// A STREAM WRITTEN BEFORE LOOKS EXISTED STILL READS, which is why the text
	// form's version did not change and why Replay.Played still pins the
	// owner's month. Same stream with every `l` line taken out.
	{
		std::string Older;
		usize From = 0;
		while (From < Text.size())
		{
			const usize End = Text.find('\n', From);
			const usize Stop = End == std::string::npos ? Text.size() : End;
			const std::string_view Line(Text.data() + From, Stop - From);
			if (Line.empty() || Line[0] != 'l')
			{
				Older.append(Line);
				Older.push_back('\n');
			}
			From = Stop + 1;
		}
		Player::InputStream Old;
		Player::StreamReport OldReport;
		VT_REQUIRE(Player::DecodeStream(Older, Old, OldReport));
		VT_CHECK_MSG(OldReport.BadLines == 0, "a stream with no looks is not a stream with bad lines");
		VT_CHECK(Old.Looks.empty());
		VT_CHECK(Old.Days.size() == D.Stream().Days.size());
		VT_CHECK(Old.Takings.size() == D.Stream().Takings.size());
	}

	// A corrupt look line is counted and skipped, never crashed on.
	{
		Player::InputStream Bad;
		Player::StreamReport BadReport;
		const std::string Broken = Text + "l 12 notanumber 3\n";
		VT_REQUIRE(Player::DecodeStream(Broken, Bad, BadReport));
		VT_CHECK_MSG(BadReport.BadLines == 1, "one line neither blank nor a record");
		VT_CHECK(Bad.Looks.size() == 3);
	}

	// And a replay puts the looks back through the same door, in order.
	{
		Aelvor Fresh(O);
		VT_REQUIRE(Fresh.Begin());
		const ReplayReport R = Replay(Fresh, D.Stream(), Rules);
		VT_CHECK(R.Refused == 0);
		VT_CHECK_MSG(R.Looks == 3, "three looks recorded, three applied");
		VT_CHECK_MSG(Fresh.Attending().Region == 0u, "and the world ends where the last look left it");
		VT_CHECK_MSG(Fresh.Attending().Most == 0u, "with Most from the host and not from the stream");
	}
}

VAELEN_TEST(Door, TheWardenTurnsALookIntoRequestsAndDoesNotThrashOnABorder)
{
	// Phase 15 task 15.07. The warden is a function from attention to detail
	// REQUESTS - never a promotion, because the bridge is the only thing that
	// promotes and it decides on its own cadence (ADR-0037, ADR-0141). What is
	// asserted here is that it is deterministic, that it obeys the host's
	// budget, that it gives back what a new look no longer wants, and that
	// walking back and forth over one border does not thrash.
	Options O;
	O.Size = 128;
	O.Years = 100;
	O.Play = true;
	O.Stream = true;

	const auto Wardened = [](Options Ask, const std::vector<Attention>& Looks)
	{
		Aelvor A(Ask);
		std::vector<std::vector<uint16>> Seen;
		if (!A.Begin())
		{
			return Seen; // the caller checks it is not empty
		}
		for (const Attention& At : Looks)
		{
			A.LookAt(At);
			Seen.push_back(A.Watching());
		}
		return Seen;
	};

	// ── Deterministic: the same looks into the same world give the same asks,
	// every time, in the same order. This is the property a replay lives on.
	const std::vector<Attention> Walk = {{5u, 1u, 4u}, {6u, 1u, 4u}, {5u, 1u, 4u}, {6u, 1u, 4u}};
	const std::vector<std::vector<uint16>> Once = Wardened(O, Walk);
	const std::vector<std::vector<uint16>> Twice = Wardened(O, Walk);
	VT_CHECK_MSG(Once == Twice, "the same attention makes the same requests, or no replay is possible");

	// ── The host's budget is obeyed, and ascending order is what it is.
	for (const std::vector<uint16>& Asked : Once)
	{
		VT_CHECK_MSG(Asked.size() <= 4u, "never more than the host said it would pay for");
		VT_CHECK(std::is_sorted(Asked.begin(), Asked.end()));
	}
	Options Tight = O;
	const std::vector<std::vector<uint16>> Small = Wardened(Tight, {{5u, 2u, 1u}});
	VT_CHECK_MSG(Small[0].size() <= 1u, "a budget of one buys one");

	// ── A look at nowhere gives everything back.
	const std::vector<std::vector<uint16>> Away = Wardened(O, {{5u, 1u, 4u}, {0u, 0u, 4u}});
	VT_CHECK_MSG(!Away[0].empty(), "looking somewhere asks for something");
	VT_CHECK_MSG(Away[1].empty(), "and looking nowhere asks for nothing");

	// ── HYSTERESIS. Walking back and forth over one border must not promote
	// and demote the same places every day: a promotion costs about 65 ms
	// (14.10) and a demotion destroys every person it made, so thrashing there
	// is not a stutter, it is a world that keeps forgetting a place and
	// inventing it again. The Keep band is one step wider than the Want band,
	// so a neighbour stays asked-for while the camera is next door.
	{
		Aelvor A(O);
		VT_REQUIRE(A.Begin());
		// Two regions that are ACTUALLY neighbours, read out of the graph
		// rather than guessed: a band one step wide does nothing between two
		// places that do not touch, and the first draft of this assertion
		// picked 5 and 6 out of the air and measured ten changes because the
		// camera was teleporting, not walking.
		const WorldGen::RegionGraph Graph = WorldGen::BuildRegionGraph(A.Instance().Map(), A.Ages().World.Regions);
		uint32 Here = 0;
		uint32 There = 0;
		for (uint32 R = 1; R < Graph.Neighbours.size() && Here == 0; ++R)
		{
			for (const uint16 N : Graph.Neighbours[R])
			{
				if (N != 0)
				{
					Here = R;
					There = N;
					break;
				}
			}
		}
		VT_REQUIRE(Here != 0 && There != 0);
		A.LookAt(Attention{Here, 1u, 4u});
		uint32 Changes = 0;
		std::vector<uint16> Last = A.Watching();
		for (uint32 i = 0; i < 10; ++i)
		{
			A.LookAt(Attention{(i % 2) == 0 ? There : Here, 1u, 4u});
			Changes += A.Watching() == Last ? 0u : 1u;
			Last = A.Watching();
		}
		VAELEN_LOG_INFO(LogDoor, "ten crossings of one border changed the watched set %u time(s)", Changes);
		VT_CHECK_MSG(Changes <= 2u, "ten crossings of one border are not ten changes of mind");
	}

	// ── And a world that did not ask for streaming has no warden at all, which
	// is what keeps every frozen digest of fourteen phases where it is.
	{
		Options Plain = O;
		Plain.Stream = false;
		Aelvor A(Plain);
		VT_REQUIRE(A.Begin());
		A.LookAt(Attention{5u, 1u, 4u});
		VT_CHECK_MSG(A.Attending().Region == 5u, "the look is still remembered");
		VT_CHECK_MSG(A.Watching().empty(), "and nothing was asked of the world");
	}
}
