// VAELEN - VaelenCore tests
// Deterministic hashing: FNV-1a 64 known answers (published test vectors plus
// an independent inline re-derivation), compile-time evaluation, byte
// semantics (octets, embedded NULs, seed chaining), the SplitMix64 finaliser
// Mix64 (known answers, invertibility and bijectivity, avalanche), HashCombine
// (order dependence, frozen values) and HashUInt64.
//
// STATUS: VALIDATED
//
// Every value below is either a published test vector, re-derived by an
// independent implementation in this file, or a FROZEN regression value:
// HashCombine feeds RandomStream::Derive / Fork, so any change to it silently
// changes every derived seed of every saved world. Frozen values must never be
// "updated" without a save-format migration.
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <vector>

using Vaelen::Hash64;
using Vaelen::HashBytes;
using Vaelen::HashCombine;
using Vaelen::HashString;
using Vaelen::HashUInt64;
using Vaelen::Mix64;
using Vaelen::uint32;
using Vaelen::uint64;
using Vaelen::usize;
using Vaelen::operator""_vhash;

namespace
{
	// ── Independent FNV-1a 64 (Fowler / Noll / Vo) ───────────────────────────
	// Written from the published algorithm with the constants in DECIMAL so a
	// typo in the kernel's hexadecimal constants cannot be mirrored here.
	uint64 RefFnv1a64(const void* Data, usize Size) noexcept
	{
		const auto* Bytes = static_cast<const unsigned char*>(Data);
		uint64 H = 14695981039346656037ull; // offset basis
		for (usize i = 0; i < Size; ++i)
		{
			H ^= static_cast<uint64>(Bytes[i]);
			H *= 1099511628211ull; // prime = 2^40 + 2^8 + 0xb3
		}
		return H;
	}

	// Published FNV-1a 64-bit test vectors (Landon Curt Noll's test_fnv.c).
	struct KnownAnswer
	{
		const char* Text;
		uint64 Expected;
	};

	constexpr KnownAnswer Fnv1a64KnownAnswers[] = {
		{"", 0xcbf29ce484222325ull},
		{"a", 0xaf63dc4c8601ec8cull},
		{"foobar", 0x85944171f73967e8ull},
	};

	// The SplitMix64 golden-ratio increment, written out here independently.
	constexpr uint64 Golden = 0x9e3779b97f4a7c15ull;

	// Knuth's MMIX LCG: a cheap, deterministic, kernel-independent source of
	// sample inputs (never used for the values under test themselves).
	uint64 NextLcg(uint64& State) noexcept
	{
		State = State * 6364136223846793005ull + 1442695040888963407ull;
		return State;
	}

	// ── Mix64 inverse, derived here rather than copied ───────────────────────
	// Inverse of an odd number modulo 2^64 by Newton iteration: every step
	// doubles the number of correct low bits (3 -> 6 -> 12 -> 24 -> 48 -> 96).
	constexpr uint64 ModularInverse(uint64 A) noexcept
	{
		uint64 X = A;
		for (int i = 0; i < 6; ++i)
		{
			X *= uint64{2} - A * X;
		}
		return X;
	}

	// Inverts Y = X ^ (X >> Shift): the top `Shift` bits of Y are already those
	// of X, and each iteration recovers `Shift` more bits below them.
	constexpr uint64 UnXorShiftRight(uint64 Y, uint32 Shift) noexcept
	{
		uint64 X = Y;
		for (uint32 Recovered = Shift; Recovered < 64; Recovered += Shift)
		{
			X = Y ^ (X >> Shift);
		}
		return X;
	}

	constexpr uint64 Mix64Inverse(uint64 H) noexcept
	{
		uint64 X = UnXorShiftRight(H, 31);
		X *= ModularInverse(0x94d049bb133111ebull);
		X = UnXorShiftRight(X, 27);
		X *= ModularInverse(0xbf58476d1ce4e5b9ull);
		X = UnXorShiftRight(X, 30);
		return X - Golden;
	}

	static_assert(ModularInverse(0xbf58476d1ce4e5b9ull) * 0xbf58476d1ce4e5b9ull == 1);
	static_assert(ModularInverse(0x94d049bb133111ebull) * 0x94d049bb133111ebull == 1);
	static_assert(UnXorShiftRight(uint64{0x0123456789abcdefull} ^ (uint64{0x0123456789abcdefull} >> 31), 31) ==
				  0x0123456789abcdefull);
} // namespace

// ── Types and constants ─────────────────────────────────────────────────────

static_assert(std::is_same_v<Hash64, uint64>, "Hash64 must be the 64-bit unsigned kernel integer");
static_assert(sizeof(Hash64) == 8);
static_assert(Vaelen::HashConstants::Fnv1a64Offset == 14695981039346656037ull, "FNV-1a 64 offset basis");
static_assert(Vaelen::HashConstants::Fnv1a64Prime == 1099511628211ull, "FNV-1a 64 prime");

// ── Compile-time evaluation ─────────────────────────────────────────────────

// HashBytes is constexpr: a published vector evaluated in a constant expression.
static_assert(HashBytes("a", 1) == 0xaf63dc4c8601ec8cull);
static_assert(HashBytes("foobar", 6) == 0x85944171f73967e8ull);
static_assert(HashBytes("", 0) == Vaelen::HashConstants::Fnv1a64Offset);
// HashString and the _vhash literal are constexpr and agree with each other.
static_assert(HashString("foobar") == 0x85944171f73967e8ull);
static_assert("foobar"_vhash == HashString("foobar"));
static_assert("foobar"_vhash == HashBytes("foobar", 6));
static_assert(""_vhash == Vaelen::HashConstants::Fnv1a64Offset);
static_assert("a"_vhash == 0xaf63dc4c8601ec8cull);
static_assert(HashString(std::string_view("foobar")) == "foobar"_vhash);
// Mix64, HashCombine and HashUInt64 are constexpr too.
static_assert(Mix64(0) == 0xE220A8397B1DCDAFull);
static_assert(HashUInt64(0) == Mix64(0));
static_assert(HashCombine(1, 2) != HashCombine(2, 1));
static_assert(Mix64Inverse(Mix64(0)) == 0);
static_assert(Mix64Inverse(0xE220A8397B1DCDAFull) == 0);

// ── FNV-1a 64 ───────────────────────────────────────────────────────────────

VAELEN_TEST(Hash, Fnv1a64PublishedVectors)
{
	for (const KnownAnswer& Answer : Fnv1a64KnownAnswers)
	{
		const usize Length = std::strlen(Answer.Text);
		VT_CHECK_EQ(HashString(Answer.Text), Answer.Expected);
		VT_CHECK_EQ(HashBytes(Answer.Text, Length), Answer.Expected);
		// The independent reference must agree with the published vectors as well.
		VT_CHECK_EQ(RefFnv1a64(Answer.Text, Length), Answer.Expected);
	}
}

VAELEN_TEST(Hash, Fnv1a64MatchesIndependentLoop)
{
	const char* const Texts[] = {
		"",
		"a",
		"b",
		"foobar",
		"hello",
		"vaelen",
		"hydrology",
		"chongo was here!",
		"The quick brown fox jumps over the lazy dog",
		"0123456789012345678901234567890123456789012345678901234567890123456789",
	};
	for (const char* Text : Texts)
	{
		const usize Length = std::strlen(Text);
		VT_CHECK_EQ(HashBytes(Text, Length), RefFnv1a64(Text, Length));
		VT_CHECK_EQ(HashString(Text), RefFnv1a64(Text, Length));
	}

	// Every single byte value, and every pair (b, b ^ 0x5a): 512 binary inputs.
	for (uint32 Byte = 0; Byte < 256; ++Byte)
	{
		const char One[1] = {static_cast<char>(Byte)};
		const char Two[2] = {static_cast<char>(Byte), static_cast<char>(Byte ^ 0x5au)};
		VT_CHECK_EQ(HashBytes(One, 1), RefFnv1a64(One, 1));
		VT_CHECK_EQ(HashBytes(Two, 2), RefFnv1a64(Two, 2));
	}

	// A longer pseudo-random binary buffer.
	char Buffer[4096];
	uint64 Lcg = 0x5641454c454e0001ull;
	for (usize i = 0; i < sizeof(Buffer); ++i)
	{
		Buffer[i] = static_cast<char>(NextLcg(Lcg) >> 56);
	}
	VT_CHECK_EQ(HashBytes(Buffer, sizeof(Buffer)), RefFnv1a64(Buffer, sizeof(Buffer)));
	VT_CHECK_EQ(HashBytes(Buffer, 1000), RefFnv1a64(Buffer, 1000));
}

VAELEN_TEST(Hash, BytesAreHashedAsOctets)
{
	// FNV hashes OCTETS: a byte >= 0x80 must be folded in as 0x80..0xff, never
	// sign-extended through `char`.
	const char HighByte[1] = {static_cast<char>(0xff)};
	const uint64 Expected = (Vaelen::HashConstants::Fnv1a64Offset ^ 0xffull) * Vaelen::HashConstants::Fnv1a64Prime;
	const uint64 SignExtended =
		(Vaelen::HashConstants::Fnv1a64Offset ^ 0xffffffffffffffffull) * Vaelen::HashConstants::Fnv1a64Prime;
	VT_CHECK_EQ(HashBytes(HighByte, 1), Expected);
	VT_CHECK_EQ(HashBytes(HighByte, 1), RefFnv1a64(HighByte, 1));
	VT_CHECK_NE(HashBytes(HighByte, 1), SignExtended);
	VT_CHECK_EQ(HashBytes(HighByte, 1), 0xaf64724c8602eb6eull);

	// Embedded NUL bytes are data, not terminators.
	const char WithNul[3] = {'a', '\0', 'b'};
	VT_CHECK_EQ(HashBytes(WithNul, 3), RefFnv1a64(WithNul, 3));
	VT_CHECK_NE(HashBytes(WithNul, 3), HashBytes(WithNul, 1));
	VT_CHECK_NE(HashBytes(WithNul, 3), HashString("ab"));
	VT_CHECK_EQ(HashString(std::string_view(WithNul, 3)), HashBytes(WithNul, 3));

	// Zero length yields the seed regardless of the (unread) data pointer.
	VT_CHECK_EQ(HashBytes(WithNul, 0), Vaelen::HashConstants::Fnv1a64Offset);
	VT_CHECK_EQ(HashBytes(WithNul, 0, 12345), uint64{12345});

	// The empty name (null data pointer, size 0) is the common Derive("") case.
	VT_CHECK_EQ(Vaelen::HashString(std::string_view{}), Vaelen::HashConstants::Fnv1a64Offset);
	VT_CHECK_EQ(HashBytes(nullptr, 0), Vaelen::HashConstants::Fnv1a64Offset);
	static_assert(Vaelen::HashString(std::string_view{}) == Vaelen::HashConstants::Fnv1a64Offset);
}

VAELEN_TEST(Hash, HashStringEqualsHashBytes)
{
	const char* const Texts[] = {"", "a", "foobar", "hydrology", "settlement/market/route"};
	for (const char* Text : Texts)
	{
		const std::string_view View(Text);
		VT_CHECK_EQ(HashString(View), HashBytes(View.data(), View.size()));
		VT_CHECK_EQ(HashString(Text), HashBytes(Text, std::strlen(Text)));
	}

	// Sub-views hash exactly the bytes they cover, independent of the buffer.
	const std::string_view Whole("prefix:hydrology:suffix");
	const std::string_view Middle = Whole.substr(7, 9);
	VT_CHECK_EQ(Middle.size(), usize{9});
	VT_CHECK_EQ(HashString(Middle), HashString("hydrology"));
	VT_CHECK_EQ(HashString(Middle), HashBytes(Whole.data() + 7, 9));
	VT_CHECK_EQ(HashString(Middle), "hydrology"_vhash);
	VT_CHECK_NE(HashString(Middle), HashString(Whole));

	// Runtime-built text agrees with the compile-time literal.
	char Runtime[16] = {};
	std::snprintf(Runtime, sizeof(Runtime), "%s%s", "foo", "bar");
	VT_CHECK_EQ(HashString(Runtime), "foobar"_vhash);
	VT_CHECK_EQ(HashString(Runtime), 0x85944171f73967e8ull);
}

VAELEN_TEST(Hash, SeedParameterChainsIncrementally)
{
	// Hashing "foo" then continuing with "bar" from that seed equals hashing "foobar".
	const uint64 Foo = HashBytes("foo", 3);
	VT_CHECK_EQ(HashBytes("bar", 3, Foo), HashString("foobar"));

	// Byte-by-byte chaining equals a single call.
	const std::string_view Text("The quick brown fox");
	uint64 Chained = Vaelen::HashConstants::Fnv1a64Offset;
	for (usize i = 0; i < Text.size(); ++i)
	{
		Chained = HashBytes(Text.data() + i, 1, Chained);
	}
	VT_CHECK_EQ(Chained, HashString(Text));

	// The default seed is the FNV offset basis.
	VT_CHECK_EQ(HashBytes("x", 1), HashBytes("x", 1, Vaelen::HashConstants::Fnv1a64Offset));
	VT_CHECK_NE(HashBytes("x", 1), HashBytes("x", 1, 0));
}

VAELEN_TEST(Hash, DistinctNamesGiveDistinctHashes)
{
	// The kernel's stream / category names must not collide (birthday risk at
	// 64 bits is negligible; a collision here would mean a broken hash).
	const char* const Names[] = {"hydrology", "climate",	"geology",	"ecology",	"history",	"culture",
								 "language",  "religion",	"economy",	"politics", "military", "knowledge",
								 "Hydrology", "hydrology ", "hydrolog", "yhdrology"};
	std::vector<uint64> Hashes;
	for (const char* Name : Names)
	{
		Hashes.push_back(HashString(Name));
	}
	std::sort(Hashes.begin(), Hashes.end());
	VT_CHECK(std::adjacent_find(Hashes.begin(), Hashes.end()) == Hashes.end());
}

// ── Mix64 (SplitMix64 finaliser) ────────────────────────────────────────────

VAELEN_TEST(Hash, Mix64KnownAnswers)
{
	// SplitMix64 seeded with 0 produces e220a8397b1dcdaf, 6e789e6aa1b965f4,
	// 06c45d188009454f, f88bb8a8724c81ec (Vigna's reference implementation);
	// output k of that sequence is Mix64(k * golden).
	VT_CHECK_EQ(Mix64(0), 0xE220A8397B1DCDAFull);
	VT_CHECK_EQ(Mix64(Golden), 0x6e789e6aa1b965f4ull);
	VT_CHECK_EQ(Mix64(2 * Golden), 0x06c45d188009454full);
	VT_CHECK_EQ(Mix64(3 * Golden), 0xf88bb8a8724c81ecull);

	// Mix64(X) is exactly one SplitMix64 step from state X.
	uint64 State = 0;
	VT_CHECK_EQ(Vaelen::SplitMix64Next(State), Mix64(0));
	VT_CHECK_EQ(State, Golden);
	VT_CHECK_EQ(Vaelen::SplitMix64Next(State), Mix64(Golden));
	VT_CHECK_EQ(Vaelen::SplitMix64Next(State), Mix64(2 * Golden));

	uint64 Lcg = 0x1234ull;
	for (int i = 0; i < 1000; ++i)
	{
		const uint64 X = NextLcg(Lcg);
		uint64 Step = X;
		VT_CHECK_EQ(Vaelen::SplitMix64Next(Step), Mix64(X));
	}
}

VAELEN_TEST(Hash, Mix64IsBijectiveOnSample)
{
	constexpr usize Count = 100000;
	std::vector<uint64> Outputs;
	Outputs.reserve(Count);

	// Batch 1: 100k consecutive inputs (distinct by construction).
	for (usize i = 0; i < Count; ++i)
	{
		Outputs.push_back(Mix64(static_cast<uint64>(i)));
	}
	std::sort(Outputs.begin(), Outputs.end());
	VT_CHECK(std::adjacent_find(Outputs.begin(), Outputs.end()) == Outputs.end());
	Outputs.erase(std::unique(Outputs.begin(), Outputs.end()), Outputs.end());
	VT_CHECK_EQ(Outputs.size(), Count);

	// Batch 2: 100k inputs i * golden (odd multiplier => injective mod 2^64).
	Outputs.clear();
	for (usize i = 0; i < Count; ++i)
	{
		Outputs.push_back(Mix64(static_cast<uint64>(i) * Golden));
	}
	std::sort(Outputs.begin(), Outputs.end());
	Outputs.erase(std::unique(Outputs.begin(), Outputs.end()), Outputs.end());
	VT_CHECK_EQ(Outputs.size(), Count);
}

VAELEN_TEST(Hash, Mix64IsInvertible)
{
	// A function with a two-sided inverse is a bijection: check both directions
	// on 100k samples plus the edge values.
	const uint64 Edges[] = {0, 1, 2, Golden, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull};
	for (uint64 X : Edges)
	{
		VT_CHECK_EQ(Mix64Inverse(Mix64(X)), X);
		VT_CHECK_EQ(Mix64(Mix64Inverse(X)), X);
	}

	uint64 Lcg = 0x5641454c454e0002ull;
	uint32 RoundTripFailures = 0;
	for (uint32 i = 0; i < 100000; ++i)
	{
		const uint64 X = NextLcg(Lcg);
		if (Mix64Inverse(Mix64(X)) != X || Mix64(Mix64Inverse(X)) != X)
		{
			++RoundTripFailures;
		}
	}
	VT_CHECK_EQ(RoundTripFailures, 0u);
}

VAELEN_TEST(Hash, Mix64Avalanche)
{
	// Flipping any single input bit should flip about half (32) of the 64
	// output bits on average. Fixed LCG inputs: deterministic, never flaky.
	// A correct mixer sits within +/-0.2 of 32 per bit at this sample size;
	// the 28..36 window is far outside that noise.
	constexpr uint32 SampleCount = 10000;
	uint64 PerBitFlips[64] = {};
	uint64 Lcg = 0x0123456789abcdefull;
	for (uint32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const uint64 X = NextLcg(Lcg);
		const uint64 H = Mix64(X);
		for (uint32 Bit = 0; Bit < 64; ++Bit)
		{
			PerBitFlips[Bit] += static_cast<uint64>(std::popcount(H ^ Mix64(X ^ (uint64{1} << Bit))));
		}
	}

	double Total = 0.0;
	for (uint32 Bit = 0; Bit < 64; ++Bit)
	{
		const double Average = static_cast<double>(PerBitFlips[Bit]) / static_cast<double>(SampleCount);
		VT_CHECK_MSG(Average >= 28.0 && Average <= 36.0, "input bit %u flips %.3f output bits on average", Bit,
					 Average);
		Total += Average;
	}
	const double Overall = Total / 64.0;
	VT_CHECK_MSG(Overall >= 28.0 && Overall <= 36.0, "overall average %.4f output bits flipped", Overall);
	if (Ctx.Verbose)
	{
		std::printf("         Mix64 avalanche: %.4f output bits flip per input bit (10k samples x 64 bits)\n", Overall);
	}
}

// ── HashCombine / HashUInt64 ────────────────────────────────────────────────

VAELEN_TEST(Hash, HashCombineIsOrderDependent)
{
	struct Pair
	{
		uint64 A;
		uint64 B;
	};
	const Pair Pairs[] = {
		{0, 1},
		{1, 2},
		{1, 0xffffffffffffffffull},
		{"hydrology"_vhash, "climate"_vhash},
		{"a"_vhash, "b"_vhash},
		{Golden, 0},
		{Mix64(1), Mix64(2)},
	};
	for (const Pair& P : Pairs)
	{
		VT_CHECK_NE(HashCombine(P.A, P.B), HashCombine(P.B, P.A));
	}

	uint64 Lcg = 0x5641454c454e0003ull;
	uint32 Symmetric = 0;
	for (uint32 i = 0; i < 10000; ++i)
	{
		const uint64 A = NextLcg(Lcg);
		const uint64 B = NextLcg(Lcg);
		if (A != B && HashCombine(A, B) == HashCombine(B, A))
		{
			++Symmetric;
		}
	}
	VT_CHECK_EQ(Symmetric, 0u);
}

VAELEN_TEST(Hash, HashCombineDiffersFromItsInputs)
{
	const uint64 Values[] = {0,		  1, 2, Golden, 0x7fffffffffffffffull, 0xffffffffffffffffull, "hydrology"_vhash,
							 Mix64(7)};
	for (uint64 A : Values)
	{
		for (uint64 B : Values)
		{
			const uint64 C = HashCombine(A, B);
			VT_CHECK_NE(C, A);
			VT_CHECK_NE(C, B);
		}
	}

	uint64 Lcg = 0x5641454c454e0004ull;
	uint32 FixedPoints = 0;
	for (uint32 i = 0; i < 10000; ++i)
	{
		const uint64 A = NextLcg(Lcg);
		const uint64 B = NextLcg(Lcg);
		const uint64 C = HashCombine(A, B);
		if (C == A || C == B)
		{
			++FixedPoints;
		}
	}
	VT_CHECK_EQ(FixedPoints, 0u);
}

VAELEN_TEST(Hash, HashCombineOfMixedInputsHasNoCollisionsOnSample)
{
	// HashCombine is documented as combining HASHES. On a 300 x 300 grid of
	// well-mixed inputs (90k combinations) every output must be distinct.
	std::vector<uint64> Outputs;
	Outputs.reserve(300 * 300);
	for (uint64 A = 0; A < 300; ++A)
	{
		for (uint64 B = 0; B < 300; ++B)
		{
			Outputs.push_back(HashCombine(Mix64(A), Mix64(1000 + B)));
		}
	}
	std::sort(Outputs.begin(), Outputs.end());
	Outputs.erase(std::unique(Outputs.begin(), Outputs.end()), Outputs.end());
	VT_CHECK_EQ(Outputs.size(), usize{300 * 300});
}

VAELEN_TEST(Hash, HashCombineFrozenValues)
{
	// FROZEN: these feed RandomStream::Derive / Fork and therefore every saved
	// world. A failure here means the save format changed, not that the test is
	// stale. The only legitimate fix is a new VAELEN_SAVE_FORMAT_VERSION with a
	// migration.
	VT_CHECK_EQ(HashCombine(0, 0), 0x6e789e6aa1b965f4ull); // == Mix64(golden)
	VT_CHECK_EQ(HashCombine(1, 2), 0xa3efbcce2e044f84ull);
	VT_CHECK_EQ(HashCombine(2, 1), 0x88a32f63162d1170ull);
	VT_CHECK_EQ(HashCombine("a"_vhash, "b"_vhash), 0xe93e3179a77c1783ull);
	VT_CHECK_EQ(HashCombine("b"_vhash, "a"_vhash), 0x0595d2f5b50ab803ull);

	// Characterisation, not an endorsement: the boost-style inner combine
	// A ^ (B + golden + (A << 6) + (A >> 2)) is NOT injective for small raw
	// integers - (0, 65) and (1, 2) collide. This is harmless for the kernel's
	// use (the first operand is always a well-mixed hash, see the grid test
	// above) and is frozen for the same save-format reason as the values above.
	VT_CHECK_EQ(HashCombine(0, 65), HashCombine(1, 2));
}

VAELEN_TEST(Hash, HashUInt64EqualsMix64)
{
	const uint64 Values[] = {0, 1, 2, Golden, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull};
	for (uint64 X : Values)
	{
		VT_CHECK_EQ(HashUInt64(X), Mix64(X));
	}
	VT_CHECK_EQ(HashUInt64(0), 0xE220A8397B1DCDAFull);

	uint64 Lcg = 0x5641454c454e0005ull;
	uint32 Mismatches = 0;
	for (uint32 i = 0; i < 10000; ++i)
	{
		const uint64 X = NextLcg(Lcg);
		if (HashUInt64(X) != Mix64(X))
		{
			++Mismatches;
		}
	}
	VT_CHECK_EQ(Mismatches, 0u);
}
