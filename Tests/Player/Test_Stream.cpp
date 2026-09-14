// VAELEN - Tests/Player
// Phase 14.01: the played input as a stream - what was meant, when, and how
// the days were turned - written out and read back without loss.
//
// STATUS: PROTOTYPE (Phase 14)

#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/Stream.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Player;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogStream);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	StreamHeader Aelvor128()
	{
		StreamHeader H;
		H.Seed = AelvorSeed;
		H.Size = 128;
		H.PreHistory = 300;
		H.Years = 120;
		return H;
	}

	Recorded Meant(uint64 Tick, Intent Kind, uint32 Target, uint32 Amount, Refusal Verdict)
	{
		Recorded R;
		R.Tick = Tick;
		R.Command.Kind = static_cast<uint8>(Kind);
		R.Command.Target = Target;
		R.Command.Amount = Amount;
		R.Command.Hours = 0;
		R.Command.Issued = Tick;
		R.Verdict = Verdict;
		return R;
	}

	/// A month of a life: a taking, commands every day, a day turn every day,
	/// with ties at equal ticks on purpose - that is where an order rule earns
	/// its keep.
	InputStream AMonth()
	{
		InputStream S;
		S.Header = Aelvor128();
		const uint64 T0 = 8640ull * 300ull; // some tick after pre-history
		S.Takings.push_back(TakenUp{T0, 4212, 0});
		for (uint32 Day = 0; Day < 30; ++Day)
		{
			const uint64 T = T0 + 24ull * Day;
			S.Commands.push_back(Meant(T, Intent::Work, 0, 0, Refusal::None));
			if (Day % 3 == 0)
			{
				S.Commands.push_back(Meant(T, Intent::Eat, 0, 1, Refusal::None));
			}
			if (Day == 7)
			{
				S.Commands.push_back(Meant(T, Intent::Move, 99, 0, Refusal::None));
			}
			if (Day == 20)
			{
				S.Commands.push_back(Meant(T, Intent::Give, 4300, 2, Refusal::Full));
			}
			S.Days.push_back(DayTurned{T});
		}
		return S;
	}

	bool Same(const Recorded& A, const Recorded& B)
	{
		return A.Tick == B.Tick && A.Command.Kind == B.Command.Kind && A.Command.Target == B.Command.Target &&
			   A.Command.Amount == B.Command.Amount && A.Command.Hours == B.Command.Hours &&
			   A.Command.Issued == B.Command.Issued && A.Verdict == B.Verdict;
	}
} // namespace

VAELEN_TEST(Stream, TheRecordsAreFlatAndTheirPaddingIsNamed)
{
	// The static_asserts in the header are the test; this makes them run
	// through the harness so a failure has a name in the suite and not only in
	// a compiler error.
	VT_CHECK_EQ(sizeof(Recorded), usize{40});
	VT_CHECK_EQ(sizeof(TakenUp), usize{16});
	VT_CHECK_EQ(sizeof(DayTurned), usize{8});
	VT_CHECK_EQ(sizeof(StreamHeader), usize{24});
	VT_CHECK_EQ(sizeof(PlayerCommand), usize{24});
}

VAELEN_TEST(Stream, ARoundTripIsByteIdentical)
{
	const InputStream S = AMonth();
	const std::string Once = EncodeStream(S);
	VT_CHECK_MSG(!Once.empty(), "a month encodes to something");
	VT_CHECK_MSG(Once.rfind("vaelen-stream 1 ", 0) == 0, "the header is the first line");

	InputStream Back;
	StreamReport R;
	VT_REQUIRE(DecodeStream(Once, Back, R));
	VT_CHECK_EQ(R.BadLines, 0u);
	VT_CHECK_MSG(R.Lines == static_cast<uint32>(std::count(Once.begin(), Once.end(), '\n')),
				 "Lines is what wc -l says: one per newline, none for the nothing after the last");
	VT_CHECK_EQ(R.HeaderBad, 0u);
	VT_CHECK_EQ(R.Refused, 0u);
	VT_CHECK_EQ(R.Records, static_cast<uint32>(StreamRecords(S)));
	VT_CHECK_EQ(Back.Commands.size(), S.Commands.size());
	VT_CHECK_EQ(Back.Takings.size(), S.Takings.size());
	VT_CHECK_EQ(Back.Days.size(), S.Days.size());
	VT_CHECK(SameWorld(Back.Header, S.Header));
	for (usize i = 0; i < S.Commands.size() && i < Back.Commands.size(); ++i)
	{
		VT_CHECK_MSG(Same(S.Commands[i], Back.Commands[i]), "every command survives the text");
	}
	for (usize i = 0; i < S.Days.size() && i < Back.Days.size(); ++i)
	{
		VT_CHECK_EQ(S.Days[i].Tick, Back.Days[i].Tick);
	}

	// The whole claim: encode(decode(encode(S))) == encode(S), byte for byte.
	const std::string Twice = EncodeStream(Back);
	VT_CHECK_MSG(Once == Twice, "the text form is a fixed point of encode-decode");
	VAELEN_LOG_INFO(LogStream, "a month: %u records, %u bytes of text, round trip byte-identical",
					static_cast<uint32>(StreamRecords(S)), static_cast<uint32>(Once.size()));
}

VAELEN_TEST(Stream, EqualTicksKeepTheirOrder)
{
	// At one tick: the taking, then the commands, then the day turn. That is
	// what really happens - a life taken up at T, its first command issued at
	// T, the day turned at T - and a replay that read them in another order
	// would submit that first command to nobody, or turn the day before it.
	InputStream S;
	S.Header = Aelvor128();
	const uint64 T = 1000;
	S.Days.push_back(DayTurned{T});
	S.Takings.push_back(TakenUp{T, 7, 0});
	S.Commands.push_back(Meant(T, Intent::Rest, 0, 0, Refusal::None));
	S.Commands.push_back(Meant(T, Intent::Wait, 0, 0, Refusal::None));
	const std::string Text = EncodeStream(S);
	const usize Header = Text.find('\n');
	VT_REQUIRE(Header != std::string::npos);
	const std::string Body = Text.substr(Header + 1);
	VT_CHECK_MSG(Body.rfind("t 1000 ", 0) == 0, "the first record at a tick is the taking");
	const usize FirstC = Body.find("\nc 1000 ");
	const usize FirstD = Body.find("\nd 1000");
	VT_CHECK_MSG(FirstC != std::string::npos && FirstD != std::string::npos, "all three kinds are present");
	VT_CHECK_MSG(FirstC < FirstD, "the taking, then the commands, then the day turn");
	// And the two commands keep the order they were meant in.
	const usize RestAt = Body.find("c 1000 3 ");
	const usize WaitAt = Body.find("c 1000 1 ");
	VT_CHECK_MSG(RestAt < WaitAt, "Rest was meant before Wait and is written before it");
}

VAELEN_TEST(Stream, ACorruptLineIsCountedAndNotCrashedOn)
{
	const InputStream S = AMonth();
	std::string Text = EncodeStream(S);
	// Three kinds of damage, in the middle of the file, none of them fatal.
	const usize Middle = Text.find("\nd ", Text.size() / 2);
	VT_REQUIRE(Middle != std::string::npos);
	Text.insert(Middle + 1, "c 12 not a number\n");
	Text.insert(Middle + 1, "x 5 5\n");
	Text.insert(Middle + 1, "d 99999999999999999999999\n"); // overflows uint64
	InputStream Back;
	StreamReport R;
	const bool Survived = DecodeStream(Text, Back, R);
	VT_CHECK_MSG(Survived, "damage to records does not fail the stream");
	VT_REQUIRE(Survived);
	VT_CHECK_EQ(R.BadLines, 3u);
	VT_CHECK_EQ(R.Records, static_cast<uint32>(StreamRecords(S)));
	VT_CHECK_EQ(Back.Commands.size(), S.Commands.size());
	VT_CHECK_EQ(Back.Days.size(), S.Days.size());

	// A field out of range for its type is a bad line too, not a truncation.
	std::string Wide = EncodeStream(S);
	Wide += "c 5 300 0 0 0 5 0\n"; // Kind 300 does not fit a uint8
	VT_REQUIRE(DecodeStream(Wide, Back, R));
	VT_CHECK_EQ(R.BadLines, 1u);
	VT_CHECK_EQ(Back.Commands.size(), S.Commands.size());
}

VAELEN_TEST(Stream, AHeaderOfAnotherWorldIsRefused)
{
	const InputStream S = AMonth();
	const std::string Text = EncodeStream(S);
	InputStream Back;
	StreamReport R;

	StreamHeader Other = Aelvor128();
	Other.Seed ^= 1;
	VT_CHECK_MSG(!DecodeStream(Text, Back, R, &Other), "another seed is another world");
	VT_CHECK_EQ(R.Refused, 1u);
	VT_CHECK_MSG(Back.Commands.empty(), "a refused stream leaves Out untouched");

	Other = Aelvor128();
	Other.Size = 256;
	VT_CHECK_MSG(!DecodeStream(Text, Back, R, &Other), "another size is another world");
	VT_CHECK_EQ(R.Refused, 1u);

	Other = Aelvor128();
	Other.Years = 121;
	VT_CHECK_MSG(!DecodeStream(Text, Back, R, &Other), "another moment in the same world is refused too");

	const StreamHeader Same_ = Aelvor128();
	VT_CHECK_MSG(DecodeStream(Text, Back, R, &Same_), "the world it was played in accepts it");
	VT_CHECK_EQ(R.Refused, 0u);

	// No header at all, or a header that is not one.
	VT_CHECK(!DecodeStream("", Back, R));
	VT_CHECK_EQ(R.HeaderBad, 1u);
	VT_CHECK_EQ(R.Lines, 0u);
	VT_CHECK(!DecodeStream("c 1 1 0 0 0 1 0\n", Back, R));
	VT_CHECK_EQ(R.HeaderBad, 1u);
	VT_CHECK(!DecodeStream("vaelen-stream 1 x 128 300 120\n", Back, R));
	VT_CHECK_EQ(R.HeaderBad, 1u);
}

VAELEN_TEST(Stream, AVersionThisBuildCannotReadIsNamedAsSuch)
{
	// A well-formed header of another version is not "another world" and not
	// "not a header": it is a form this build does not read, and it says so.
	// Reading a version-2 stream under version-1 rules would drop every field
	// it did not know as a bad line and call the rest a success.
	InputStream Back;
	StreamReport R;
	VT_CHECK(!DecodeStream("vaelen-stream 2 7 128 300 120\nd 5\n", Back, R));
	VT_CHECK_EQ(R.VersionBad, 1u);
	VT_CHECK_EQ(R.HeaderBad, 0u);
	VT_CHECK_EQ(R.Refused, 0u);
	VT_CHECK_MSG(Back.Days.empty(), "a stream of another version leaves Out untouched");
	VT_CHECK(!DecodeStream("vaelen-stream 0 7 128 300 120\n", Back, R));
	VT_CHECK_EQ(R.VersionBad, 1u);
	// With Expect too: the version is not part of the world's identity.
	StreamHeader Same_ = Aelvor128();
	VT_CHECK(!DecodeStream("vaelen-stream 2 7 128 300 120\n", Back, R, &Same_));
	VT_CHECK_EQ(R.VersionBad, 1u);
	VT_CHECK_EQ(R.Refused, 0u);
	Same_.Version = 2;
	VT_CHECK_MSG(SameWorld(Aelvor128(), Same_), "SameWorld compares the world, not the text form");
}

VAELEN_TEST(Stream, TheLastPossibleTickIsARecordAndNotASentinel)
{
	// 2^64-1 is a tick like any other. The first draft of the merge used it
	// to mean "this vector is exhausted", and a taking at that tick with no
	// commands read past the end of an empty vector. Each vector alone, then
	// all three at that tick, then the round trip.
	const uint64 Last = ~uint64{0};
	{
		InputStream S;
		S.Header = Aelvor128();
		S.Takings.push_back(TakenUp{Last, 7, 0});
		const std::string Text = EncodeStream(S);
		VT_CHECK_MSG(Text.find("\nt 18446744073709551615 7\n") != std::string::npos, "a taking at the last tick");
	}
	{
		InputStream S;
		S.Header = Aelvor128();
		S.Days.push_back(DayTurned{Last});
		const std::string Text = EncodeStream(S);
		VT_CHECK_MSG(Text.find("\nd 18446744073709551615\n") != std::string::npos, "a day turned at the last tick");
	}
	{
		InputStream S;
		S.Header = Aelvor128();
		S.Commands.push_back(Meant(Last, Intent::Wait, 0, 0, Refusal::None));
		const std::string Text = EncodeStream(S);
		VT_CHECK_MSG(Text.find("\nc 18446744073709551615 ") != std::string::npos, "a command at the last tick");
	}
	InputStream S;
	S.Header = Aelvor128();
	S.Days.push_back(DayTurned{Last});
	S.Commands.push_back(Meant(Last, Intent::Wait, 0, 0, Refusal::None));
	S.Takings.push_back(TakenUp{Last, 7, 0});
	const std::string Once = EncodeStream(S);
	InputStream Back;
	StreamReport R;
	VT_REQUIRE(DecodeStream(Once, Back, R));
	VT_CHECK_EQ(R.Records, 3u);
	VT_CHECK_EQ(R.BadLines, 0u);
	VT_CHECK_MSG(EncodeStream(Back) == Once, "the round trip holds at the last tick too");
	const usize Header = Once.find('\n');
	VT_REQUIRE(Header != std::string::npos);
	const std::string Body = Once.substr(Header + 1);
	VT_CHECK_MSG(Body.rfind("t ", 0) == 0 && Body.find("\nc ") < Body.find("\nd "),
				 "and the tie rule holds there: taking, command, day");
}

VAELEN_TEST(Stream, TheSameStreamAlwaysWritesTheSameBytes)
{
	const InputStream S = AMonth();
	const std::string A = EncodeStream(S);
	const std::string B = EncodeStream(S);
	VT_CHECK(A == B);
	// And a stream read back from Windows line endings is the same stream.
	std::string Crlf;
	for (char c : A)
	{
		if (c == '\n')
		{
			Crlf += "\r\n";
		}
		else
		{
			Crlf += c;
		}
	}
	InputStream Back;
	StreamReport R;
	VT_REQUIRE(DecodeStream(Crlf, Back, R));
	VT_CHECK_EQ(R.BadLines, 0u);
	VT_CHECK(EncodeStream(Back) == A);
}
