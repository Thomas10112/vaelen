// VAELEN - VaelenCore
// Deterministic, platform-independent hashing.
//
// STATUS: VALIDATED (Phase 00) - unit/deterministic/edge tests in Tests/Core;
//         integration and long-duration tests deferred to Phase 01 (ROADMAP 01.07, 01.08).
//
// All hashes here are pure functions of their input bytes: same input on any
// platform, compiler or run yields the same 64-bit value. They are used for
// named random-stream derivation, content addressing and stable map keys.
// They are NOT cryptographic.
#pragma once

#include "Vaelen/Core/CoreTypes.h"

#include <string_view>

namespace Vaelen
{
	using Hash64 = uint64;

	namespace HashConstants
	{
		inline constexpr Hash64 Fnv1a64Offset = 0xcbf29ce484222325ull;
		inline constexpr Hash64 Fnv1a64Prime = 0x00000100000001b3ull;
	} // namespace HashConstants

	/// FNV-1a over raw bytes. constexpr so category / stream names hash at compile time.
	constexpr Hash64 HashBytes(const char* Data, usize Size, Hash64 Seed = HashConstants::Fnv1a64Offset) noexcept
	{
		Hash64 H = Seed;
		for (usize i = 0; i < Size; ++i)
		{
			H ^= static_cast<uint8>(Data[i]);
			H *= HashConstants::Fnv1a64Prime;
		}
		return H;
	}

	constexpr Hash64 HashString(std::string_view Text) noexcept
	{
		return HashBytes(Text.data(), Text.size());
	}

	/// SplitMix64 finalizer: a strong 64->64 bit mixer (bijective).
	constexpr uint64 Mix64(uint64 X) noexcept
	{
		X += 0x9e3779b97f4a7c15ull;
		X = (X ^ (X >> 30)) * 0xbf58476d1ce4e5b9ull;
		X = (X ^ (X >> 27)) * 0x94d049bb133111ebull;
		return X ^ (X >> 31);
	}

	/// Order-dependent combination of two hashes.
	constexpr Hash64 HashCombine(Hash64 A, Hash64 B) noexcept
	{
		return Mix64(A ^ (B + 0x9e3779b97f4a7c15ull + (A << 6) + (A >> 2)));
	}

	constexpr Hash64 HashUInt64(uint64 Value) noexcept
	{
		return Mix64(Value);
	}

	/// Compile-time string hash literal: `"hydrology"_vhash`.
	constexpr Hash64 operator""_vhash(const char* Text, usize Size) noexcept
	{
		return HashBytes(Text, Size);
	}
} // namespace Vaelen
