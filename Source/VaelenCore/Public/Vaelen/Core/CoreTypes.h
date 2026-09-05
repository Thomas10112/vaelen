// VAELEN - VaelenCore
// Fundamental scalar types, the module export macro and small helpers shared
// by every kernel module.
//
// STATUS: VALIDATED (Phase 00) - unit/deterministic/edge tests in Tests/Core;
//         integration and long-duration tests deferred to Phase 01 (ROADMAP 01.07, 01.08).
//
// Rules:
//   - No Unreal headers. This file must compile in the headless CMake build.
//   - Fixed-width integers only; never rely on the width of `int` or `long`.
//   - The 64-bit aliases are spelled `long long` on purpose: Unreal's int64 /
//     uint64 are `long long` on every platform, while std::int64_t is `long`
//     on Linux LP64. Using the same underlying type keeps kernel and engine
//     types interchangeable (references, overloads, FArchive operators).
//
// Export macro: VAELEN_CORE_API is owned by the kernel, not by the build tool.
//   - VAELEN_CORE_EXPORTS  defined while compiling VaelenCore as a shared
//                          library (UBT modular / editor builds, private def).
//   - VAELEN_CORE_IMPORTS  defined for consumers of that shared library.
//   - neither              static library or monolithic build: empty.
// Unreal Build Tool also defines VAELEN_CORE_API=DLLEXPORT/DLLIMPORT for this
// module; that token is only meaningful after HAL/Platform.h, which the kernel
// never includes, so the kernel deliberately does not use it.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#if defined(VAELEN_CORE_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_CORE_API __declspec(dllexport)
#	else
#		define VAELEN_CORE_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_CORE_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_CORE_API __declspec(dllimport)
#	else
#		define VAELEN_CORE_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_CORE_API
#endif

/// Suppresses one MSVC warning on the next line; expands to nothing elsewhere.
#if defined(_MSC_VER)
#	define VAELEN_MSVC_WARNING_SUPPRESS(Number) __pragma(warning(suppress : Number))
#else
#	define VAELEN_MSVC_WARNING_SUPPRESS(Number)
#endif

namespace Vaelen
{
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = signed long long; // PURITY-ALLOW(R7): must match Unreal's int64 on Linux (see header)
	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = unsigned long long; // PURITY-ALLOW(R7): must match Unreal's uint64 on Linux (see header)
	using usize = std::size_t;

	static_assert(sizeof(int8) == 1 && sizeof(int16) == 2 && sizeof(int32) == 4 && sizeof(int64) == 8,
				  "fixed-width signed integers are required");
	static_assert(sizeof(uint8) == 1 && sizeof(uint16) == 2 && sizeof(uint32) == 4 && sizeof(uint64) == 8,
				  "fixed-width unsigned integers are required");
	static_assert(sizeof(float) == 4 && sizeof(double) == 8, "IEEE-754 float sizes are required");
	static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
				  "IEEE-754 floating point is required");
	// PersistentId and RandomStreamState are persisted as raw little-endian bytes.
	static_assert(std::endian::native == std::endian::little, "VAELEN persists state as little-endian raw bytes");

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
