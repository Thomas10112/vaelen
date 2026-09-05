// VAELEN - VaelenSim
// Event log (append-only history with a running digest) and event bus
// (publish now, deliver at the next tick, in publish order).
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// Determinism:
//   - Events are delivered in publish order, one tick after publication
//     (Dispatch(tick) delivers everything published before that tick).
//     Events published while dispatching are queued for the next tick.
//   - Listeners are ordered by the hash of their listener name, not by
//     subscription order, so the delivery order is a function of the set
//     of listeners.
//   - The log's digest is a running hash over every event in order; two
//     worlds with equal digests have identical histories.
// The log is unbounded by design in Phase 01 (it IS the history); tiering
// and compaction are Phase 16/17 concerns.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/SimClock.h"

#include <string_view>
#include <vector>

namespace Vaelen
{
	class VAELEN_SIM_API EventLog
	{
	public:
		static constexpr Hash64 EmptyDigest = 0x5641454c454e2d45ull; // "VAELEN-E"

		void Append(const Event& E);
		uint64 Count() const noexcept { return Events.size(); }
		const Event& At(uint64 Index) const noexcept { return Events[static_cast<usize>(Index)]; }
		const std::vector<Event>& All() const noexcept { return Events; }
		Hash64 Digest() const noexcept { return RunningDigest; }
		void Clear() noexcept;

		/// Raw byte image: [uint64 count][uint64 digest][Event * count].
		void WriteTo(std::vector<uint8>& Out) const;

		/// Replaces the log from a byte image. Returns false (log left empty)
		/// on a size mismatch or when the recomputed digest differs.
		bool ReadFrom(const uint8* Bytes, usize Size);

		/// Number of events whose Cause is the given event.
		uint64 CountCausedBy(PersistentId Cause) const noexcept;

	private:
		std::vector<Event> Events;
		Hash64 RunningDigest = EmptyDigest;
	};

	class VAELEN_SIM_API IEventListener
	{
	public:
		virtual ~IEventListener() = default;
		/// Stable name; decides delivery order among listeners of one type.
		virtual const char* GetListenerName() const noexcept = 0;
		virtual void OnEvent(const Event& E) = 0;
	};

	class VAELEN_SIM_API EventBus
	{
	public:
		/// Ids come from the world allocator (kind Event); the log is shared
		/// with the world so that every published event is recorded.
		EventBus(IdAllocator& InIds, EventLog& InLog) noexcept : Ids(&InIds), Log(&InLog) {}

		/// Publishes an event at the given tick. Returns its id.
		template <typename T>
		PersistentId Publish(SimTick Tick, EventType<T> Type, const T& Payload, PersistentId Subject = {},
							 PersistentId Cause = {})
		{
			Event E{};
			E.Id = Ids->Allocate(IdKind::Event);
			E.Tick = Tick;
			E.TypeHash = Type.TypeHash;
			E.Cause = Cause;
			E.Subject = Subject;
			E.Set(Payload);
			Enqueue(E);
			return E.Id;
		}

		PersistentId Publish(SimTick Tick, EventType<NoPayload> Type, PersistentId Subject = {},
							 PersistentId Cause = {})
		{
			return Publish<NoPayload>(Tick, Type, NoPayload{}, Subject, Cause);
		}

		/// Subscribes a listener to one event type. Returns false (and
		/// reports) when the pair is already subscribed.
		bool Subscribe(Hash64 TypeHash, IEventListener* Listener);
		bool Unsubscribe(Hash64 TypeHash, IEventListener* Listener) noexcept;
		uint32 SubscriberCount(Hash64 TypeHash) const noexcept;

		/// Delivers every pending event published before Tick, in publish
		/// order, to the listeners of its type (ordered by listener name
		/// hash). Events published during delivery wait for the next tick.
		/// Returns the number of events delivered.
		uint32 Dispatch(SimTick Tick);

		uint64 PendingCount() const noexcept { return Pending.size(); }
		bool IsDispatching() const noexcept { return Dispatching; }
		const EventLog& GetLog() const noexcept { return *Log; }

	private:
		struct Subscription
		{
			Hash64 TypeHash = 0;
			Hash64 ListenerHash = 0;
			IEventListener* Listener = nullptr;
		};

		void Enqueue(const Event& E);

		IdAllocator* Ids;
		EventLog* Log;
		std::vector<Event> Pending;
		std::vector<Event> Deferred;			 ///< published during Dispatch
		std::vector<Subscription> Subscriptions; ///< kept sorted by (type, listener hash, name)
		bool Dispatching = false;
	};
} // namespace Vaelen
