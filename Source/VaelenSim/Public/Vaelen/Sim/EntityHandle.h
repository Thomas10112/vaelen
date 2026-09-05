// VAELEN - VaelenSim
// Runtime entity handle: a dense slot index plus a generation counter.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// Two identities exist for an entity:
//   - PersistentId (Vaelen/Core/Ids.h): the identity that is saved, never
//     reused and used in every record, event and reference that outlives a
//     frame.
//   - EntityHandle (this file): a runtime accessor into the EntityRegistry's
//     dense slot table. Cheap to compare, cheap to index components with, and
//     safe: a handle to a destroyed entity is detected by its generation.
//     Handles are NOT persisted; a snapshot restores them from the registry.
//
// Layout (most significant bit first):
//   [ 32 bits : generation ][ 32 bits : slot index ]
// The null handle is Value 0 (index 0, generation 0); live generations start
// at 1, so no live entity ever has that value.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"

#include <compare>
#include <functional>

namespace Vaelen
{
	struct EntityHandle
	{
		static constexpr uint32 MaxIndex = 0xFFFFFFFEu; ///< Slot indices are 0..MaxIndex.
		static constexpr uint32 FirstGeneration = 1;
		static constexpr uint32 MaxGeneration = 0xFFFFFFFFu;

		uint64 Value = 0;

		constexpr EntityHandle() noexcept = default;
		constexpr explicit EntityHandle(uint64 InValue) noexcept : Value(InValue) {}

		static constexpr EntityHandle Make(uint32 Index, uint32 Generation) noexcept
		{
			return EntityHandle((static_cast<uint64>(Generation) << 32) | static_cast<uint64>(Index));
		}

		static constexpr EntityHandle Null() noexcept { return EntityHandle(); }

		constexpr uint32 Index() const noexcept { return static_cast<uint32>(Value & 0xFFFFFFFFull); }
		constexpr uint32 Generation() const noexcept { return static_cast<uint32>(Value >> 32); }

		/// A handle that was never produced by a registry. Whether a non-null
		/// handle still refers to a live entity is answered by the registry.
		constexpr bool IsNull() const noexcept { return Value == 0; }

		constexpr explicit operator bool() const noexcept { return !IsNull(); }
		constexpr bool operator==(const EntityHandle&) const noexcept = default;
		constexpr auto operator<=>(const EntityHandle&) const noexcept = default;

		constexpr Hash64 Hash() const noexcept { return HashUInt64(Value); }
	};

	static_assert(sizeof(EntityHandle) == 8, "EntityHandle must stay 64-bit");

	/// Hash entry point for Unreal containers (found by ADL).
	constexpr uint32 GetTypeHash(EntityHandle Handle) noexcept
	{
		const Hash64 H = Handle.Hash();
		return static_cast<uint32>(H ^ (H >> 32));
	}
} // namespace Vaelen

namespace std
{
	template <>
	struct hash<::Vaelen::EntityHandle>
	{
		size_t operator()(const ::Vaelen::EntityHandle& Handle) const noexcept
		{
			return static_cast<size_t>(Handle.Hash());
		}
	};
} // namespace std
