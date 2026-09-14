// VAELEN - Tests/Player
// Phase 14.01: the PROBE. A translation unit that includes ONLY the leaf, built
// with ONLY VaelenCore/Public and VaelenPlayer/Public on its include path and
// linked to nothing - so that if Intent.h ever grows an include that reaches
// past those two directories, this file stops compiling and CTest Player.IntentLeaf
// goes red before any UI is written against it.
//
// It is deliberately not a Test_*.cpp: the test harness would pull the whole
// module in and prove nothing. Compiling IS the test; running it prints one line.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/Player/Intent.h"

#include <cstdio>

int main()
{
	using namespace Vaelen::Player;
	static_assert(sizeof(PlayerCommand) == 24, "the leaf carries the same 24-byte command");
	static_assert(static_cast<int>(Intent::Count) == 9, "the eight verbs and None");
	static_assert(static_cast<int>(Refusal::Count) == 10, "nine refusals and None");
	PlayerCommand C;
	C.Kind = static_cast<Vaelen::uint8>(Intent::Wait);
	C.Why = static_cast<Vaelen::uint8>(Refusal::None);
	std::printf("[probe] Intent.h is a leaf: %u kinds, %u refusals, %u bytes a command\n",
				static_cast<unsigned>(Intent::Count), static_cast<unsigned>(Refusal::Count),
				static_cast<unsigned>(sizeof(C)));
	return 0;
}
