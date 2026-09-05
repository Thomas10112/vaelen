// VAELEN - VaelenSim
// All component pools of a world, addressed by ComponentType<T>.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// The store owns one pool per registered component type that has been
// created with CreatePool. Pools are addressed by the typed id returned by
// ComponentTypeRegistry::Register<T>, so no RTTI is needed and a mismatch
// between T and the id is impossible at the call site. RemoveAll(handle)
// visits pools in type-id order: the order of cleanup is deterministic.
#pragma once

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/ComponentPool.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/EntityHandle.h"
#include "Vaelen/Sim/SimApi.h"

#include <memory>
#include <vector>

namespace Vaelen
{
	class VAELEN_SIM_API ComponentStore
	{
	public:
		explicit ComponentStore(const ComponentTypeRegistry& InTypes) noexcept : Types(&InTypes) {}

		/// Creates the pool of a registered type. Creating it twice or for an
		/// unregistered id is a Check failure; the existing pool (or a scratch
		/// pool that is not part of the store) is returned.
		template <typename T>
		ComponentPool<T>& CreatePool(ComponentType<T> Type)
		{
			const bool Registered = Types->IsValid(Type.Id);
			VAELEN_CHECKF(Registered, "component type %u is not registered", unsigned{Type.Id});
			if (!Registered)
			{
				return ScratchPool<T>();
			}
			const ComponentTypeInfo& Info = Types->GetInfo(Type.Id);
			const bool LayoutMatches = Info.Size == sizeof(T) && Info.Alignment == alignof(T);
			VAELEN_CHECKF(LayoutMatches, "component type '%s' was registered with another layout", Info.Name);
			if (!LayoutMatches)
			{
				return ScratchPool<T>();
			}
			if (Type.Id >= Pools.size())
			{
				Pools.resize(static_cast<usize>(Type.Id) + 1);
			}
			VAELEN_CHECKF(Pools[Type.Id] == nullptr, "pool for component type '%s' already exists", Info.Name);
			if (Pools[Type.Id] != nullptr)
			{
				return *static_cast<ComponentPool<T>*>(Pools[Type.Id].get());
			}
			Pools[Type.Id] = std::make_unique<ComponentPool<T>>(Type);
			return *static_cast<ComponentPool<T>*>(Pools[Type.Id].get());
		}

		bool HasPool(ComponentTypeId Id) const noexcept { return Id < Pools.size() && Pools[Id] != nullptr; }

		/// The pool of a type whose pool was created (Check failure and a
		/// scratch pool otherwise).
		template <typename T>
		ComponentPool<T>& GetPool(ComponentType<T> Type) noexcept
		{
			VAELEN_CHECKF(HasPool(Type.Id), "no pool for component type %u", unsigned{Type.Id});
			return HasPool(Type.Id) ? *static_cast<ComponentPool<T>*>(Pools[Type.Id].get()) : ScratchPool<T>();
		}

		template <typename T>
		const ComponentPool<T>& GetPool(ComponentType<T> Type) const noexcept
		{
			VAELEN_CHECKF(HasPool(Type.Id), "no pool for component type %u", unsigned{Type.Id});
			return HasPool(Type.Id) ? *static_cast<const ComponentPool<T>*>(Pools[Type.Id].get()) : ScratchPool<T>();
		}

		IComponentPool* GetPoolBase(ComponentTypeId Id) noexcept { return HasPool(Id) ? Pools[Id].get() : nullptr; }

		/// Removes every component of an entity, pools in type-id order.
		/// Returns the number of components removed.
		uint32 RemoveAll(EntityHandle Handle) noexcept;

		/// Number of pools that hold a component for the entity.
		uint32 CountComponents(EntityHandle Handle) const noexcept;

		uint32 PoolCount() const noexcept;
		void ClearAll() noexcept;

		const ComponentTypeRegistry& GetTypes() const noexcept { return *Types; }

	private:
		template <typename T>
		static ComponentPool<T>& ScratchPool() noexcept
		{
			static ComponentPool<T> Pool{ComponentType<T>{}};
			Pool.Clear();
			return Pool;
		}

		const ComponentTypeRegistry* Types;
		std::vector<std::unique_ptr<IComponentPool>> Pools;
	};
} // namespace Vaelen
