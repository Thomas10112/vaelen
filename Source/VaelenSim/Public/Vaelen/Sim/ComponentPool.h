// VAELEN - VaelenSim
// Typed sparse-set storage of one component type: dense data, dense entity
// list, sparse index by entity slot.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// Determinism: the dense order is a pure function of the sequence of Add and
// Remove calls (Remove is swap-with-last). It is NOT slot order; systems that
// need a canonical order sort by PersistentId or iterate the registry.
//
// Stale handles: the dense list stores full handles (index + generation), so a
// handle from a previous generation of the same slot is never a match. Adding
// a component for a new generation while a stale entry from the old one is
// still present replaces the entry and reports it with VAELEN_ENSURE: the
// owner is expected to call ComponentStore::RemoveAll before destroying an
// entity.
#pragma once

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/Archive.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/EntityHandle.h"

#include <vector>

namespace Vaelen
{
	/// Type-erased view used by ComponentStore for per-entity cleanup.
	class IComponentPool
	{
	public:
		virtual ~IComponentPool() = default;
		virtual ComponentTypeId GetTypeId() const noexcept = 0;
		virtual bool Has(EntityHandle Handle) const noexcept = 0;
		virtual bool Remove(EntityHandle Handle) noexcept = 0;
		virtual uint32 Size() const noexcept = 0;
		virtual void Clear() noexcept = 0;
		/// Saves or loads the dense state (count, entities, raw data). Returns
		/// false when loading failed (pool left empty).
		virtual bool Serialize(IArchive& Ar) = 0;
		/// Size in bytes of one element, for layout checks when loading.
		virtual uint32 ElementSize() const noexcept = 0;
	};

	template <typename T>
	class ComponentPool final : public IComponentPool
	{
	public:
		static constexpr uint32 NoEntry = 0xFFFFFFFFu;

		/// Snapshot state: the dense arrays; the sparse index is rebuilt.
		struct State
		{
			std::vector<EntityHandle> Entities;
			std::vector<T> Data;
		};

		explicit ComponentPool(ComponentType<T> InType) noexcept : Type(InType) {}

		ComponentTypeId GetTypeId() const noexcept override { return Type.Id; }
		uint32 Size() const noexcept override { return static_cast<uint32>(DenseEntities.size()); }
		bool IsEmpty() const noexcept { return DenseEntities.empty(); }

		/// Adds a component. Adding to an entity that already has one is a
		/// Check failure and returns the existing component unchanged.
		T& Add(EntityHandle Handle, const T& Value = T{})
		{
			VAELEN_CHECKF(!Handle.IsNull(), "cannot add a component to the null handle");
			if (Handle.IsNull())
			{
				return Scratch;
			}
			const uint32 Index = Handle.Index();
			if (Index >= Sparse.size())
			{
				Sparse.resize(static_cast<usize>(Index) + 1, NoEntry);
			}
			const uint32 Dense = Sparse[Index];
			if (Dense != NoEntry)
			{
				if (DenseEntities[Dense] == Handle)
				{
					VAELEN_CHECKF(false, "entity already has this component (type %u)", unsigned{Type.Id});
					return DenseData[Dense];
				}
				// Stale entry from a destroyed entity of the same slot: replace it.
				(void)VAELEN_ENSURE(DenseEntities[Dense].Generation() == Handle.Generation());
				DenseEntities[Dense] = Handle;
				DenseData[Dense] = Value;
				return DenseData[Dense];
			}
			Sparse[Index] = static_cast<uint32>(DenseEntities.size());
			DenseEntities.push_back(Handle);
			DenseData.push_back(Value);
			return DenseData.back();
		}

		bool Has(EntityHandle Handle) const noexcept override { return FindDense(Handle) != NoEntry; }

		T* TryGet(EntityHandle Handle) noexcept
		{
			const uint32 Dense = FindDense(Handle);
			return Dense == NoEntry ? nullptr : &DenseData[Dense];
		}

		const T* TryGet(EntityHandle Handle) const noexcept
		{
			const uint32 Dense = FindDense(Handle);
			return Dense == NoEntry ? nullptr : &DenseData[Dense];
		}

		/// The component of an entity that must have one (Check failure and a
		/// scratch value otherwise).
		T& Get(EntityHandle Handle) noexcept
		{
			T* Found = TryGet(Handle);
			VAELEN_CHECKF(Found != nullptr, "entity has no component of type %u", unsigned{Type.Id});
			return Found != nullptr ? *Found : Scratch;
		}

		/// Removes by swapping the last dense entry into the hole. Returns
		/// false when the entity has no component.
		bool Remove(EntityHandle Handle) noexcept override
		{
			const uint32 Dense = FindDense(Handle);
			if (Dense == NoEntry)
			{
				return false;
			}
			const uint32 Last = static_cast<uint32>(DenseEntities.size() - 1);
			if (Dense != Last)
			{
				DenseEntities[Dense] = DenseEntities[Last];
				DenseData[Dense] = DenseData[Last];
				Sparse[DenseEntities[Dense].Index()] = Dense;
			}
			Sparse[Handle.Index()] = NoEntry;
			DenseEntities.pop_back();
			DenseData.pop_back();
			return true;
		}

		void Clear() noexcept override
		{
			Sparse.clear();
			DenseEntities.clear();
			DenseData.clear();
		}

		EntityHandle EntityAt(uint32 Dense) const noexcept { return DenseEntities[Dense]; }
		T& DataAt(uint32 Dense) noexcept { return DenseData[Dense]; }
		const T& DataAt(uint32 Dense) const noexcept { return DenseData[Dense]; }
		const std::vector<EntityHandle>& Entities() const noexcept { return DenseEntities; }
		const std::vector<T>& Data() const noexcept { return DenseData; }
		std::vector<T>& Data() noexcept { return DenseData; }

		/// Visit(EntityHandle, T&) in dense order. Visit must not add or remove.
		template <typename Visitor>
		void ForEach(Visitor&& Visit)
		{
			for (usize i = 0; i < DenseEntities.size(); ++i)
			{
				Visit(DenseEntities[i], DenseData[i]);
			}
		}

		template <typename Visitor>
		void ForEach(Visitor&& Visit) const
		{
			for (usize i = 0; i < DenseEntities.size(); ++i)
			{
				Visit(DenseEntities[i], DenseData[i]);
			}
		}

		State GetState() const { return State{DenseEntities, DenseData}; }

		bool Serialize(IArchive& Ar) override
		{
			if (Ar.IsSaving())
			{
				std::vector<EntityHandle> Entities = DenseEntities;
				std::vector<T> Values = DenseData;
				SerializeVector(Ar, Entities);
				SerializeVector(Ar, Values);
				return true;
			}
			State Loaded;
			if (!SerializeVector(Ar, Loaded.Entities) || !SerializeVector(Ar, Loaded.Data))
			{
				Clear();
				return false;
			}
			return SetState(Loaded);
		}

		uint32 ElementSize() const noexcept override { return static_cast<uint32>(sizeof(T)); }

		/// Restores the dense arrays and rebuilds the sparse index. Returns
		/// false (pool left empty) on mismatched sizes, null handles or
		/// duplicate slots.
		bool SetState(const State& InState)
		{
			Clear();
			if (InState.Entities.size() != InState.Data.size())
			{
				return false;
			}
			std::vector<uint32> NewSparse;
			for (usize i = 0; i < InState.Entities.size(); ++i)
			{
				const EntityHandle Handle = InState.Entities[i];
				if (Handle.IsNull())
				{
					return false;
				}
				const uint32 Index = Handle.Index();
				if (Index >= NewSparse.size())
				{
					NewSparse.resize(static_cast<usize>(Index) + 1, NoEntry);
				}
				if (NewSparse[Index] != NoEntry)
				{
					return false;
				}
				NewSparse[Index] = static_cast<uint32>(i);
			}
			Sparse = std::move(NewSparse);
			DenseEntities = InState.Entities;
			DenseData = InState.Data;
			return true;
		}

	private:
		uint32 FindDense(EntityHandle Handle) const noexcept
		{
			if (Handle.IsNull())
			{
				return NoEntry;
			}
			const uint32 Index = Handle.Index();
			if (Index >= Sparse.size())
			{
				return NoEntry;
			}
			const uint32 Dense = Sparse[Index];
			if (Dense == NoEntry || DenseEntities[Dense] != Handle)
			{
				return NoEntry;
			}
			return Dense;
		}

		ComponentType<T> Type;
		std::vector<uint32> Sparse;
		std::vector<EntityHandle> DenseEntities;
		std::vector<T> DenseData;
		T Scratch{}; ///< Returned after a failed Check so callers never dereference null.
	};
} // namespace Vaelen
