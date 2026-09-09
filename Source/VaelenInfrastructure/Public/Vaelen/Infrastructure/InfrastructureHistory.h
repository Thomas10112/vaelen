// VAELEN - VaelenInfrastructure
// Phase 09.07: infrastructure in the chronicle.
//
// STATUS: VALIDATED (Phase 09) - integration/text/deterministic tests in Tests/Infrastructure
//
// The phase has spent six tasks building things and none of it saying so. The
// event log holds every unit of timber taken and every year a road was mended,
// which is exactly what nobody remembers. A chronicle is the small part of that
// a century keeps: a granary raised, a mill fallen in, a road cut, a town
// settled on a river, a place emptied.
//
// The same shape as 08.07 one layer up. Every infrastructure event has a
// sentence whether the chronicle kept it or not, stamped with the year and the
// age in the same hand as every layer below, falling through to the military
// text for everything else - so an infrastructure chronicle can tell you about
// a battle, and through that about a harvest, and through that about a person.
//
// And the why runs the other way. A famine is the drought of 03.05, and 04.04
// asks a region's stores how much of that drought it can absorb. From 09.02
// those stores include the granary standing there - so the why of a famine in a
// region with no granary is now a chain that ends at the year the granary fell
// in and nobody rebuilt it, which is the sentence this whole phase exists to be
// able to write.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Infrastructure/Buildings.h"
#include "Vaelen/Infrastructure/Decay.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
#include "Vaelen/Infrastructure/Places.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Military/MilitaryHistory.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"

#include <string>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Infrastructure
{
	struct WorksChronicleRules
	{
		uint32 RecordRaisings = 1; ///< BuildingRaised
		uint32 RecordFalls = 1;	   ///< BuildingFell
		uint32 RecordRoads = 1;	   ///< RoadCut, RoadLost
		uint32 RecordPlaces = 1;   ///< PlaceSettled, PlaceEmptied
		/// An enlargement from two to three is not history; a work raised where
		/// there was none is. Set this to record the growing as well.
		uint32 RecordEnlargements = 0;
		/// A town growing by one is not history either; a town that reaches this
		/// size for the first time is the year it is remembered for.
		uint32 SizeWorthRecording = 4;
		uint32 MaxRecordsPerYear = 8; ///< per region
	};

	/// Singleton component: the listener's tallies.
	struct WorksChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 Region = 0;
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(WorksChronicleState) == 24, "WorksChronicleState must stay padding free");

	struct WorksChronicleTypes
	{
		ComponentType<WorksChronicleState> State;
		static VAELEN_INFRASTRUCTURE_API WorksChronicleTypes Declare(World& W);
	};

	/// Everything the infrastructure text needs to name things.
	struct WorksContext
	{
		InfrastructureTypes Buildings;
		PlaceTypes Places;
		RoadTypes Roads;
		/// Optional: with it, an event of the military layer (and, through it,
		/// of every layer under that) gets its own line rather than the plain one.
		const Military::MilitaryContext* Military = nullptr;
	};

	/// Listener: the infrastructure events that matter become chronicle records.
	class VAELEN_INFRASTRUCTURE_API WorksChronicle final : public IEventListener
	{
	public:
		WorksChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, WorksContext InContext,
					   WorksChronicleTypes InState, WorksChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Context(InContext), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "WorksChronicle"; }
		void OnEvent(const Event& E) override;
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Region) const;
		World* Owner;
		History::PreHistoryTypes Types;
		WorksContext Context;
		WorksChronicleTypes State;
		WorksChronicleRules Rules;
	};

	/// "the granary of Kratfa", or "a granary" when the region is unknown.
	VAELEN_INFRASTRUCTURE_API void NameWork(const World& W, const History::PreHistoryTypes& Types, uint32 Region,
											Work Kind, std::string& Out);
	/// "the road between Kratfa and Zakru", or "road 3".
	VAELEN_INFRASTRUCTURE_API void NameRoad(const World& W, const History::PreHistoryTypes& Types,
											const WorksContext& Context, uint32 Route, std::string& Out);
	/// One line for any event: the infrastructure events get their own sentence,
	/// every other event goes through the military text (and so down to the person).
	VAELEN_INFRASTRUCTURE_API void DescribeWorksEvent(const World& W, const History::PreHistoryTypes& Types,
													  const WorksContext& Context, const Event& E, std::string& Out,
													  const Population::PersonIndex* Index = nullptr);
	/// The whole chronicle as text, in tick order, one line each.
	VAELEN_INFRASTRUCTURE_API uint32 ExportChronicleWithWorks(const World& W, const History::PreHistoryTypes& Types,
															  const WorksContext& Context, std::string& Out,
															  uint32 MaxLines = 0);
	/// The why of an event id as text: the event, then "because ..." lines to the root.
	VAELEN_INFRASTRUCTURE_API uint32 ExportWhyWithWorks(const World& W, const History::PreHistoryTypes& Types,
														const WorksContext& Context, PersistentId Id, std::string& Out);

	struct WorksChronicleStats
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Described = 0; ///< records whose event has an infrastructure line
		uint32 WithRegion = 0;
		uint32 EraConsistent = 0;
		uint32 ByType[4] = {}; ///< raised, fallen, road, place
	};
	VAELEN_INFRASTRUCTURE_API WorksChronicleStats CheckWorksChronicle(const World& W,
																	  const History::PreHistoryTypes& Types,
																	  const WorksContext& Context,
																	  const WorksChronicleTypes& State);
} // namespace Vaelen::Infrastructure
