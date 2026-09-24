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
		/// A STORY: 98.7% of this world's events are here, because the cause
		/// edge is filled at 30 of 132 publish sites.
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
		/// nine times the deepest chain this tree has ever measured (one edge).
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
} // namespace Vaelen::History
