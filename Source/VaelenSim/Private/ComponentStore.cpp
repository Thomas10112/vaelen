// VAELEN - VaelenSim
// Per-entity cleanup across pools.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_ComponentStore.cpp
#include "Vaelen/Sim/ComponentStore.h"

namespace Vaelen
{
	uint32 ComponentStore::RemoveAll(EntityHandle Handle) noexcept
	{
		uint32 Removed = 0;
		for (std::unique_ptr<IComponentPool>& Pool : Pools)
		{
			if (Pool != nullptr && Pool->Remove(Handle))
			{
				++Removed;
			}
		}
		return Removed;
	}

	uint32 ComponentStore::CountComponents(EntityHandle Handle) const noexcept
	{
		uint32 Count = 0;
		for (const std::unique_ptr<IComponentPool>& Pool : Pools)
		{
			if (Pool != nullptr && Pool->Has(Handle))
			{
				++Count;
			}
		}
		return Count;
	}

	uint32 ComponentStore::PoolCount() const noexcept
	{
		uint32 Count = 0;
		for (const std::unique_ptr<IComponentPool>& Pool : Pools)
		{
			Count += Pool != nullptr ? 1u : 0u;
		}
		return Count;
	}

	void ComponentStore::ClearAll() noexcept
	{
		for (std::unique_ptr<IComponentPool>& Pool : Pools)
		{
			if (Pool != nullptr)
			{
				Pool->Clear();
			}
		}
	}
} // namespace Vaelen
