// VAELEN - VaelenCore
// Fundamental scalar types and small helpers shared by every kernel module.
//
// STATUS: VALIDATED (Phase 00)
//
// Rules:
//   - No Unreal headers. This file must compile in the headless CMake build.
//   - Fixed-width integers only; never rely on the width of `int` or `long`.
#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#ifndef VAELENCORE_API
#	define VAELENCORE_API
#endif

namespace Vaelen
{
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = std::int64_t;
	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = std::uint64_t;
	using usize = std::size_t;

	static_assert(sizeof(int64) == 8 && sizeof(uint64) == 8, "64-bit integers are required");
	static_assert(sizeof(float) == 4 && sizeof(double) == 8, "IEEE-754 float sizes are required");

	/// Base class for types that must never be copied (owners of unique state).
	struct NonCopyable
	{
		NonCopyable() = default;
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
		NonCopyable(NonCopyable&&) = default;
		NonCopyable& operator=(NonCopyable&&) = default;
	};

	/// Number of elements of a C array, evaluated at compile time.
	template <typename T, usize N>
	constexpr usize ArrayCount(const T (&)[N]) noexcept
	{
		return N;
	}

	/// Cast an enum class to its underlying integer.
	template <typename E>
	constexpr auto ToUnderlying(E Value) noexcept
	{
		static_assert(std::is_enum_v<E>, "ToUnderlying requires an enum type");
		return static_cast<std::underlying_type_t<E>>(Value);
	}
} // namespace Vaelen
