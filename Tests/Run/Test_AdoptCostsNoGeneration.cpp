// VAELEN - VaelenRun tests
// Phase 16 gate, clause (h): a 256 checkpoint adopted into a world that was
// only CONSTRUCTED reaches the saved digest, is playable, and generates
// nothing.
//
// The clause existed unbuilt while the phase was reported as closed. Its
// substance was covered inside Run.Checkpoint's 128 gate cell, which asserts
// Generations() == 0 and the digest thirty days on - but not at 256, not under
// the name the gate calls for, and WITHOUT the playability checks, which are
// the half that says a restored world is a world somebody can go on living in
// rather than a correct-looking corpse.
//
// THE INSTRUMENT IS A CALL COUNTER AND NEVER A STOPWATCH. `Generate` being
// skipped cannot be read off the result: a world generated and then loaded
// over looks exactly like a world only loaded, and it would merely be slower.
// ADR-0109 forbids asserting on the wall clock, so the milliseconds below are
// PRINTED and nothing compares them to anything.
//
// STATUS: PROTOTYPE (Phase 16) - ctest Run.AdoptCostsNoGeneration
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Sim/Snapshot.h"
#include "VaelenTest.h"

#include <chrono>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

VAELEN_TEST(AdoptCostsNoGeneration, A256CheckpointRestoresIntoAWorldThatNeverGenerated)
{
	// 256 tiles, as the clause says, at twenty years of pre-history and ten
	// lived. Measured on this machine: Begin 4.7 s, container 51.7 MiB, Adopt
	// 712 ms, peak 223 MiB - which a CI runner holds without noticing. The
	// SIZE is what the clause pins; the history is chosen to be affordable.
	Options O;
	O.Size = 256u;
	O.PreHistory = 20u;
	O.Years = 10u;
	O.Play = true;
	O.Stream = true;
	O.Lively = true;
	O.Colony = true;

	const auto Started = std::chrono::steady_clock::now();
	Aelvor Source(O);
	VT_REQUIRE(Source.Begin());
	const auto Begun = std::chrono::steady_clock::now();
	VT_CHECK_MSG(Source.Generations() == 1u, "the source generated its world exactly once");

	VT_REQUIRE(Source.TakeUp(Player::StartRules{}) != 0u);
	for (uint32 Day = 0; Day < 12u; ++Day)
	{
		Attention At;
		At.Region = static_cast<uint32>(1u + (Day % 9u));
		At.Reach = 1u;
		Source.LookAt(At);
		Source.Day();
	}
	VT_REQUIRE(!Source.Watching().empty());
	const Hash64 Truth = ComputeStateDigest(Source.Instance());

	std::vector<uint8> Bytes;
	VT_REQUIRE(BuildCheckpoint(Source, Bytes) == CheckpointResult::Ok);

	// CONSTRUCTED AND NOT BEGUN. This is the whole claim: Adopt is entitled to
	// skip generation because a checkpoint already holds its result.
	Aelvor Taken(O);
	VT_CHECK_MSG(Taken.Generations() == 0u, "a constructed world has generated nothing yet");
	const auto BeforeAdopt = std::chrono::steady_clock::now();
	VT_REQUIRE(Taken.Adopt(Bytes.data(), Bytes.size()) == Aelvor::AdoptResult::Ok);
	const auto Adopted = std::chrono::steady_clock::now();

	VT_CHECK_MSG(Taken.Generations() == 0u, "and it STILL has, after adopting a world of 65536 tiles");
	VT_CHECK_MSG(ComputeStateDigest(Taken.Instance()) == Truth, "the adopted world is the saved world: %016llx",
				 static_cast<unsigned long long>(ComputeStateDigest(Taken.Instance())));

	// AND IT IS PLAYABLE, which the 128 cell never checked. A world that
	// restores to the right digest and then cannot be lived in has restored a
	// picture, not a world.
	const uint64 Was = Taken.Now();
	VT_CHECK_MSG(Taken.Day() > Was, "a day turns: %llu was %llu", static_cast<unsigned long long>(Taken.Now()),
				 static_cast<unsigned long long>(Was));

	const std::vector<uint16> Watched = Taken.Watching();
	Attention Elsewhere;
	// A region nothing was looking at, so a change in Watching() is this look's
	// doing and not a coincidence of where the source had been.
	//
	// AND THE GUARD WAS CHECKED THE WRONG WAY FIRST. Pointing the look at
	// region 0 - a host looking NOWHERE - was meant to make it fail, and it did
	// not: looking nowhere is itself a change, and Watching() moves. That says
	// something true about the world rather than about the test, and the guard
	// that does fire is removing the look entirely, which was then done.
	Elsewhere.Region = 40u;
	Elsewhere.Reach = 1u;
	Taken.LookAt(Elsewhere);
	VT_CHECK_MSG(Taken.Watching() != Watched, "looking somewhere new changes what is watched: %zu was %zu",
				 Taken.Watching().size(), Watched.size());

	VT_REQUIRE(Taken.Played() != 0u);
	VT_CHECK_MSG(Taken.Release(), "the carried life can be put down");
	const uint32 Who = Taken.TakeUp(Player::StartRules{});
	VT_CHECK_MSG(Who != 0u, "and the restored world still offers somebody to play: person %u", Who);

	// ADR-0109: printed, never asserted on.
	const double BeginMs = std::chrono::duration<double, std::milli>(Begun - Started).count();
	const double AdoptMs = std::chrono::duration<double, std::milli>(Adopted - BeforeAdopt).count();
	std::printf("  [adopt256] Begin %.0f ms, Adopt %.0f ms, container %.1f MiB, Generations %u\n", BeginMs, AdoptMs,
				static_cast<double>(Bytes.size()) / 1048576.0, Taken.Generations());
}
