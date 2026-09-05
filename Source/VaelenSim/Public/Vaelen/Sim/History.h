// VAELEN - VaelenSim
// Phase 03.01: eras, the chronicle and history queries.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// Eras are entities opened by the yearly EraSystem: a new era opens when the
// current one reaches its span or when a listener requests one with a cause.
// The chronicle is a listener that turns every event of a chronicled type into
// a Record entity carrying the era and the region of the event's subject.
// Every piece of state lives in components (the pending request included), so
// a restored world continues identically (ADR-0015 rule 6).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/System.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	enum class EraTrigger : uint32
	{
		Founding = 0, ///< the first era
		Span,		  ///< the previous era reached its span
		Requested,	  ///< a listener asked for a new era (cause recorded)
	};

	/// Component of an era entity (ids of kind Era, added to IdKind for 03.01).
	struct EraInfo
	{
		uint32 Index = 0; ///< 1-based, in order of opening
		uint32 Trigger = 0;
		uint64 Start = 0; ///< tick the era opened
		uint64 End = 0;	  ///< tick the era closed; 0 while open
		uint64 Cause = 0; ///< event id that opened it (0 for founding and span)
	};

	/// Component of a record entity (ids of kind Document): one per chronicled event.
	struct RecordInfo
	{
		uint64 Event = 0; ///< PersistentId value of the event
		uint64 Tick = 0;
		Hash64 Type = 0; ///< event type hash
		uint64 Subject = 0;
		uint32 Era = 0;	   ///< era index at the event's tick
		uint32 Region = 0; ///< region index of the subject when it is a region, else 0
	};

	/// Singleton component on the history entity: the era system's own state.
	struct HistoryState
	{
		uint64 PendingCause = 0;   ///< event id of a pending era request
		uint32 PendingRequest = 0; ///< 1 while a request waits for the yearly tick
		uint32 OpenEra = 0;		   ///< index of the open era (0 before founding)
		uint32 EraCount = 0;
		uint32 RecordCount = 0;
	};

	struct HistoryTypes
	{
		ComponentType<EraInfo> Era;
		ComponentType<RecordInfo> Record;
		ComponentType<HistoryState> State;

		static HistoryTypes Declare(World& W);
	};

	struct EraRules
	{
		uint64 SpanTicks = 8640ull * 100; ///< 100 years at the default calendar
	};

	/// Events published by the era system (subject = the era entity).
	struct EraPayload
	{
		uint32 Index = 0;
		uint32 Trigger = 0;
	};
	inline constexpr EventType<EraPayload> EraOpenedEvent = MakeEventType<EraPayload>("EraOpened");
	inline constexpr EventType<EraPayload> EraClosedEvent = MakeEventType<EraPayload>("EraClosed");

	/// Creates the history entity with its state component. Call once on a
	/// fresh world (never on a restored one). Returns the handle.
	VAELEN_SIM_API EntityHandle InitializeHistory(World& W, const HistoryTypes& Types);

	/// Yearly (LOD World): founds the first era, closes an era at its span or on
	/// request and opens the next, publishing EraClosed / EraOpened.
	class VAELEN_SIM_API EraSystem final : public ISystem
	{
	public:
		EraSystem(World& InWorld, HistoryTypes InTypes, EraRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Eras"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		void Tick(TickContext& Context) override;

		/// Asks for a new era at the next yearly tick, remembering the cause.
		/// A second request before that tick keeps the first cause.
		void RequestEra(PersistentId Cause);

	private:
		World* Owner;
		HistoryTypes Types;
		EraRules Rules;
	};

	/// Listener that records every event of the subscribed types.
	class VAELEN_SIM_API Chronicle final : public IEventListener
	{
	public:
		Chronicle(World& InWorld, HistoryTypes InTypes, WorldGen::RegionTypes InRegions) noexcept
			: Owner(&InWorld), Types(InTypes), Regions(InRegions)
		{
		}
		const char* GetListenerName() const noexcept override { return "Chronicle"; }
		void OnEvent(const Event& E) override;
		/// Subscribes to a type; every event of it becomes a record.
		bool Chronicle_(Hash64 TypeHash);

	private:
		World* Owner;
		HistoryTypes Types;
		WorldGen::RegionTypes Regions;
	};

	// ── Queries ──────────────────────────────────────────────────────────────
	/// Index of the era open at Tick (0 when none was founded yet or Tick precedes it).
	VAELEN_SIM_API uint32 EraAt(const World& W, const HistoryTypes& Types, uint64 Tick);
	/// The event with this id, or nullptr.
	VAELEN_SIM_API const Event* FindEvent(const EventLog& Log, PersistentId Id) noexcept;
	/// The cause chain from Id back to its root cause (Id first). Stops at a
	/// missing link or after MaxDepth entries.
	VAELEN_SIM_API void CauseChain(const EventLog& Log, PersistentId Id, std::vector<const Event*>& Out,
								   uint32 MaxDepth = 64);
	/// Events whose tick falls inside the era, in log order.
	VAELEN_SIM_API void EventsInEra(const World& W, const HistoryTypes& Types, uint32 EraIndex,
									std::vector<const Event*>& Out);
	/// Events about a subject, in log order.
	VAELEN_SIM_API void EventsAbout(const EventLog& Log, PersistentId Subject, std::vector<const Event*>& Out);
} // namespace Vaelen::History
