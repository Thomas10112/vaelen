// VAELEN - VaelenSim
// The registry of live entities: dense slots, generations, and the
// bidirectional mapping between PersistentId and EntityHandle.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// Determinism: every operation is a pure function of the sequence of calls.
// Slots are recycled through a LIFO free list, so two registries fed the same
// operations produce the same handles. Iteration (ForEachAlive, GetState) is
// always in slot-index order; the PersistentId lookup table is an
// unordered_map used for lookups only and is never iterated.
//
// Not thread-safe by design: entities are created and destroyed on the
// simulation thread in a deterministic order.
//
// A slot whose generation reaches MaxGeneration is retired (never reused), so
// a stale handle can never become valid again.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/EntityHandle.h"
#include "Vaelen/Sim/SimApi.h"

#include <unordered_map>
#include <vector>

namespace Vaelen
{
	class VAELEN_SIM_API EntityRegistry
	{
	public:
		/// One slot of the dense table. Part of the snapshot state.
		struct Slot
		{
			PersistentId Id;		  ///< Invalid when the slot is free or retired.
			uint32 Generation = 0;	  ///< FirstGeneration.. for live/free slots.
			uint32 NextFree = NoSlot; ///< Free-list link (free slots only).
			bool Alive = false;
			bool Retired = false;

			bool operator==(const Slot&) const noexcept = default;
		};

		/// Snapshot state: restores handles exactly (01.06 serialises it).
		struct State
		{
			std::vector<Slot> Slots;
			uint32 FreeHead = NoSlot;
			uint32 AliveCount = 0;

			bool operator==(const State&) const noexcept = default;
		};

		static constexpr uint32 NoSlot = 0xFFFFFFFFu;

		EntityRegistry() = default;

		/// Registers an entity under a persistent id the caller allocated
		/// (normally from the world's IdAllocator). Returns Null() and reports
		/// a Check failure if the id is invalid or already registered, or if
		/// the slot space is exhausted.
		EntityHandle Create(PersistentId Id);

		/// Convenience: allocates a fresh id of the given kind and registers it.
		EntityHandle Create(IdAllocator& Ids, IdKind Kind);

		/// Destroys a live entity. Returns false (no change) for a null, stale,
		/// out-of-range or already-destroyed handle.
		bool Destroy(EntityHandle Handle) noexcept;

		/// True when the handle refers to a live entity (index in range and
		/// generation matches).
		bool IsAlive(EntityHandle Handle) const noexcept;

		/// Persistent id of a live entity; Invalid() for anything else.
		PersistentId GetId(EntityHandle Handle) const noexcept;

		/// Handle of the live entity registered under an id; Null() if none.
		EntityHandle Find(PersistentId Id) const noexcept;

		uint32 GetAliveCount() const noexcept { return Current.AliveCount; }
		uint32 GetSlotCount() const noexcept { return static_cast<uint32>(Current.Slots.size()); }
		uint32 GetFreeCount() const noexcept;

		/// Calls Visitor(EntityHandle, PersistentId) for every live entity, in
		/// slot-index order. Visitor must not create or destroy entities.
		template <typename Visitor>
		void ForEachAlive(Visitor&& Visit) const
		{
			for (uint32 i = 0; i < static_cast<uint32>(Current.Slots.size()); ++i)
			{
				const Slot& S = Current.Slots[i];
				if (S.Alive)
				{
					Visit(EntityHandle::Make(i, S.Generation), S.Id);
				}
			}
		}

		const State& GetState() const noexcept { return Current; }

		/// Restores a state and rebuilds the id lookup. Returns false (and
		/// leaves the registry empty) when the state is inconsistent: duplicate
		/// ids, a broken free list, or counters that do not match the slots.
		bool SetState(const State& InState);

		void Clear() noexcept;

	private:
		bool SlotMatches(EntityHandle Handle, const Slot*& OutSlot) const noexcept;

		State Current;
		std::unordered_map<PersistentId, uint32> IndexById;
	};
} // namespace Vaelen
