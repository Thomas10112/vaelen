// VAELEN - VaelenCore
// Persistent identifiers.
//
// STATUS: VALIDATED (Phase 00)
//
// PersistentId is the identity of anything that outlives a frame: a person,
// a family, a settlement, an item, an event. It is:
//   - 64-bit, trivially copyable and serialisable as-is;
//   - deterministic: allocated from monotonic per-kind counters that are
//     part of the world state (same seed + same inputs => same ids);
//   - never reused: an id refers to the same thing forever, even after death
//     or destruction (history must remain addressable);
//   - self-describing: the top 8 bits carry the IdKind.
//
// Layout (most significant bit first):
//   [ 8 bits : IdKind ][ 56 bits : serial, starting at 1 ]
//
// Runtime slot handles with generation counters (for dense component storage)
// are a separate concept introduced in Phase 01 - CORE SIMULATION.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"

#include <array>
#include <compare>
#include <functional>

namespace Vaelen
{
	/// Kinds of persistent things. Values are part of the save format: append
	/// only, never renumber, never delete (deprecate instead).
	enum class IdKind : uint8
	{
		None = 0,

		// ── Core / simulation (Phase 01) ──
		Entity = 1,
		Event = 2,

		// ── World (Phase 02) ──
		Region = 10,
		Tile = 11,
		River = 12,
		ResourceDeposit = 13,

		// ── History & society (Phases 03-05) ──
		Culture = 20,
		Language = 21,
		Religion = 22,
		Person = 23,
		Family = 24,
		Organization = 25,

		// ── Economy & infrastructure (Phases 06, 09) ──
		Item = 30,
		Building = 31,
		Settlement = 32,
		Market = 33,
		Route = 34,

		// ── Politics & military (Phases 07-08) ──
		Polity = 40,
		Law = 41,
		Army = 42,
		War = 43,

		// ── Knowledge (Phase 12) ──
		Document = 50,
		Map = 51,

		MaxValue = 255,
	};

	VAELENCORE_API const char* IdKindToString(IdKind Kind) noexcept;

	struct PersistentId
	{
		static constexpr uint32 KindBits = 8;
		static constexpr uint32 SerialBits = 56;
		static constexpr uint64 SerialMask = (uint64{1} << SerialBits) - 1;
		static constexpr uint64 MaxSerial = SerialMask;

		uint64 Value = 0;

		constexpr PersistentId() noexcept = default;
		constexpr explicit PersistentId(uint64 InValue) noexcept : Value(InValue) {}

		static constexpr PersistentId Make(IdKind Kind, uint64 Serial) noexcept
		{
			return PersistentId((static_cast<uint64>(ToUnderlying(Kind)) << SerialBits) | (Serial & SerialMask));
		}

		static constexpr PersistentId Invalid() noexcept { return PersistentId(); }

		constexpr IdKind Kind() const noexcept { return static_cast<IdKind>(Value >> SerialBits); }
		constexpr uint64 Serial() const noexcept { return Value & SerialMask; }
		constexpr bool IsValid() const noexcept { return Kind() != IdKind::None && Serial() != 0; }
		constexpr bool IsKind(IdKind InKind) const noexcept { return Kind() == InKind; }

		constexpr explicit operator bool() const noexcept { return IsValid(); }
		constexpr bool operator==(const PersistentId&) const noexcept = default;
		constexpr auto operator<=>(const PersistentId&) const noexcept = default;

		constexpr Hash64 Hash() const noexcept { return HashUInt64(Value); }
	};

	static_assert(sizeof(PersistentId) == 8, "PersistentId must stay 64-bit");

	/// Deterministic allocator of PersistentIds: one monotonic counter per
	/// kind. Its state is part of the world state and must be saved/loaded
	/// with it. Not thread-safe by design: allocation happens on the
	/// simulation thread in a deterministic order.
	class VAELENCORE_API IdAllocator
	{
	public:
		static constexpr usize KindCount = 256;

		/// Serialisable state: the next serial for each kind.
		struct State
		{
			std::array<uint64, KindCount> NextSerial{};
		};

		IdAllocator() noexcept;

		/// Allocates the next id of the given kind. Kind must not be None.
		PersistentId Allocate(IdKind Kind) noexcept;

		/// Number of ids allocated so far for a kind.
		uint64 GetAllocatedCount(IdKind Kind) const noexcept;

		/// Peeks the id that the next Allocate(Kind) would return.
		PersistentId PeekNext(IdKind Kind) const noexcept;

		/// Marks a serial as used (e.g. when importing ids); subsequent
		/// allocations of that kind start above it. Returns false if the
		/// serial is already below the current counter (no change).
		bool ReserveUpTo(IdKind Kind, uint64 Serial) noexcept;

		const State& GetState() const noexcept { return Current; }
		void SetState(const State& InState) noexcept;
		void Reset() noexcept;

	private:
		State Current;
	};
} // namespace Vaelen

namespace std
{
	template <>
	struct hash<::Vaelen::PersistentId>
	{
		size_t operator()(const ::Vaelen::PersistentId& Id) const noexcept
		{
			return static_cast<size_t>(Id.Hash());
		}
	};
} // namespace std
