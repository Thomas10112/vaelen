// VAELEN - VaelenSim
// Simulation events: plain-data records with a cause, a subject and a small
// typed payload. The event log is the historical record and the replay input.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// An event is immutable once published. It carries:
//   - Id      : PersistentId of kind Event, monotonic per world;
//   - Tick    : when it was published;
//   - Type    : FNV-1a hash of the event type name (EventType<T>);
//   - Cause   : the id of the event that caused it (Invalid for root causes):
//               this is the edge of the causal graph (Phase 17);
//   - Subject : the persistent id the event is about (may be Invalid);
//   - Payload : up to MaxPayloadBytes of a trivially copyable T.
// Every byte of an Event is defined (zero-filled before use), so events can be
// hashed and serialised as raw bytes.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/SimClock.h"

#include <cstring>
#include <string_view>
#include <type_traits>

namespace Vaelen
{
	/// A typed event identity: name plus its hash, usable as a constant.
	template <typename T>
	struct EventType
	{
		static_assert(std::is_trivially_copyable_v<T>, "event payloads must be trivially copyable plain data");

		const char* Name = "";
		Hash64 TypeHash = 0;

		constexpr bool IsValid() const noexcept { return TypeHash != 0; }
	};

	template <typename T>
	constexpr EventType<T> MakeEventType(const char* Name) noexcept
	{
		EventType<T> Type;
		Type.Name = Name;
		Type.TypeHash = HashString(Name);
		return Type;
	}

	/// Payload carried by an event without a payload type.
	struct NoPayload
	{
	};

	struct Event
	{
		static constexpr uint32 MaxPayloadBytes = 64;

		PersistentId Id;
		SimTick Tick = 0;
		Hash64 TypeHash = 0;
		PersistentId Cause;
		PersistentId Subject;
		uint32 PayloadSize = 0;
		uint32 Reserved = 0; ///< keeps the payload 8-byte aligned; always 0
		uint8 Payload[MaxPayloadBytes] = {};

		/// True when the event carries a payload of exactly T.
		template <typename T>
		bool Is(EventType<T> Type) const noexcept
		{
			return TypeHash == Type.TypeHash && PayloadSize == PayloadBytesOf<T>();
		}

		/// Copies the payload out. The caller must have checked Is(Type).
		template <typename T>
		T Get() const noexcept
		{
			static_assert(sizeof(T) <= MaxPayloadBytes, "event payload too large");
			T Value{};
			if constexpr (!std::is_empty_v<T>)
			{
				std::memcpy(&Value, Payload, sizeof(T));
			}
			return Value;
		}

		/// Stores the payload (zero-fills the remaining bytes).
		template <typename T>
		void Set(const T& Value) noexcept
		{
			static_assert(std::is_trivially_copyable_v<T>, "event payloads must be trivially copyable");
			static_assert(sizeof(T) <= MaxPayloadBytes, "event payload too large");
			std::memset(Payload, 0, MaxPayloadBytes);
			if constexpr (!std::is_empty_v<T>)
			{
				std::memcpy(Payload, &Value, sizeof(T));
			}
			PayloadSize = PayloadBytesOf<T>();
		}

		template <typename T>
		static constexpr uint32 PayloadBytesOf() noexcept
		{
			return std::is_empty_v<T> ? 0u : static_cast<uint32>(sizeof(T));
		}

		/// Order-independent per-event hash over every defined byte.
		Hash64 Hash() const noexcept
		{
			return HashBytes(reinterpret_cast<const char*>(this), sizeof(Event));
		}

		bool operator==(const Event&) const noexcept = default;
	};

	static_assert(std::is_trivially_copyable_v<Event>, "Event must be raw-serialisable");
	static_assert(sizeof(Event) == 8 + 8 + 8 + 8 + 8 + 4 + 4 + Event::MaxPayloadBytes, "Event must have no padding");
} // namespace Vaelen
