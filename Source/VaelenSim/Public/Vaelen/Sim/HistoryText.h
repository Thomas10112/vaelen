// VAELEN - VaelenSim
// Phase 03.07: queryable history and the chronicle as text.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// "Why did this happen?" From any event or entity the history answers with
// the cause chain and the era; every region has a timeline of records; and
// the chronicle can be read as text lines built from the records, the names
// given in 03.03 and the entities the events are about. The text is a pure
// function of the state and the log, so it is identical on every platform
// and after a snapshot.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/SimApi.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	/// A step of an explanation: the event, its era and the text of it.
	struct WhyStep
	{
		const Event* Cause = nullptr;
		uint32 Era = 0;
		uint32 Region = 0; ///< region of the subject when it is a region, else 0
	};

	/// The name of an entity, or a deterministic fallback ("region 12",
	/// "culture 3", "era 2", "religion 1", "river 4", "lake 2", "entity 12345").
	VAELEN_SIM_API void NameEntity(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::string& Out);
	/// The name of a region by index, same fallback.
	VAELEN_SIM_API void NameRegion(const World& W, const PreHistoryTypes& Types, uint32 Region, std::string& Out);

	/// The event that made an entity: the earliest chronicled event whose
	/// subject is the entity (culture founded, religion founded, era opened...).
	/// Null when the entity has no record.
	VAELEN_SIM_API const Event* OriginOf(const World& W, const PreHistoryTypes& Types, PersistentId Entity);

	/// Why: from an event id, or from an entity id (through its origin), the
	/// cause chain from the event itself to the root cause, each with its era
	/// and region. Empty when the id resolves to nothing.
	VAELEN_SIM_API void Why(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::vector<WhyStep>& Out,
							uint32 MaxDepth = 64);

	/// Every record about a region, in tick order (then id order).
	VAELEN_SIM_API void RegionTimeline(const World& W, const PreHistoryTypes& Types, uint32 Region,
									   std::vector<RecordInfo>& Out);

	/// One line of chronicle for an event: "Year 231, age of Divik: the people
	/// of Thuthanyo settled Ekdu." Unknown event types get a generic line.
	VAELEN_SIM_API void DescribeEvent(const World& W, const PreHistoryTypes& Types, const Event& E, std::string& Out);
	/// One line for a record (its event described, or a fallback when the
	/// event is gone from the log).
	VAELEN_SIM_API void DescribeRecord(const World& W, const PreHistoryTypes& Types, const RecordInfo& R,
									   std::string& Out);

	/// The chronicle as text: every record in tick order, one line each, at
	/// most MaxLines (0 = all). Returns the number of lines written.
	VAELEN_SIM_API uint32 ExportChronicle(const World& W, const PreHistoryTypes& Types, std::string& Out,
										  uint32 MaxLines = 0);
	/// The timeline of a region as text.
	VAELEN_SIM_API uint32 ExportRegionChronicle(const World& W, const PreHistoryTypes& Types, uint32 Region,
												std::string& Out);
	/// The explanation of an id as text, one line per step, the root cause last.
	VAELEN_SIM_API uint32 ExportWhy(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::string& Out);

	struct ChronicleStats
	{
		uint32 Records = 0;
		uint32 Resolved = 0;	  ///< records whose event is in the log
		uint32 EraConsistent = 0; ///< records whose era equals EraAt(tick)
		uint32 WithRegion = 0;
		uint32 Described = 0; ///< records with a type-specific line
	};
	VAELEN_SIM_API ChronicleStats CheckChronicle(const World& W, const PreHistoryTypes& Types);
} // namespace Vaelen::History
