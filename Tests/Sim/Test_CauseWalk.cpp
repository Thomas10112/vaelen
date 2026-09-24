// VAELEN - VaelenSim tests
// Phase 17 task 17.04: six ends, and the one that could not be told from
// another.
//
// `History::CauseChain` walks the cause edge backwards and hands back a vector.
// It has six ways to stop and the caller can tell them apart in none of them -
// `History.cpp` reached four through one `break` and a fifth by falling out of
// the loop. A tool that walks backwards and stops cannot say whether it reached
// THE BEGINNING OF THE WORLD or FELL OFF THE END OF A LOG, and this phase is
// the one whose job is telling a person why something happened.
//
// THE PRE-FIX ARM IS KEPT AND IS PERMANENT. `TodaysChainCannotTellARootFromACut`
// runs the OLD function over the root log and the cut log and requires the two
// answers to be IDENTICAL. That indistinguishability is the defect; a test that
// only showed `CauseWalk` working could not show it, and in a year nobody would
// reconstruct why this file exists.
//
// THE LOGS ARE HAND-BUILT, not generated. A world would give a log that is
// 98.7% roots (the 17.05 census measures it), so five of the six ends would
// never occur and the test would be a test of AELVOR's demography rather than
// of the walk.
//
// STATUS: PROTOTYPE (Phase 17)
#include "VaelenTest.h"

#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/Causality.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/History.h"

#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;

namespace
{
	constexpr EventType<NoPayload> SomethingHappened = MakeEventType<NoPayload>("EraOpened");

	PersistentId EventId(uint64 Serial)
	{
		return PersistentId::Make(IdKind::Event, Serial);
	}

	/// One event, appended in ascending id order because `FindEvent` binary
	/// searches the log and every real log is built that way.
	void Add(EventLog& Log, uint64 Serial, PersistentId Cause)
	{
		Event E;
		E.Id = EventId(Serial);
		E.Tick = Serial * 10u;
		E.TypeHash = SomethingHappened.TypeHash;
		E.Cause = Cause;
		Log.Append(E);
	}

	std::vector<uint64> SerialsOf(const std::vector<const Event*>& Chain)
	{
		std::vector<uint64> Out;
		Out.reserve(Chain.size());
		for (const Event* E : Chain)
		{
			Out.push_back(E->Id.Serial());
		}
		return Out;
	}

	bool Same(const std::vector<uint64>& A, std::initializer_list<uint64> B)
	{
		if (A.size() != B.size())
		{
			return false;
		}
		usize I = 0;
		for (uint64 Value : B)
		{
			if (A[I++] != Value)
			{
				return false;
			}
		}
		return true;
	}
} // namespace

VAELEN_TEST(CauseWalk, EachEndIsReachedAndNamed)
{
	std::vector<const Event*> Chain;

	// (1) ROOT: 3 <- 2 <- 1, and 1 declares no cause. The chain is whole.
	{
		EventLog Log;
		Add(Log, 1, PersistentId::Invalid());
		Add(Log, 2, EventId(1));
		Add(Log, 3, EventId(2));
		const WalkEnd End = CauseWalk(Log, EventId(3), Chain);
		VT_CHECK_MSG(End == WalkEnd::Root, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Same(SerialsOf(Chain), {3u, 2u, 1u}), "the chain is %zu long", Chain.size());
	}

	// (2) NO SUCH EVENT: the walk is given an id the log does not hold.
	{
		EventLog Log;
		Add(Log, 1, PersistentId::Invalid());
		const WalkEnd End = CauseWalk(Log, EventId(99), Chain);
		VT_CHECK_MSG(End == WalkEnd::NoSuchEvent, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Chain.empty(), "nothing was walked, so nothing is reported: %zu", Chain.size());
	}

	// (3) CAUSE MISSING: 3 <- 2, and 2 names a cause 1 that is not in the log.
	// THE PARTIAL CHAIN IS KEPT, because a cut log is exactly when a person
	// most wants to see how far it got.
	{
		EventLog Log;
		Add(Log, 2, EventId(1));
		Add(Log, 3, EventId(2));
		const WalkEnd End = CauseWalk(Log, EventId(3), Chain);
		VT_CHECK_MSG(End == WalkEnd::CauseMissing, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Same(SerialsOf(Chain), {3u, 2u}), "the chain is %zu long", Chain.size());
	}

	// (4) CAUSE NOT AN EVENT, the low side: a PERSON caused it. IdKind is the
	// high byte, so a Person id (23) compares ABOVE an Event id (2) and would
	// trip the ordering guard if the kind were not checked first.
	{
		EventLog Log;
		Add(Log, 1, PersistentId::Invalid());
		Add(Log, 2, PersistentId::Make(IdKind::Person, 7));
		const WalkEnd End = CauseWalk(Log, EventId(2), Chain);
		VT_CHECK_MSG(End == WalkEnd::CauseNotAnEvent, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Same(SerialsOf(Chain), {2u}), "the chain is %zu long", Chain.size());
	}

	// (5) CAUSE NOT AN EVENT, the other side: an ENTITY (kind 1) compares BELOW
	// an Event (kind 2), so it sails past the ordering guard entirely. Both
	// arms are here because the kind check exists for both and a test of one
	// would pass with the checks in the wrong order.
	{
		EventLog Log;
		Add(Log, 5, PersistentId::Make(IdKind::Entity, 3));
		const WalkEnd End = CauseWalk(Log, EventId(5), Chain);
		VT_CHECK_MSG(End == WalkEnd::CauseNotAnEvent, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Same(SerialsOf(Chain), {5u}), "the chain is %zu long", Chain.size());
	}

	// (6) CAUSE NOT BEFORE EFFECT: event 2 says it was caused by event 4, which
	// had not happened. A log that disagrees with itself.
	{
		EventLog Log;
		Add(Log, 2, EventId(4));
		Add(Log, 4, PersistentId::Invalid());
		const WalkEnd End = CauseWalk(Log, EventId(2), Chain);
		VT_CHECK_MSG(End == WalkEnd::CauseNotBeforeEffect, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Same(SerialsOf(Chain), {2u}), "the chain is %zu long", Chain.size());
	}

	// (7) DEPTH EXHAUSTED: a chain of five, walked with room for three.
	{
		EventLog Log;
		Add(Log, 1, PersistentId::Invalid());
		for (uint64 Serial = 2; Serial <= 5; ++Serial)
		{
			Add(Log, Serial, EventId(Serial - 1));
		}
		WalkLimits Limits;
		Limits.Depth = 3u;
		const WalkEnd End = CauseWalk(Log, EventId(5), Chain, Limits);
		VT_CHECK_MSG(End == WalkEnd::DepthExhausted, "%s", WalkEndToString(End));
		VT_CHECK_MSG(Same(SerialsOf(Chain), {5u, 4u, 3u}), "the chain is %zu long", Chain.size());

		// AND THE BOUNDARY, which is where an off-by-one lives: the SAME log
		// walked with room for exactly five reaches the root and says Root, not
		// DepthExhausted. A walk that reached the beginning on its last allowed
		// step reached the beginning.
		Limits.Depth = 5u;
		const WalkEnd Exactly = CauseWalk(Log, EventId(5), Chain, Limits);
		VT_CHECK_MSG(Exactly == WalkEnd::Root, "at exactly the chain's length: %s", WalkEndToString(Exactly));
		VT_CHECK(Same(SerialsOf(Chain), {5u, 4u, 3u, 2u, 1u}));

		// A depth of nothing collects nothing and says so.
		Limits.Depth = 0u;
		const WalkEnd None = CauseWalk(Log, EventId(5), Chain, Limits);
		VT_CHECK_MSG(None == WalkEnd::DepthExhausted, "%s", WalkEndToString(None));
		VT_CHECK(Chain.empty());
	}

	// (8) A CUT THAT LANDS EXACTLY ON THE LIMIT, and this case exists because
	// an experiment did not fail.
	//
	// Moving the depth check ahead of the link resolution was supposed to break
	// case (7) and did not: for a WHOLE chain the two orders are identical, so
	// the test was measuring nothing about the ordering. They differ in exactly
	// one place - when the budget runs out on the step that would have found
	// the link missing - and both facts are then true at once.
	//
	// THE DATA FAULT WINS. `CauseMissing` says the log is broken;
	// `DepthExhausted` says the caller asked for less than there was. A person
	// shown the second when the first is true goes and raises the limit, and
	// learns nothing.
	{
		EventLog Log;
		Add(Log, 2, EventId(1)); // 1 is absent: the chain is CUT here
		Add(Log, 3, EventId(2));
		WalkLimits Limits;
		Limits.Depth = 2u; // and the budget runs out on exactly that step
		const WalkEnd End = CauseWalk(Log, EventId(3), Chain, Limits);
		VT_CHECK_MSG(End == WalkEnd::CauseMissing, "a cut on the limit must report the cut, not the limit: %s",
					 WalkEndToString(End));
		VT_CHECK(Same(SerialsOf(Chain), {3u, 2u}));
	}
}

VAELEN_TEST(CauseWalk, TodaysChainCannotTellARootFromACut)
{
	// THE PRE-FIX ARM, KEPT PERMANENTLY. Two logs that mean opposite things:
	//
	//   Whole: 3 <- 2 <- 1, and 1 is a genuine root. The chain is complete.
	//   Cut:   3 <- 2, and 2's cause is not in the log. The chain is BROKEN.
	//
	// `CauseChain` answers both with two events and no way to tell which. That
	// is the defect, and it is the reason 17.04 exists; this arm keeps it
	// visible after the fix rather than leaving a green test to imply the old
	// function was fine.
	EventLog Whole;
	Add(Whole, 2, PersistentId::Invalid());
	Add(Whole, 3, EventId(2));

	EventLog Cut;
	Add(Cut, 2, EventId(1));
	Add(Cut, 3, EventId(2));

	std::vector<const Event*> FromWhole;
	std::vector<const Event*> FromCut;
	CauseChain(Whole, EventId(3), FromWhole, 64u);
	CauseChain(Cut, EventId(3), FromCut, 64u);

	VT_CHECK_MSG(SerialsOf(FromWhole) == SerialsOf(FromCut),
				 "the old function now distinguishes them (%zu against %zu), so this arm has stopped testing anything",
				 FromWhole.size(), FromCut.size());
	VT_CHECK_MSG(FromWhole.size() == 2u, "both answers are two events long: %zu", FromWhole.size());

	// AND THE NEW ONE DOES TELL THEM APART, over the same two logs, in the same
	// run. Without this the arm above would pass over a walk that had stopped
	// working entirely.
	std::vector<const Event*> Chain;
	const WalkEnd EndWhole = CauseWalk(Whole, EventId(3), Chain);
	const WalkEnd EndCut = CauseWalk(Cut, EventId(3), Chain);
	VT_CHECK_MSG(EndWhole == WalkEnd::Root, "%s", WalkEndToString(EndWhole));
	VT_CHECK_MSG(EndCut == WalkEnd::CauseMissing, "%s", WalkEndToString(EndCut));
	VT_CHECK(EndWhole != EndCut);
}

VAELEN_TEST(CauseWalk, TheWrapperStillAnswersItsTwoCallers)
{
	// `CauseChain` is now `CauseWalk` with the end thrown away. Its two callers
	// - the chronicle text in HistoryText.cpp and `Atlas --why` - must see
	// exactly what they saw before, or 17.04 has changed behaviour while
	// claiming to add a return value.
	EventLog Log;
	Add(Log, 1, PersistentId::Invalid());
	for (uint64 Serial = 2; Serial <= 6; ++Serial)
	{
		Add(Log, Serial, EventId(Serial - 1));
	}

	for (const uint32 MaxDepth : {1u, 2u, 3u, 6u, 8u, 64u})
	{
		std::vector<const Event*> Old;
		std::vector<const Event*> New;
		CauseChain(Log, EventId(6), Old, MaxDepth);
		WalkLimits Limits;
		Limits.Depth = MaxDepth;
		(void)CauseWalk(Log, EventId(6), New, Limits);
		VT_CHECK_MSG(SerialsOf(Old) == SerialsOf(New), "at depth %u the wrapper gives %zu and the walk gives %zu",
					 MaxDepth, Old.size(), New.size());
		const usize Want = MaxDepth < 6u ? static_cast<usize>(MaxDepth) : usize{6};
		VT_CHECK_MSG(Old.size() == Want, "at depth %u: %zu events, expected %zu", MaxDepth, Old.size(), Want);
	}

	// An id that is not there is still an empty vector, which is what the
	// chronicle text checks before it prints anything.
	std::vector<const Event*> Nothing;
	CauseChain(Log, EventId(99), Nothing, 64u);
	VT_CHECK(Nothing.empty());
}
