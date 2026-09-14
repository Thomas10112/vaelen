// VAELEN - Tests/Run
// Phase 14.03: two inputs reach the simulation and both are recorded, and a
// replay of the record is the same world.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <string>

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
