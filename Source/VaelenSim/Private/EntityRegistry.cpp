// VAELEN - VaelenSim
// Dense slot table with LIFO free list and generation counters.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_EntityRegistry.cpp
#include "Vaelen/Sim/EntityRegistry.h"
#include "Vaelen/Core/Assert.h"

namespace Vaelen
{
	EntityHandle EntityRegistry::Create(PersistentId Id)
	{
		VAELEN_CHECKF(Id.IsValid(), "EntityRegistry::Create requires a valid PersistentId");
		if (!Id.IsValid())
		{
			return EntityHandle::Null();
		}
		VAELEN_CHECKF(IndexById.find(Id) == IndexById.end(), "PersistentId %llu is already registered",
					  static_cast<unsigned long long>(Id.Value));
		if (IndexById.find(Id) != IndexById.end())
		{
			return EntityHandle::Null();
		}

		uint32 Index = NoSlot;
		if (Current.FreeHead != NoSlot)
		{
			Index = Current.FreeHead;
			Slot& S = Current.Slots[Index];
			Current.FreeHead = S.NextFree;
			S.NextFree = NoSlot;
		}
		else
		{
			VAELEN_CHECKF(Current.Slots.size() <= EntityHandle::MaxIndex, "EntityRegistry slot space exhausted");
			if (Current.Slots.size() > EntityHandle::MaxIndex)
			{
				return EntityHandle::Null();
			}
			Index = static_cast<uint32>(Current.Slots.size());
			Slot Fresh;
			Fresh.Generation = EntityHandle::FirstGeneration;
			Current.Slots.push_back(Fresh);
		}

		Slot& S = Current.Slots[Index];
		S.Id = Id;
		S.Alive = true;
		++Current.AliveCount;
		IndexById.emplace(Id, Index);
		return EntityHandle::Make(Index, S.Generation);
	}

	EntityHandle EntityRegistry::Create(IdAllocator& Ids, IdKind Kind)
	{
		return Create(Ids.Allocate(Kind));
	}

	bool EntityRegistry::SlotMatches(EntityHandle Handle, const Slot*& OutSlot) const noexcept
	{
		OutSlot = nullptr;
		if (Handle.IsNull())
		{
			return false;
		}
		const uint32 Index = Handle.Index();
		if (Index >= Current.Slots.size())
		{
			return false;
		}
		const Slot& S = Current.Slots[Index];
		if (!S.Alive || S.Generation != Handle.Generation())
		{
			return false;
		}
		OutSlot = &S;
		return true;
	}

	bool EntityRegistry::Destroy(EntityHandle Handle) noexcept
	{
		const Slot* Found = nullptr;
		if (!SlotMatches(Handle, Found))
		{
			return false;
		}
		const uint32 Index = Handle.Index();
		Slot& S = Current.Slots[Index];
		IndexById.erase(S.Id);
		S.Id = PersistentId::Invalid();
		S.Alive = false;
		--Current.AliveCount;
		if (S.Generation == EntityHandle::MaxGeneration)
		{
			// The generation space of this slot is spent: retire it so that no
			// stale handle can ever match again.
			S.Retired = true;
			S.NextFree = NoSlot;
			return true;
		}
		++S.Generation;
		S.NextFree = Current.FreeHead;
		Current.FreeHead = Index;
		return true;
	}

	bool EntityRegistry::IsAlive(EntityHandle Handle) const noexcept
	{
		const Slot* Found = nullptr;
		return SlotMatches(Handle, Found);
	}

	PersistentId EntityRegistry::GetId(EntityHandle Handle) const noexcept
	{
		const Slot* Found = nullptr;
		return SlotMatches(Handle, Found) ? Found->Id : PersistentId::Invalid();
	}

	EntityHandle EntityRegistry::Find(PersistentId Id) const noexcept
	{
		if (!Id.IsValid())
		{
			return EntityHandle::Null();
		}
		const auto It = IndexById.find(Id);
		if (It == IndexById.end())
		{
			return EntityHandle::Null();
		}
		return EntityHandle::Make(It->second, Current.Slots[It->second].Generation);
	}

	uint32 EntityRegistry::GetFreeCount() const noexcept
	{
		uint32 Count = 0;
		for (uint32 Index = Current.FreeHead; Index != NoSlot; Index = Current.Slots[Index].NextFree)
		{
			++Count;
		}
		return Count;
	}

	bool EntityRegistry::SetState(const State& InState)
	{
		Clear();

		std::unordered_map<PersistentId, uint32> Lookup;
		uint32 Alive = 0;
		uint32 FreeSlots = 0;
		for (uint32 i = 0; i < static_cast<uint32>(InState.Slots.size()); ++i)
		{
			const Slot& S = InState.Slots[i];
			if (S.Alive)
			{
				if (!S.Id.IsValid() || S.Retired || S.Generation < EntityHandle::FirstGeneration ||
					!Lookup.emplace(S.Id, i).second)
				{
					return false;
				}
				++Alive;
			}
			else
			{
				if (S.Id.IsValid())
				{
					return false;
				}
				if (!S.Retired)
				{
					++FreeSlots;
				}
			}
		}
		if (Alive != InState.AliveCount)
		{
			return false;
		}

		// Walk the free list: every free slot exactly once, no cycles, no live
		// or retired slot on it.
		uint32 Walked = 0;
		for (uint32 Index = InState.FreeHead; Index != NoSlot; Index = InState.Slots[Index].NextFree)
		{
			if (Index >= InState.Slots.size() || InState.Slots[Index].Alive || InState.Slots[Index].Retired ||
				++Walked > FreeSlots)
			{
				return false;
			}
		}
		if (Walked != FreeSlots)
		{
			return false;
		}

		Current = InState;
		IndexById = std::move(Lookup);
		return true;
	}

	void EntityRegistry::Clear() noexcept
	{
		Current = State{};
		IndexById.clear();
	}
} // namespace Vaelen
