// VAELEN - VaelenSim
// The "plain data" rule shared by components and event payloads.
//
// STATUS: VALIDATED (Phase 01)
//
// Simulation state is copied and hashed as raw bytes (snapshots, event
// digests). A type with padding bytes would make two identical worlds differ
// in bytes nobody wrote, so every stored type must have a unique object
// representation: trivially copyable, and no padding. The compiler proves
// that for integer-only types. Types with floating-point members cannot be
// proven by the compiler (NaN payloads); their author declares the absence
// of padding by specialising PlainDataTraits<T>::NoPadding = true, and the
// pool/event tests check the size arithmetic. Empty types (no payload) are
// never copied, so they pass.
#pragma once

#include <type_traits>

namespace Vaelen
{
	template <typename T>
	struct PlainDataTraits
	{
		/// Set to true only for types whose fields leave no padding bytes.
		static constexpr bool NoPadding = false;
	};

	template <typename T>
	inline constexpr bool IsPlainData =
		std::is_trivially_copyable_v<T> &&
		(std::is_empty_v<T> || std::has_unique_object_representations_v<T> || PlainDataTraits<T>::NoPadding);

#define VAELEN_PLAIN_DATA_CHECK(T, What)                                                                               \
	static_assert(std::is_trivially_copyable_v<T>, What " must be trivially copyable plain data");                     \
	static_assert(::Vaelen::IsPlainData<T>,                                                                            \
				  What " must have no padding bytes (order fields by decreasing alignment; "                           \
					   "for floating-point members specialise PlainDataTraits<T>::NoPadding)")
} // namespace Vaelen
