// VAELEN - Tests/Player
// Phase 14.01: the PROBE. A translation unit that includes ONLY the leaf - both
// of its headers, Intent.h and Stream.h - built with ONLY VaelenCore/Public and
// VaelenPlayer/Public on its include path and linked to no library, so that if
// either header ever grows an include that reaches past those two directories,
// this file stops compiling and CTest Player.IntentLeaf goes red before any UI
// is written against it. It IS held to the project's warning flags
// (vaelen_build_flags carries options and definitions and no include
// directory): a probe that compiled under no standard would prove less.
//
// It is deliberately not a Test_*.cpp: the test harness would pull the whole
// module in and prove nothing. Compiling IS the test; running it prints one line.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/Stream.h"

#include <cstdio>

int main()
{
	using namespace Vaelen::Player;
	static_assert(sizeof(PlayerCommand) == 24, "the leaf carries the same 24-byte command");
	static_assert(static_cast<int>(Intent::Count) == 9, "the eight verbs and None");
	static_assert(static_cast<int>(Refusal::Count) == 10, "nine refusals and None");
	static_assert(sizeof(Recorded) == 40 && sizeof(TakenUp) == 16 && sizeof(DayTurned) == 8,
				  "the three records, as Stream.h asserts them");
	PlayerCommand C;
	C.Kind = static_cast<Vaelen::uint8>(Intent::Wait);
	C.Why = static_cast<Vaelen::uint8>(Refusal::None);
	// The stream's types are instantiated; its functions are not called, since
	// they live in the library this probe deliberately does not link.
	InputStream S;
	S.Commands.push_back(Recorded{1, C, Refusal::None, {}});
	S.Takings.push_back(TakenUp{1, 7, 0});
	S.Days.push_back(DayTurned{1});
	const StreamReport Report;
	std::printf("[probe] Intent.h and Stream.h are leaves: %u kinds, %u refusals, %u bytes a command, %u records, "
				"a report of %u bytes\n",
				static_cast<unsigned>(Intent::Count), static_cast<unsigned>(Refusal::Count),
				static_cast<unsigned>(sizeof(C)),
				static_cast<unsigned>(S.Commands.size() + S.Takings.size() + S.Days.size()),
				static_cast<unsigned>(sizeof(Report)));
	return 0;
}
