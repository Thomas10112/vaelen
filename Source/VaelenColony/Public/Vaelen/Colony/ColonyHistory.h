// VAELEN - VaelenColony
// Phase 11 task 11.07: the colony in the chronicle.
//
// Nothing here invents a record. Every chronicle in this project is the same
// shape - a listener that turns the events that matter into records in the
// world's history, and a sentence for each - and this is that shape for a
// colony: the ground taken, the people bound to it, the ore lifted out of it
// and the seams that give no more.
//
// What a colony is worth remembering FOR is not every day of it. A day's lift
// is not history; a seam worked out is. The rules below say which is which, and
// a colony that lifts ore every single day for a century would otherwise write
// thirty-six thousand records nobody will ever read.
//
// STATUS: PROTOTYPE (Phase 11) - text/deterministic tests in Tests/Colony/Test_Chronicle.cpp
#pragma once

#include "Vaelen/Colony/ColonyApi.h"
#include "Vaelen/Colony/Holders.h"
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"

#include <string>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Colony
{
	struct ColonyChronicleRules
	{
		uint32 RecordFoundings = 1;	 ///< ColonyFounded
		uint32 RecordSeamsSpent = 1; ///< SeamWorkedOut
		/// A day's lift is not history. A year in which the colony lifted this
		/// much or more is: it is the year the seam gave up its best.
		uint32 LiftWorthRecording = 200;
		/// The binding of a colony is one act however many people it took, so it
		/// is remembered as one record and not as a thousand.
		uint32 RecordBindings = 1;
		uint32 MaxRecordsPerYear = 8;
	};

	/// Singleton component: the listener's tallies.
	struct ColonyChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 InYear = 0;
		uint32 Lifted = 0; ///< units this year, against LiftWorthRecording
		uint32 Bound = 0;  ///< people bound this year, told as one record
	};
	static_assert(sizeof(ColonyChronicleState) == 24, "ColonyChronicleState must stay padding free");

	struct ColonyChronicleTypes
	{
		ComponentType<ColonyChronicleState> State;
		static VAELEN_COLONY_API ColonyChronicleTypes Declare(World& W);
	};

	/// Everything the colony text needs to name things.
	struct ColonyContext
	{
		ColonyTypes Colony;
		Population::PersonTypes Persons;
		Society::BondageTypes Bondage;
	};

	/// "the colony of Kratfa", or "the colony" when the region is unknown.
	VAELEN_COLONY_API void NameColony(const World& W, const History::PreHistoryTypes& Types, uint32 Region,
									  std::string& Out);
	/// A sentence for one colony event, in the same hand as every layer below.
	/// False when the event is not one of this layer's.
	VAELEN_COLONY_API bool DescribeColonyEvent(const World& W, const History::PreHistoryTypes& Types,
											   const ColonyContext& Context, const Event& E, std::string& Out);
	/// One line for a year's binding, which is one act however many people it
	/// took. A thousand records saying the same thing on the same day is not a
	/// chronicle, it is a ledger - so the binding of a colony is told once, with
	/// the count in the sentence.
	VAELEN_COLONY_API void DescribeBinding(const World& W, const History::PreHistoryTypes& Types, uint32 Region,
										   uint32 People, std::string& Out);
} // namespace Vaelen::Colony
