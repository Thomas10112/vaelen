// VAELEN - VaelenSim
// Phase 17 task 17.04: a causal walk that says HOW it ended.
//
// THE DEFECT. `History::CauseChain` walks an event's `Cause` edge backwards and
// hands the caller a vector. It has FIVE ways to stop and the caller can tell
// them apart in none of them:
//
//   the starting id is not in the log ..... an empty vector
//   a genuine root cause .................. a vector
//   a cause that is not an event .......... a vector
//   a cause that does not precede it ...... a vector
//   a link the log does not have .......... a vector
//   the depth limit ....................... a vector
//
// `History.cpp:198-211` reaches four of those through the same `break` and the
// fifth by falling out of the loop condition. So a tool that walks backwards
// and stops cannot say whether it reached the BEGINNING OF THE WORLD or fell
// off the end of a log - and Phase 17 is the phase whose whole job is telling a
// person why something happened.
//
// THE SIXTH STOP IS NOT HERE, AND THAT IS A DECISION. The plan wrote
// `WalkLimits{Depth, Nodes}` and a `BudgetExhausted` end beside it. A backwards
// walk is LINEAR - one node per step - so a node budget and a depth limit can
// never disagree, and a state no input can reach is a state no test can reach
// either. Two limits that always agree look thorough and measure one thing.
// There is also no cycle to guard against: `CauseNotBeforeEffect` makes every
// step strictly decrease the id, so the walk terminates whatever the data says.
//
// STATUS: PROTOTYPE (Phase 17 task 17.04) - Tests/Sim/Test_CauseWalk.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/SimApi.h"

#include <vector>

namespace Vaelen::History
{
	/// Why a causal walk stopped. Every one of these is a DIFFERENT fact about
	/// the log, and four of them used to be the same `break`.
	enum class WalkEnd : uint8
	{
		/// The chain reached an event that declares no cause. THE BEGINNING OF
		/// A STORY - and where most stories begin: 69% of a fresh AELVOR 128's
		/// events, and every birth, death, marriage and seating of a ruler in
		/// it. 17.05 measured it; the 98.7% the plan quoted was three ten-year
		/// worlds, not this one.
		Root,
		/// The id the walk was GIVEN is not in the log. Nothing was walked and
		/// `Out` is empty - which the old function reported the same way it
		/// reported an event with no cause.
		NoSuchEvent,
		/// A cause id that names an event this log does not contain. The chain
		/// is real and it is CUT: somewhere, a log was truncated, or an event
		/// was published with the id of one that was never published.
		CauseMissing,
		/// A cause whose `IdKind` is not `Event` - a person, a region, an era.
		///
		/// CHECKED BEFORE `CauseNotBeforeEffect`, and the order is not
		/// cosmetic. `IdKind` is the HIGH byte of `PersistentId::Value`, so an
		/// Entity cause (kind 1) always compares BELOW an Event effect (kind 2)
		/// and sails past the ordering guard, while a Region cause (kind 10)
		/// always compares above it and would be reported as an ordering fault.
		/// Both are the same mistake and it is this one.
		CauseNotAnEvent,
		/// A cause whose id does not precede its effect's. An event cannot be
		/// caused by something that had not happened yet, so this is a log that
		/// disagrees with itself rather than a chain that ended.
		CauseNotBeforeEffect,
		/// The chain is longer than `WalkLimits::Depth` and there was somewhere
		/// left to go. NOT reported when the chain ends exactly at the limit:
		/// a walk that reached a root on its last allowed step reached a root.
		DepthExhausted,
	};

	VAELEN_SIM_API const char* WalkEndToString(WalkEnd End) noexcept;

	struct WalkLimits
	{
		/// How many events the walk may collect. 64 by default, which is about
		/// twenty times the deepest chain this tree has measured (three edges,
		/// at AELVOR 128 over 420 years; 17.05).
		uint32 Depth = 64;
	};

	/// Walks `Id`'s cause edge backwards, `Id` first, and says how it ended.
	///
	/// `Out` is cleared first and holds what WAS reached, whatever the end -
	/// a partial chain is the most useful thing to show a person when a log is
	/// cut, and throwing it away would make `CauseMissing` less informative
	/// than the function this replaces.
	VAELEN_SIM_API WalkEnd CauseWalk(const EventLog& Log, PersistentId Id, std::vector<const Event*>& Out,
									 WalkLimits Limits = WalkLimits{});

	// ── 17.05: the census ────────────────────────────────────────────────────
	//
	// WHAT IS ACTUALLY IN THE CAUSAL GRAPH, countable and re-runnable.
	//
	// The Phase 17 panel measured 7 causes in 535 events - 1.31% - with a
	// deepest chain of ONE EDGE, and three of its four angles had planned a
	// walker, an index and a renderer over that. A figure written once into a
	// report becomes folklore; a figure a command re-prints stays true or stops
	// being green. This is the command.
	//
	// It is also allowed to conclude that the phase is about something else:
	// 17.09's deeper `why` is conditional on what this reports.

	/// Every depth here is in EDGES, so an event with no cause has depth 0.
	/// A "chain of depth 7" is eight events.
	struct CauseCensus
	{
		/// Every event in the log.
		uint64 Events = 0;
		/// Events whose `Cause` is set at all, valid or not.
		uint64 WithCause = 0;
		/// Events whose `Cause` is unset - the beginning of a story, and the
		/// overwhelming majority of this world.
		uint64 RootCauses = 0;
		/// A cause of kind Event that the log does not contain: a cut chain.
		uint64 Dangling = 0;
		/// A cause whose kind is not Event - a person, a region, an era.
		uint64 NotAnEvent = 0;
		/// A cause that does not precede its effect: a log disagreeing with
		/// itself.
		uint64 NotBeforeEffect = 0;
		/// The longest chain, in edges.
		uint32 MaxDepth = 0;
		/// The median event's depth, in edges. Reported because a maximum alone
		/// cannot tell one deep chain from a graph that is deep throughout.
		uint32 MedianDepth = 0;
		/// The most effects any single event has. 0 when nothing causes
		/// anything.
		uint32 MaxFanOut = 0;
		/// The event with `MaxFanOut` effects, so a reader can go and look at
		/// it. Invalid when `MaxFanOut` is 0.
		PersistentId Busiest;
	};

	/// Counts the whole log in one pass, then a second for the fan-out.
	///
	/// EVERY FIELD IS 0 FOR AN EMPTY LOG, including `MaxDepth` and `MaxFanOut`
	/// - which is the same answer as "a log with no causal edges at all", and
	/// deliberately so: the census reports what is there and does not editorial-
	/// ise. `Events` is what tells the two apart.
	VAELEN_SIM_API CauseCensus TakeCauseCensus(const EventLog& Log);
} // namespace Vaelen::History
