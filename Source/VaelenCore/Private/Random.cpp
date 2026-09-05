// VAELEN - VaelenCore
// xoshiro256** / SplitMix64 implementation of RandomStream.
//
// STATUS: VALIDATED (Phase 00) - covered by Tests/Core/Test_Random.cpp
#include "Vaelen/Core/Random.h"
#include "Vaelen/Core/Assert.h"

#include <cmath>

// Floating-point contraction (fused multiply-add) changes rounding and would
// make RangeDouble/NextNormal differ between compilers and targets. The build
// systems pass the equivalent flag, but the source protects itself so that any
// consumer (Unreal Build Tool included) gets the same results (ADR-0009).
#if defined(__clang__)
#	pragma clang fp contract(off)
#elif defined(__GNUC__)
#	pragma GCC optimize("fp-contract=off")
#elif defined(_MSC_VER)
#	pragma fp_contract(off)
#endif

namespace Vaelen
{
	namespace
	{
		constexpr uint64 RotL(uint64 X, int K) noexcept
		{
			return (X << K) | (X >> (64 - K));
		}

		// Stream-derivation domain separators (arbitrary but fixed forever).
		constexpr uint64 DeriveByNameSalt = 0x5641454c454e2d4eull;	// "VAELEN-N"
		constexpr uint64 DeriveByIndexSalt = 0x5641454c454e2d49ull; // "VAELEN-I"
	} // namespace

	uint64 SplitMix64Next(uint64& State) noexcept
	{
		State += 0x9e3779b97f4a7c15ull;
		uint64 Z = State;
		Z = (Z ^ (Z >> 30)) * 0xbf58476d1ce4e5b9ull;
		Z = (Z ^ (Z >> 27)) * 0x94d049bb133111ebull;
		return Z ^ (Z >> 31);
	}

	RandomStream::RandomStream() noexcept
	{
		Reseed(0);
	}

	RandomStream::RandomStream(uint64 Seed) noexcept
	{
		Reseed(Seed);
	}

	RandomStream::RandomStream(const RandomStreamState& InState) noexcept
	{
		SetState(InState);
	}

	void RandomStream::SetState(const RandomStreamState& InState) noexcept
	{
		State = InState;
		// The all-zero state is the xoshiro fixed point (every draw is 0 and
		// NextNormal never terminates). Restore the invariant the same way
		// Reseed does, and report it: a zero state means a corrupt or
		// uninitialised save.
		const bool AllZero = (State.S[0] | State.S[1] | State.S[2] | State.S[3]) == 0;
		if (!VAELEN_ENSURE(!AllZero))
		{
			State.S[0] = 0x9e3779b97f4a7c15ull;
		}
	}

	void RandomStream::Reseed(uint64 Seed) noexcept
	{
		State.Seed = Seed;
		uint64 Sm = Seed;
		State.S[0] = SplitMix64Next(Sm);
		State.S[1] = SplitMix64Next(Sm);
		State.S[2] = SplitMix64Next(Sm);
		State.S[3] = SplitMix64Next(Sm);
		State.DrawCount = 0;
		// xoshiro requires a non-zero state; SplitMix64 makes this astronomically
		// unlikely, but we guarantee it.
		if ((State.S[0] | State.S[1] | State.S[2] | State.S[3]) == 0)
		{
			State.S[0] = 0x9e3779b97f4a7c15ull;
		}
	}

	RandomStream RandomStream::Derive(std::string_view Name) const noexcept
	{
		return Derive(HashString(Name));
	}

	RandomStream RandomStream::Derive(Hash64 NameHash) const noexcept
	{
		return RandomStream(HashCombine(HashCombine(State.Seed, DeriveByNameSalt), NameHash));
	}

	RandomStream RandomStream::Fork(uint64 Index) const noexcept
	{
		return RandomStream(HashCombine(HashCombine(State.Seed, DeriveByIndexSalt), Index));
	}

	uint64 RandomStream::NextU64() noexcept
	{
		uint64* S = State.S;
		const uint64 Result = RotL(S[1] * 5, 7) * 9;
		const uint64 T = S[1] << 17;

		S[2] ^= S[0];
		S[3] ^= S[1];
		S[1] ^= S[2];
		S[0] ^= S[3];
		S[2] ^= T;
		S[3] = RotL(S[3], 45);

		++State.DrawCount;
		return Result;
	}

	uint32 RandomStream::NextU32() noexcept
	{
		return static_cast<uint32>(NextU64() >> 32);
	}

	uint64 RandomStream::Below(uint64 Count) noexcept
	{
		VAELEN_CHECK(Count > 0);
		if (Count <= 1)
		{
			return 0;
		}
		// Bitmask-with-rejection: unbiased, portable (no 128-bit multiply,
		// which MSVC lacks), expected number of draws < 2. Chosen over Lemire's
		// method for portability; the draw count is still deterministic.
		uint64 Mask = Count - 1;
		Mask |= Mask >> 1;
		Mask |= Mask >> 2;
		Mask |= Mask >> 4;
		Mask |= Mask >> 8;
		Mask |= Mask >> 16;
		Mask |= Mask >> 32;
		for (;;)
		{
			const uint64 X = NextU64() & Mask;
			if (X < Count)
			{
				return X;
			}
		}
	}

	uint64 RandomStream::RangeInclusiveU(uint64 Min, uint64 Max) noexcept
	{
		VAELEN_CHECK(Min <= Max);
		const uint64 Span = Max - Min;
		if (Span == ~uint64{0})
		{
			return NextU64();
		}
		return Min + Below(Span + 1);
	}

	int64 RandomStream::RangeInclusive(int64 Min, int64 Max) noexcept
	{
		VAELEN_CHECK(Min <= Max);
		const uint64 Span = static_cast<uint64>(Max) - static_cast<uint64>(Min);
		if (Span == ~uint64{0})
		{
			return static_cast<int64>(NextU64());
		}
		return static_cast<int64>(static_cast<uint64>(Min) + Below(Span + 1));
	}

	double RandomStream::NextDouble() noexcept
	{
		return static_cast<double>(NextU64() >> 11) * 0x1.0p-53;
	}

	float RandomStream::NextFloat() noexcept
	{
		return static_cast<float>(NextU64() >> 40) * 0x1.0p-24f;
	}

	double RandomStream::RangeDouble(double Min, double Max) noexcept
	{
		VAELEN_CHECK(Min <= Max);
		VAELEN_CHECK(std::isfinite(Max - Min));
		const double Result = Min + (Max - Min) * NextDouble();
		// Rounding of the final addition can reach Max when ulp(Min) exceeds
		// the scaled draw; keep the documented half-open interval.
		return Result < Max ? Result : std::nextafter(Max, Min);
	}

	bool RandomStream::Chance(double Probability) noexcept
	{
		VAELEN_CHECK(Probability == Probability); // NaN is not a probability
		if (Probability <= 0.0)
		{
			return false;
		}
		if (Probability >= 1.0)
		{
			return true;
		}
		return NextDouble() < Probability;
	}

	double RandomStream::NextNormal() noexcept
	{
		// Marsaglia polar method. Deterministic given the stream; no caching of
		// the second variate so that draw count stays a pure function of calls.
		for (;;)
		{
			const double U = 2.0 * NextDouble() - 1.0;
			const double V = 2.0 * NextDouble() - 1.0;
			const double SS = U * U + V * V;
			if (SS > 0.0 && SS < 1.0)
			{
				return U * std::sqrt(-2.0 * std::log(SS) / SS);
			}
		}
	}

	void RandomStream::Jump() noexcept
	{
		static constexpr uint64 JumpTable[] = {0x180ec6d33cfd0abaull, 0xd5a61266f0c9392cull, 0xa9582618e03fc9aaull,
											   0x39abdc4529b1661cull};
		uint64 S0 = 0, S1 = 0, S2 = 0, S3 = 0;
		for (uint64 J : JumpTable)
		{
			for (int B = 0; B < 64; ++B)
			{
				if (J & (uint64{1} << B))
				{
					S0 ^= State.S[0];
					S1 ^= State.S[1];
					S2 ^= State.S[2];
					S3 ^= State.S[3];
				}
				(void)NextU64();
			}
		}
		State.S[0] = S0;
		State.S[1] = S1;
		State.S[2] = S2;
		State.S[3] = S3;
	}
} // namespace Vaelen
