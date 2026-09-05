// VAELEN - VaelenSim
// Component type identities: an explicit, ordered registry (no RTTI, no
// static-initialisation counters whose order would depend on link order).
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// A world registers its component types once, in a fixed order, at setup:
//   ComponentTypeRegistry Types;
//   const ComponentType<Position> PositionType = Types.Register<Position>("Position");
// The id is the registration index; the name hash is the stable identity that
// snapshots and mods refer to (ids may differ between two worlds that register
// different type sets, name hashes never do).
//
// Components are plain data: trivially copyable, no pointers to other
// components, no owning resources. That is what makes them serialisable as
// raw bytes (01.06) and comparable between replays (01.07).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/SimApi.h"

#include <string_view>
#include <type_traits>
#include <vector>

namespace Vaelen
{
	using ComponentTypeId = uint16;
	inline constexpr ComponentTypeId InvalidComponentTypeId = 0xFFFFu;
	inline constexpr uint32 MaxComponentTypes = 4096;

	/// A typed component id: carries the type at compile time so that pools are
	/// looked up without RTTI and without the caller repeating the type.
	template <typename T>
	struct ComponentType
	{
		static_assert(std::is_trivially_copyable_v<T>, "components must be trivially copyable plain data");
		static_assert(std::is_default_constructible_v<T>, "components must be default constructible");

		ComponentTypeId Id = InvalidComponentTypeId;

		constexpr bool IsValid() const noexcept { return Id != InvalidComponentTypeId; }
	};

	struct ComponentTypeInfo
	{
		const char* Name = "";
		Hash64 NameHash = 0;
		uint32 Size = 0;
		uint32 Alignment = 0;
	};

	class VAELEN_SIM_API ComponentTypeRegistry
	{
	public:
		/// Registers a type under a unique name. Returns an invalid id (and a
		/// Check failure) for an empty or duplicate name or when the registry
		/// is full. Names must be string literals or otherwise outlive the registry.
		template <typename T>
		ComponentType<T> Register(const char* Name)
		{
			ComponentType<T> Type;
			Type.Id = RegisterRaw(Name, static_cast<uint32>(sizeof(T)), static_cast<uint32>(alignof(T)));
			return Type;
		}

		ComponentTypeId FindByName(std::string_view Name) const noexcept;
		ComponentTypeId FindByHash(Hash64 NameHash) const noexcept;

		const ComponentTypeInfo& GetInfo(ComponentTypeId Id) const noexcept;
		bool IsValid(ComponentTypeId Id) const noexcept { return Id < Infos.size(); }
		uint32 Count() const noexcept { return static_cast<uint32>(Infos.size()); }

		/// Order-sensitive digest of the registered set (names, sizes): two
		/// worlds with equal digests have interchangeable component layouts.
		Hash64 LayoutDigest() const noexcept;

	private:
		ComponentTypeId RegisterRaw(const char* Name, uint32 Size, uint32 Alignment);

		std::vector<ComponentTypeInfo> Infos;
	};
} // namespace Vaelen
