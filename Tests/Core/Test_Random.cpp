// VAELEN - VaelenCore tests
// RandomStream / SplitMix64Next: known answers against independent reference
// implementations, determinism, hierarchical derivation, integer and floating
// draw bounds, distribution sanity and assertion paths.
//
// STATUS: VALIDATED (Phase 00) - all tests pass warning-free with clang++ 18 and g++ 13.
//
// Every statistical check below runs on a fixed seed, so it is fully
// deterministic and can never flake; the tolerances are chosen far outside the
// sampling noise of a correct generator (see the per-test comments).
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <vector>

using Vaelen::int64;
using Vaelen::RandomStream;
using Vaelen::RandomStreamState;
using Vaelen::uint32;
using Vaelen::uint64;
using Vaelen::usize;
using Vaelen::operator""_vhash;

namespace
{
	// ── Independent reference implementations (Blackman & Vigna, public domain)
	// Transcribed from the published splitmix64.c / xoshiro256starstar.c and
	// deliberately sharing nothing with the kernel, so that a kernel typo
	// cannot be mirrored here.

	uint64 RefSplitMix64(uint64& X) noexcept
	{
		uint64 Z = (X += 0x9e3779b97f4a7c15ull);
		Z = (Z ^ (Z >> 30)) * 0xbf58476d1ce4e5b9ull;
		Z = (Z ^ (Z >> 27)) * 0x94d049bb133111ebull;
		return Z ^ (Z >> 31);
	}

	uint64 RefRotL(uint64 X, uint32 K) noexcept
	{
		return (X << K) | (X >> (64u - K));
	}

	struct RefXoshiro256StarStar
	{
		uint64 S[4] = {0, 0, 0, 0};

		void SeedFrom(uint64 Seed) noexcept
		{
			uint64 Sm = Seed;
			S[0] = RefSplitMix64(Sm);
			S[1] = RefSplitMix64(Sm);
			S[2] = RefSplitMix64(Sm);
			S[3] = RefSplitMix64(Sm);
		}

		uint64 Next() noexcept
		{
			const uint64 Result = RefRotL(S[1] * 5, 7) * 9;
			const uint64 T = S[1] << 17;
			S[2] ^= S[0];
			S[3] ^= S[1];
			S[1] ^= S[2];
			S[0] ^= S[3];
			S[2] ^= T;
			S[3] = RefRotL(S[3], 45);
			return Result;
		}

		void Jump() noexcept
		{
			static constexpr uint64 JumpPolynomial[4] = {0x180ec6d33cfd0abaull, 0xd5a61266f0c9392cull,
														 0xa9582618e03fc9aaull, 0x39abdc4529b1661cull};
			uint64 S0 = 0;
			uint64 S1 = 0;
			uint64 S2 = 0;
			uint64 S3 = 0;
			for (uint64 Word : JumpPolynomial)
			{
				for (uint32 B = 0; B < 64; ++B)
				{
					if ((Word & (uint64{1} << B)) != 0)
					{
						S0 ^= S[0];
						S1 ^= S[1];
						S2 ^= S[2];
						S3 ^= S[3];
					}
					(void)Next();
				}
			}
			S[0] = S0;
			S[1] = S1;
			S[2] = S2;
			S[3] = S3;
		}
	};

	bool SameGeneratorState(const RefXoshiro256StarStar& Ref, const RandomStreamState& State) noexcept
	{
		return Ref.S[0] == State.S[0] && Ref.S[1] == State.S[1] && Ref.S[2] == State.S[2] && Ref.S[3] == State.S[3];
	}

	bool SameGeneratorState(const RandomStreamState& A, const RandomStreamState& B) noexcept
	{
		return A.S[0] == B.S[0] && A.S[1] == B.S[1] && A.S[2] == B.S[2] && A.S[3] == B.S[3];
	}

	std::vector<uint64> DrawSequence(RandomStream& Stream, usize Count)
	{
		std::vector<uint64> Out;
		Out.reserve(Count);
		for (usize i = 0; i < Count; ++i)
		{
			Out.push_back(Stream.NextU64());
		}
		return Out;
	}

	RandomStreamState MakeRawState(uint64 S0, uint64 S1, uint64 S2, uint64 S3) noexcept
	{
		RandomStreamState State;
		State.Seed = 0;
		State.S[0] = S0;
		State.S[1] = S1;
		State.S[2] = S2;
		State.S[3] = S3;
		State.DrawCount = 0;
		return State;
	}

	struct BucketStats
	{
		uint64 MinCount = 0;
		uint64 MaxCount = 0;
		uint64 OutOfRange = 0;
		double Expected = 0.0;
		double ChiSquare = 0.0;
	};

	BucketStats CollectBelowBuckets(RandomStream& Stream, uint64 BucketCount, uint64 Draws)
	{
		std::vector<uint64> Counts(static_cast<usize>(BucketCount), uint64{0});
		BucketStats Stats;
		for (uint64 i = 0; i < Draws; ++i)
		{
			const uint64 V = Stream.Below(BucketCount);
			if (V < BucketCount)
			{
				++Counts[static_cast<usize>(V)];
			}
			else
			{
				++Stats.OutOfRange;
			}
		}
		Stats.Expected = static_cast<double>(Draws) / static_cast<double>(BucketCount);
		Stats.MinCount = ~uint64{0};
		Stats.MaxCount = 0;
		for (uint64 C : Counts)
		{
			Stats.MinCount = C < Stats.MinCount ? C : Stats.MinCount;
			Stats.MaxCount = C > Stats.MaxCount ? C : Stats.MaxCount;
			const double D = static_cast<double>(C) - Stats.Expected;
			Stats.ChiSquare += D * D / Stats.Expected;
		}
		return Stats;
	}

	/// Checks every bucket lies within `RelativeTolerance` of the expectation
	/// and that the chi-square statistic is within 5 sigma of its mean
	/// (mean = df, sigma = sqrt(2 df)).
	void CheckUniformBuckets(VaelenTest::Context& Ctx, const BucketStats& Stats, uint64 BucketCount,
							 double RelativeTolerance)
	{
		VT_CHECK_EQ(Stats.OutOfRange, uint64{0});
		const double Low = Stats.Expected * (1.0 - RelativeTolerance);
		const double High = Stats.Expected * (1.0 + RelativeTolerance);
		VT_CHECK_MSG(static_cast<double>(Stats.MinCount) >= Low, "min bucket %llu below %.1f (expected %.1f)",
					 static_cast<unsigned long long>(Stats.MinCount), Low, Stats.Expected);
		VT_CHECK_MSG(static_cast<double>(Stats.MaxCount) <= High, "max bucket %llu above %.1f (expected %.1f)",
					 static_cast<unsigned long long>(Stats.MaxCount), High, Stats.Expected);
		const double DegreesOfFreedom = static_cast<double>(BucketCount - 1);
		const double ChiSigma = std::sqrt(2.0 * DegreesOfFreedom);
		VT_CHECK_MSG(Stats.ChiSquare <= DegreesOfFreedom + 5.0 * ChiSigma, "chi-square %.2f too high (df %.0f)",
					 Stats.ChiSquare, DegreesOfFreedom);
	}
} // namespace

// ── Known answers ────────────────────────────────────────────────────────────

VAELEN_TEST(Random, SplitMix64KnownAnswers)
{
	// Reference outputs of splitmix64.c for initial state 0, re-derived by
	// the independent implementation above (the first value is the widely
	// published one; the following ones are what the algorithm produces).
	constexpr uint64 Expected[4] = {0xE220A8397B1DCDAFull, 0x6E789E6AA1B965F4ull, 0x06C45D188009454Full,
									0xF88BB8A8724C81ECull};

	uint64 KernelState = 0;
	uint64 RefState = 0;
	for (usize i = 0; i < 4; ++i)
	{
		const uint64 Ref = RefSplitMix64(RefState);
		const uint64 Kernel = Vaelen::SplitMix64Next(KernelState);
		VT_CHECK_EQ(Ref, Expected[i]);
		VT_CHECK_EQ(Kernel, Expected[i]);
		VT_CHECK_EQ(KernelState, RefState);
	}
	// The state advances by the golden-ratio increment on every call.
	VT_CHECK_EQ(KernelState, uint64{4} * 0x9e3779b97f4a7c15ull);

	// Longer agreement with the reference, from a non-trivial state.
	KernelState = 0x0123456789ABCDEFull;
	RefState = KernelState;
	usize Mismatches = 0;
	for (usize i = 0; i < 10000; ++i)
	{
		if (Vaelen::SplitMix64Next(KernelState) != RefSplitMix64(RefState))
		{
			++Mismatches;
		}
	}
	VT_CHECK_EQ(Mismatches, usize{0});
	VT_CHECK_EQ(KernelState, RefState);

	// SplitMix64 is a bijection of its state: distinct consecutive outputs.
	uint64 S = 0;
	const uint64 A = Vaelen::SplitMix64Next(S);
	const uint64 B = Vaelen::SplitMix64Next(S);
	VT_CHECK_NE(A, B);
}

VAELEN_TEST(Random, Xoshiro256StarStarKnownAnswers)
{
	// xoshiro256starstar.c with s = {1, 2, 3, 4}: first outputs are
	// rotl(2*5, 7)*9 = 11520, then 0, then 1509978240.
	const RandomStreamState Raw = MakeRawState(1, 2, 3, 4);

	RandomStream Stream(Raw);
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{0});
	VT_CHECK_EQ(Stream.NextU64(), uint64{11520});
	VT_CHECK_EQ(Stream.NextU64(), uint64{0});
	VT_CHECK_EQ(Stream.NextU64(), uint64{1509978240});
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{3});

	// Same through SetState() on a stream that was seeded and used before.
	RandomStream Reused(12345);
	(void)Reused.NextU64();
	(void)Reused.NextU64();
	Reused.SetState(Raw);
	VT_CHECK_EQ(Reused.GetDrawCount(), uint64{0});
	VT_CHECK_EQ(Reused.NextU64(), uint64{11520});
	VT_CHECK_EQ(Reused.NextU64(), uint64{0});
	VT_CHECK_EQ(Reused.NextU64(), uint64{1509978240});

	// 1000-draw agreement with the independent reference from {1,2,3,4}.
	{
		RefXoshiro256StarStar Ref;
		Ref.S[0] = 1;
		Ref.S[1] = 2;
		Ref.S[2] = 3;
		Ref.S[3] = 4;
		RandomStream Kernel(Raw);
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			if (Kernel.NextU64() != Ref.Next())
			{
				++Mismatches;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
		VT_CHECK(SameGeneratorState(Ref, Kernel.GetState()));
		VT_CHECK_EQ(Kernel.GetDrawCount(), uint64{1000});
	}

	// Seeded path: the kernel seeds through SplitMix64 exactly like the
	// reference recommends (four consecutive SplitMix64 outputs).
	{
		constexpr uint64 Seed = 0xDEADBEEFCAFEBABEull;
		RefXoshiro256StarStar Ref;
		Ref.SeedFrom(Seed);
		RandomStream Kernel(Seed);
		VT_CHECK(SameGeneratorState(Ref, Kernel.GetState()));
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			if (Kernel.NextU64() != Ref.Next())
			{
				++Mismatches;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
		VT_CHECK(SameGeneratorState(Ref, Kernel.GetState()));
	}
}

VAELEN_TEST(Random, NextU32IsHighHalfOfNextU64)
{
	RandomStream A(55);
	RandomStream B(55);
	usize Mismatches = 0;
	usize TopBitSet = 0;
	for (usize i = 0; i < 2000; ++i)
	{
		const uint32 Narrow = A.NextU32();
		const uint64 Wide = B.NextU64();
		if (Narrow != static_cast<uint32>(Wide >> 32))
		{
			++Mismatches;
		}
		if ((Narrow & 0x80000000u) != 0)
		{
			++TopBitSet;
		}
	}
	VT_CHECK_EQ(Mismatches, usize{0});
	VT_CHECK_EQ(A.GetDrawCount(), uint64{2000});
	VT_CHECK(TopBitSet > 800 && TopBitSet < 1200);
}

// ── Determinism, reseeding, state ───────────────────────────────────────────

VAELEN_TEST(Random, DeterminismSameSeedIdenticalDifferentSeedsDiffer)
{
	constexpr usize Length = 10000;
	RandomStream A(42);
	RandomStream B(42);
	RandomStream C(43);
	RandomStream D(~uint64{42});

	const std::vector<uint64> SeqA = DrawSequence(A, Length);
	const std::vector<uint64> SeqB = DrawSequence(B, Length);
	const std::vector<uint64> SeqC = DrawSequence(C, Length);
	const std::vector<uint64> SeqD = DrawSequence(D, Length);

	VT_CHECK(SeqA == SeqB);
	VT_CHECK(A.GetState() == B.GetState());
	VT_CHECK(SeqA != SeqC);
	VT_CHECK(SeqA != SeqD);
	VT_CHECK(SeqC != SeqD);

	// Position-wise coincidences between different seeds are negligible for
	// 64-bit outputs: allow at most a handful just to be safe.
	usize Coincidences = 0;
	for (usize i = 0; i < Length; ++i)
	{
		if (SeqA[i] == SeqC[i])
		{
			++Coincidences;
		}
	}
	VT_CHECK(Coincidences <= 2);

	// Adjacent seeds must not produce a shifted copy of the same sequence.
	usize ShiftedMatches = 0;
	for (usize i = 0; i + 1 < Length; ++i)
	{
		if (SeqA[i + 1] == SeqC[i] || SeqA[i] == SeqC[i + 1])
		{
			++ShiftedMatches;
		}
	}
	VT_CHECK(ShiftedMatches <= 2);
}

VAELEN_TEST(Random, ReseedResetsDrawCountAndReproducesSequence)
{
	constexpr uint64 Seed = 777;
	RandomStream Stream(Seed);
	const std::vector<uint64> First = DrawSequence(Stream, 1000);
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{1000});
	VT_CHECK_EQ(Stream.GetSeed(), Seed);

	Stream.Reseed(Seed);
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{0});
	VT_CHECK_EQ(Stream.GetSeed(), Seed);
	VT_CHECK(Stream.GetState() == RandomStream(Seed).GetState());

	const std::vector<uint64> Second = DrawSequence(Stream, 1000);
	VT_CHECK(First == Second);
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{1000});

	// Reseeding with another seed changes the seed, the sequence, and still
	// resets the draw count.
	Stream.Reseed(Seed + 1);
	VT_CHECK_EQ(Stream.GetSeed(), Seed + 1);
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{0});
	const std::vector<uint64> Third = DrawSequence(Stream, 1000);
	VT_CHECK(Third != First);
	RandomStream Fresh(Seed + 1);
	VT_CHECK(Third == DrawSequence(Fresh, 1000));
	VT_CHECK(Stream.GetState() == Fresh.GetState());
}

VAELEN_TEST(Random, StateRoundTrip)
{
	constexpr usize Prefix = 137;
	RandomStream A(99);
	for (usize i = 0; i < Prefix; ++i)
	{
		(void)A.NextU64();
	}
	const RandomStreamState Saved = A.GetState();
	VT_CHECK_EQ(Saved.Seed, uint64{99});
	VT_CHECK_EQ(Saved.DrawCount, uint64{Prefix});

	// Restore into a stream with a different seed and history.
	RandomStream B(5);
	(void)B.NextU64();
	B.SetState(Saved);
	VT_CHECK(B.GetState() == Saved);
	VT_CHECK_EQ(B.GetSeed(), uint64{99});
	VT_CHECK_EQ(B.GetDrawCount(), uint64{Prefix});

	// Restore through the constructor as well.
	RandomStream C(Saved);
	VT_CHECK(C.GetState() == Saved);

	const std::vector<uint64> SeqA = DrawSequence(A, 1000);
	const std::vector<uint64> SeqB = DrawSequence(B, 1000);
	const std::vector<uint64> SeqC = DrawSequence(C, 1000);
	VT_CHECK(SeqA == SeqB);
	VT_CHECK(SeqA == SeqC);
	VT_CHECK(A.GetState() == B.GetState());
	VT_CHECK(A.GetState() == C.GetState());
	VT_CHECK_EQ(B.GetDrawCount(), uint64{Prefix + 1000});

	// GetState() returns a snapshot: the saved copy is unaffected by later draws.
	VT_CHECK_EQ(Saved.DrawCount, uint64{Prefix});
	VT_CHECK(!(Saved == A.GetState()));

	// Derivation depends only on the root seed, so it survives the round trip.
	VT_CHECK(A.Derive("child").GetState() == B.Derive("child").GetState());
	VT_CHECK(A.Fork(3).GetState() == C.Fork(3).GetState());
}

VAELEN_TEST(Random, DefaultConstructedEqualsSeedZero)
{
	RandomStream Default;
	RandomStream Zero(0);
	VT_CHECK(Default.GetState() == Zero.GetState());
	VT_CHECK_EQ(Default.GetSeed(), uint64{0});
	VT_CHECK_EQ(Default.GetDrawCount(), uint64{0});

	// The state is the SplitMix64 expansion of seed 0 (see SplitMix64KnownAnswers).
	VT_CHECK_EQ(Default.GetState().S[0], 0xE220A8397B1DCDAFull);
	VT_CHECK_EQ(Default.GetState().S[1], 0x6E789E6AA1B965F4ull);
	VT_CHECK_EQ(Default.GetState().S[2], 0x06C45D188009454Full);
	VT_CHECK_EQ(Default.GetState().S[3], 0xF88BB8A8724C81ECull);

	VT_CHECK(DrawSequence(Default, 1000) == DrawSequence(Zero, 1000));
	VT_CHECK(Default.GetState() == Zero.GetState());
}

// ── Hierarchical derivation ─────────────────────────────────────────────────

VAELEN_TEST(Random, DeriveByName)
{
	constexpr uint64 ParentSeed = 1234;
	RandomStream Parent(ParentSeed);
	const RandomStream Hydrology = Parent.Derive("hydrology");

	// Deterministic, and derivation never advances the parent.
	VT_CHECK(Hydrology.GetState() == Parent.Derive("hydrology").GetState());
	VT_CHECK(Hydrology.GetState() == RandomStream(ParentSeed).Derive("hydrology").GetState());
	VT_CHECK_EQ(Parent.GetDrawCount(), uint64{0});
	VT_CHECK_EQ(Hydrology.GetDrawCount(), uint64{0});
	VT_CHECK_NE(Hydrology.GetSeed(), ParentSeed);
	VT_CHECK_NE(Hydrology.GetSeed(), uint64{0});

	// Independent of the parent's draw position.
	for (usize i = 0; i < 500; ++i)
	{
		(void)Parent.NextU64();
	}
	VT_CHECK_EQ(Parent.GetDrawCount(), uint64{500});
	VT_CHECK(Parent.Derive("hydrology").GetState() == Hydrology.GetState());
	VT_CHECK_EQ(Parent.GetDrawCount(), uint64{500});

	// Distinct for different names (including case and the empty name).
	VT_CHECK_NE(Parent.Derive("hydrology").GetSeed(), Parent.Derive("weather").GetSeed());
	VT_CHECK_NE(Parent.Derive("a").GetSeed(), Parent.Derive("b").GetSeed());
	VT_CHECK_NE(Parent.Derive("a").GetSeed(), Parent.Derive("A").GetSeed());
	VT_CHECK_NE(Parent.Derive("a").GetSeed(), Parent.Derive("").GetSeed());
	VT_CHECK_NE(Parent.Derive("ab").GetSeed(), Parent.Derive("ba").GetSeed());
	{
		std::set<uint64> Seeds;
		for (uint32 i = 0; i < 1000; ++i)
		{
			char Name[32];
			std::snprintf(Name, sizeof(Name), "system%u", i);
			Seeds.insert(Parent.Derive(Name).GetSeed());
		}
		VT_CHECK_EQ(Seeds.size(), usize{1000});
	}

	// Derive("a") and Fork(0) live in different domains.
	VT_CHECK_NE(Parent.Derive("a").GetSeed(), Parent.Fork(0).GetSeed());
	VT_CHECK(!(Parent.Derive("a").GetState() == Parent.Fork(0).GetState()));
	VT_CHECK_NE(Parent.Derive("0").GetSeed(), Parent.Fork(0).GetSeed());

	// Derive by string == Derive by its HashString hash (runtime and literal).
	VT_CHECK(Parent.Derive("hydrology").GetState() == Parent.Derive(Vaelen::HashString("hydrology")).GetState());
	VT_CHECK(Parent.Derive("hydrology").GetState() == Parent.Derive("hydrology"_vhash).GetState());
	VT_CHECK_EQ(Parent.Derive("weather").GetSeed(), Parent.Derive(Vaelen::HashString("weather")).GetSeed());

	// Different parents give different children; order of nesting matters.
	VT_CHECK_NE(RandomStream(1).Derive("a").GetSeed(), RandomStream(2).Derive("a").GetSeed());
	VT_CHECK_NE(Parent.Derive("a").Derive("b").GetSeed(), Parent.Derive("b").Derive("a").GetSeed());
	VT_CHECK_NE(Parent.Derive("a").Derive("b").GetSeed(), Parent.Derive("ab").GetSeed());

	// A child sequence is not the parent sequence.
	RandomStream Child = Parent.Derive("hydrology");
	RandomStream Fresh(ParentSeed);
	VT_CHECK(DrawSequence(Child, 1000) != DrawSequence(Fresh, 1000));
}

VAELEN_TEST(Random, ForkDistinctSeeds)
{
	constexpr uint64 ParentSeed = 2024;
	RandomStream Parent(ParentSeed);

	std::set<uint64> Seeds;
	for (uint64 i = 0; i < 1000; ++i)
	{
		const RandomStream Child = Parent.Fork(i);
		Seeds.insert(Child.GetSeed());
		VT_CHECK_EQ(Child.GetDrawCount(), uint64{0});
	}
	VT_CHECK_EQ(Seeds.size(), usize{1000});
	VT_CHECK_EQ(Seeds.count(ParentSeed), usize{0});
	VT_CHECK_EQ(Parent.GetDrawCount(), uint64{0});

	// Deterministic and independent of the parent's draw position.
	const RandomStream Fork7 = Parent.Fork(7);
	for (usize i = 0; i < 300; ++i)
	{
		(void)Parent.NextU64();
	}
	VT_CHECK(Parent.Fork(7).GetState() == Fork7.GetState());
	VT_CHECK(RandomStream(ParentSeed).Fork(7).GetState() == Fork7.GetState());

	// Extreme indices work and stay distinct.
	VT_CHECK_NE(Parent.Fork(~uint64{0}).GetSeed(), Parent.Fork(0).GetSeed());
	VT_CHECK_NE(Parent.Fork(uint64{1} << 63).GetSeed(), Parent.Fork(0).GetSeed());

	// Different parents give different forks.
	VT_CHECK_NE(RandomStream(1).Fork(0).GetSeed(), RandomStream(2).Fork(0).GetSeed());

	// Forked sequences differ from each other and from the parent.
	RandomStream Fork0 = Parent.Fork(0);
	RandomStream Fork1 = Parent.Fork(1);
	RandomStream Fresh(ParentSeed);
	const std::vector<uint64> Seq0 = DrawSequence(Fork0, 1000);
	const std::vector<uint64> Seq1 = DrawSequence(Fork1, 1000);
	VT_CHECK(Seq0 != Seq1);
	VT_CHECK(Seq0 != DrawSequence(Fresh, 1000));
}

// ── Integer draws ───────────────────────────────────────────────────────────

VAELEN_TEST(Random, BelowBounds)
{
	RandomStream Stream(31337);

	// Below(1) returns 0 WITHOUT consuming a draw (documented actual behaviour:
	// the single-value range short-circuits before touching the generator).
	{
		const uint64 Before = Stream.GetDrawCount();
		for (usize i = 0; i < 100; ++i)
		{
			VT_CHECK_EQ(Stream.Below(1), uint64{0});
		}
		VT_CHECK_EQ(Stream.GetDrawCount(), Before);
	}

	// Always < n, and every call with n >= 2 consumes at least one draw.
	constexpr uint64 Counts[] = {
		2,		   3, 5, 7, 10, 100, 1000, 12345, 65537, (uint64{1} << 40) + 3, (uint64{1} << 63) + 1, ~uint64{0} - 1,
		~uint64{0}};
	for (uint64 Count : Counts)
	{
		usize Violations = 0;
		usize NoDraw = 0;
		for (usize i = 0; i < 2000; ++i)
		{
			const uint64 Before = Stream.GetDrawCount();
			const uint64 V = Stream.Below(Count);
			if (V >= Count)
			{
				++Violations;
			}
			if (Stream.GetDrawCount() == Before)
			{
				++NoDraw;
			}
		}
		VT_CHECK_MSG(Violations == 0, "Below(%llu) produced %llu out-of-range values",
					 static_cast<unsigned long long>(Count), static_cast<unsigned long long>(Violations));
		VT_CHECK_EQ(NoDraw, usize{0});
	}

	// Small ranges actually reach every value.
	{
		uint64 Hits[3] = {0, 0, 0};
		for (usize i = 0; i < 300; ++i)
		{
			++Hits[static_cast<usize>(Stream.Below(3))];
		}
		VT_CHECK(Hits[0] != 0 && Hits[1] != 0 && Hits[2] != 0);
	}

	// Rejection sampling never exceeds a small expected number of draws:
	// for the worst case (Count = 2^k + 1) acceptance is ~1/2 per attempt.
	{
		const uint64 Before = Stream.GetDrawCount();
		for (usize i = 0; i < 10000; ++i)
		{
			(void)Stream.Below((uint64{1} << 20) + 1);
		}
		const uint64 Draws = Stream.GetDrawCount() - Before;
		VT_CHECK(Draws >= 10000);
		VT_CHECK(Draws < 25000);
	}
}

VAELEN_TEST(Random, BelowDistribution)
{
	// n = 7, 100k draws: expected 14285.7 per bucket, sigma ~110, so the 15%
	// band (+-2143) is ~19 sigma wide.
	{
		RandomStream Stream(7001);
		const BucketStats Stats = CollectBelowBuckets(Stream, 7, 100000);
		CheckUniformBuckets(Ctx, Stats, 7, 0.15);
	}

	// n = 1000, 100k draws: expected 100 per bucket with sigma 10. A 15% band
	// would be only 1.5 sigma and a CORRECT generator would fail ~13% of the
	// 1000 buckets, so here the per-bucket band is 50% (5 sigma) and the
	// chi-square statistic carries the real test.
	{
		RandomStream Stream(7002);
		const BucketStats Stats = CollectBelowBuckets(Stream, 1000, 100000);
		CheckUniformBuckets(Ctx, Stats, 1000, 0.50);
	}

	// n = 1000, 1M draws: expected 1000 per bucket with sigma 31.6, so the
	// 15% band (+-150) is ~4.7 sigma and the requested tolerance is meaningful.
	{
		RandomStream Stream(7003);
		const BucketStats Stats = CollectBelowBuckets(Stream, 1000, 1000000);
		CheckUniformBuckets(Ctx, Stats, 1000, 0.15);
	}
}

VAELEN_TEST(Random, BelowPowerOfTwoPath)
{
	// For Count = 2^k the mask equals Count - 1, so no candidate is ever
	// rejected: exactly one draw per call and the result is the masked output.
	RandomStream Stream(5);
	RandomStream Twin(5);
	constexpr uint32 Exponents[] = {1, 2, 3, 8, 16, 31, 32, 33, 48, 63};
	for (uint32 K : Exponents)
	{
		const uint64 Count = uint64{1} << K;
		usize Mismatches = 0;
		for (usize i = 0; i < 200; ++i)
		{
			const uint64 Before = Stream.GetDrawCount();
			const uint64 V = Stream.Below(Count);
			const uint64 Expected = Twin.NextU64() & (Count - 1);
			if (V >= Count || V != Expected || Stream.GetDrawCount() != Before + 1)
			{
				++Mismatches;
			}
		}
		VT_CHECK_MSG(Mismatches == 0, "Below(2^%u): %llu mismatches", K, static_cast<unsigned long long>(Mismatches));
	}

	// Below(2) is a fair coin over a long run.
	{
		usize Ones = 0;
		for (usize i = 0; i < 100000; ++i)
		{
			Ones += static_cast<usize>(Stream.Below(2));
		}
		VT_CHECK(Ones > 49000 && Ones < 51000);
	}
}

VAELEN_TEST(Random, RangeInclusiveEdges)
{
	constexpr int64 Int64Min = std::numeric_limits<int64>::min();
	constexpr int64 Int64Max = std::numeric_limits<int64>::max();

	RandomStream Stream(8);

	// Min == Max returns Min and consumes no draw (Span 0 -> Below(1)).
	constexpr int64 Singletons[] = {0, 5, -5, Int64Min, Int64Max};
	for (int64 V : Singletons)
	{
		const uint64 Before = Stream.GetDrawCount();
		VT_CHECK_EQ(Stream.RangeInclusive(V, V), V);
		VT_CHECK_EQ(Stream.GetDrawCount(), Before);
	}

	// Full int64 range: returns without hanging, exactly one draw, and the
	// value is the raw 64-bit output reinterpreted as signed.
	{
		RandomStream Full(8);
		RandomStream Twin(8);
		usize Negatives = 0;
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			const uint64 Before = Full.GetDrawCount();
			const int64 V = Full.RangeInclusive(Int64Min, Int64Max);
			if (Full.GetDrawCount() != Before + 1 || V != static_cast<int64>(Twin.NextU64()))
			{
				++Mismatches;
			}
			if (V < 0)
			{
				++Negatives;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
		VT_CHECK(Negatives > 400 && Negatives < 600);
	}

	// Negative range: every value in [-10, -5] is produced and nothing else.
	{
		constexpr int64 Min = -10;
		constexpr int64 Max = -5;
		uint64 Hits[6] = {0, 0, 0, 0, 0, 0};
		usize OutOfRange = 0;
		for (usize i = 0; i < 3000; ++i)
		{
			const int64 V = Stream.RangeInclusive(Min, Max);
			if (V < Min || V > Max)
			{
				++OutOfRange;
				continue;
			}
			++Hits[static_cast<usize>(V - Min)];
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		for (uint64 H : Hits)
		{
			VT_CHECK(H != 0);
		}
	}

	// Range straddling zero.
	{
		uint64 Hits[7] = {0, 0, 0, 0, 0, 0, 0};
		usize OutOfRange = 0;
		for (usize i = 0; i < 3000; ++i)
		{
			const int64 V = Stream.RangeInclusive(-3, 3);
			if (V < -3 || V > 3)
			{
				++OutOfRange;
				continue;
			}
			++Hits[static_cast<usize>(V + 3)];
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		for (uint64 H : Hits)
		{
			VT_CHECK(H != 0);
		}
	}

	// Two-value ranges at both extremes hit both values.
	{
		usize LowMin = 0;
		usize LowMax = 0;
		usize HighMin = 0;
		usize HighMax = 0;
		usize OutOfRange = 0;
		for (usize i = 0; i < 200; ++i)
		{
			const int64 Low = Stream.RangeInclusive(Int64Min, Int64Min + 1);
			const int64 High = Stream.RangeInclusive(Int64Max - 1, Int64Max);
			if (Low == Int64Min)
			{
				++LowMin;
			}
			else if (Low == Int64Min + 1)
			{
				++LowMax;
			}
			else
			{
				++OutOfRange;
			}
			if (High == Int64Max)
			{
				++HighMax;
			}
			else if (High == Int64Max - 1)
			{
				++HighMin;
			}
			else
			{
				++OutOfRange;
			}
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		VT_CHECK(LowMin != 0 && LowMax != 0 && HighMin != 0 && HighMax != 0);
	}

	// Half ranges (span 2^63 - 1 and 2^63): bounded, no hang.
	{
		usize OutOfRange = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			const int64 A = Stream.RangeInclusive(Int64Min, 0);
			const int64 B = Stream.RangeInclusive(-1, Int64Max);
			const int64 C = Stream.RangeInclusive(Int64Min, -1);
			if (A > 0 || B < -1 || C > -1)
			{
				++OutOfRange;
			}
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
	}
}

VAELEN_TEST(Random, RangeInclusiveUFullRange)
{
	constexpr uint64 UInt64Max = ~uint64{0};
	RandomStream Stream(9);
	RandomStream Twin(9);

	// Full range: exactly one draw, bit-identical to NextU64().
	{
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			const uint64 Before = Stream.GetDrawCount();
			const uint64 V = Stream.RangeInclusiveU(0, UInt64Max);
			if (Stream.GetDrawCount() != Before + 1 || V != Twin.NextU64())
			{
				++Mismatches;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
	}

	// Singletons: no draw.
	{
		const uint64 Before = Stream.GetDrawCount();
		VT_CHECK_EQ(Stream.RangeInclusiveU(UInt64Max, UInt64Max), UInt64Max);
		VT_CHECK_EQ(Stream.RangeInclusiveU(0, 0), uint64{0});
		VT_CHECK_EQ(Stream.RangeInclusiveU(42, 42), uint64{42});
		VT_CHECK_EQ(Stream.GetDrawCount(), Before);
	}

	// Almost-full ranges (span 2^64 - 2) and ranges hugging the top.
	{
		usize OutOfRange = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			const uint64 A = Stream.RangeInclusiveU(1, UInt64Max);
			const uint64 B = Stream.RangeInclusiveU(0, UInt64Max - 1);
			const uint64 C = Stream.RangeInclusiveU(UInt64Max - 5, UInt64Max);
			if (A < 1 || B == UInt64Max || C < UInt64Max - 5)
			{
				++OutOfRange;
			}
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
	}

	// Small range reaches every value.
	{
		uint64 Hits[11] = {};
		usize OutOfRange = 0;
		for (usize i = 0; i < 3000; ++i)
		{
			const uint64 V = Stream.RangeInclusiveU(10, 20);
			if (V < 10 || V > 20)
			{
				++OutOfRange;
				continue;
			}
			++Hits[static_cast<usize>(V - 10)];
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		for (uint64 H : Hits)
		{
			VT_CHECK(H != 0);
		}
	}
}

// ── Floating-point draws ────────────────────────────────────────────────────

VAELEN_TEST(Random, NextDoubleAndFloatBounds)
{
	constexpr usize Samples = 100000;
	constexpr double SamplesD = 100000.0;

	// NextDouble: [0, 1), mean 0.5 (sigma of the mean ~0.0009, tolerance 0.01),
	// bit-identical to the top 53 bits of the raw output.
	{
		RandomStream Stream(2026);
		RandomStream Twin(2026);
		double Sum = 0.0;
		double MaxSeen = 0.0;
		usize OutOfRange = 0;
		usize Mismatches = 0;
		for (usize i = 0; i < Samples; ++i)
		{
			const double D = Stream.NextDouble();
			if (!(D >= 0.0 && D < 1.0))
			{
				++OutOfRange;
			}
			if (D != static_cast<double>(Twin.NextU64() >> 11) * 0x1.0p-53)
			{
				++Mismatches;
			}
			Sum += D;
			MaxSeen = D > MaxSeen ? D : MaxSeen;
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		VT_CHECK_EQ(Mismatches, usize{0});
		VT_CHECK_EQ(Stream.GetDrawCount(), uint64{Samples});
		VT_CHECK_NEAR(Sum / SamplesD, 0.5, 0.01);
		VT_CHECK(MaxSeen > 0.99);
	}

	// The largest representable draw (all 53 bits set) is still < 1.
	VT_CHECK(static_cast<double>((uint64{1} << 53) - 1) * 0x1.0p-53 < 1.0);
	VT_CHECK(static_cast<float>((uint32{1} << 24) - 1) * 0x1.0p-24f < 1.0f);

	// NextFloat: [0, 1), derived from the top 24 bits.
	{
		RandomStream Stream(2027);
		RandomStream Twin(2027);
		double Sum = 0.0;
		usize OutOfRange = 0;
		usize Mismatches = 0;
		for (usize i = 0; i < Samples; ++i)
		{
			const float F = Stream.NextFloat();
			if (!(F >= 0.0f && F < 1.0f))
			{
				++OutOfRange;
			}
			if (F != static_cast<float>(Twin.NextU64() >> 40) * 0x1.0p-24f)
			{
				++Mismatches;
			}
			Sum += static_cast<double>(F);
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		VT_CHECK_EQ(Mismatches, usize{0});
		VT_CHECK_EQ(Stream.GetDrawCount(), uint64{Samples});
		VT_CHECK_NEAR(Sum / SamplesD, 0.5, 0.01);
	}
}

VAELEN_TEST(Random, RangeDoubleBounds)
{
	RandomStream Stream(3141);

	// [Min, Max) with Min < Max, negative Min; mean of 10k draws within 0.15
	// of the midpoint (sigma of the mean ~0.029).
	{
		constexpr double Min = -2.5;
		constexpr double Max = 7.5;
		double Sum = 0.0;
		usize OutOfRange = 0;
		for (usize i = 0; i < 10000; ++i)
		{
			const double V = Stream.RangeDouble(Min, Max);
			if (!(V >= Min && V < Max))
			{
				++OutOfRange;
			}
			Sum += V;
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
		VT_CHECK_NEAR(Sum / 10000.0, 2.5, 0.15);
	}

	// Degenerate range returns Min and still consumes one draw (documented).
	{
		const uint64 Before = Stream.GetDrawCount();
		VT_CHECK_EQ(Stream.RangeDouble(3.0, 3.0), 3.0);
		VT_CHECK_EQ(Stream.GetDrawCount(), Before + 1);
	}

	// RangeDouble(0, 1) is exactly NextDouble().
	{
		RandomStream A(3142);
		RandomStream B(3142);
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			if (A.RangeDouble(0.0, 1.0) != B.NextDouble())
			{
				++Mismatches;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
	}

	// Tiny and large spans stay inside their bounds.
	{
		usize OutOfRange = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			const double Tiny = Stream.RangeDouble(1.0, 1.0 + 1e-9);
			const double Large = Stream.RangeDouble(-1e300, 1e300);
			if (!(Tiny >= 1.0 && Tiny < 1.0 + 1e-9) || !(Large >= -1e300 && Large < 1e300))
			{
				++OutOfRange;
			}
		}
		VT_CHECK_EQ(OutOfRange, usize{0});
	}
}

VAELEN_TEST(Random, ChanceEdges)
{
	RandomStream Stream(4242);

	// Chance(0): always false, consumes NO draw.
	{
		const uint64 Before = Stream.GetDrawCount();
		usize Trues = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			if (Stream.Chance(0.0))
			{
				++Trues;
			}
		}
		VT_CHECK_EQ(Trues, usize{0});
		VT_CHECK_EQ(Stream.GetDrawCount(), Before);
	}

	// Chance(1): always true, consumes NO draw.
	{
		const uint64 Before = Stream.GetDrawCount();
		usize Falses = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			if (!Stream.Chance(1.0))
			{
				++Falses;
			}
		}
		VT_CHECK_EQ(Falses, usize{0});
		VT_CHECK_EQ(Stream.GetDrawCount(), Before);
	}

	// Out-of-range probabilities clamp and do not draw either.
	{
		const uint64 Before = Stream.GetDrawCount();
		VT_CHECK(!Stream.Chance(-0.25));
		VT_CHECK(Stream.Chance(1.75));
		VT_CHECK_EQ(Stream.GetDrawCount(), Before);
	}

	// Strictly inside (0, 1) every call consumes exactly one draw and is
	// equivalent to NextDouble() < p.
	{
		RandomStream Twin(Stream.GetState());
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			const uint64 Before = Stream.GetDrawCount();
			const bool Result = Stream.Chance(0.5);
			if (Stream.GetDrawCount() != Before + 1 || Result != (Twin.NextDouble() < 0.5))
			{
				++Mismatches;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
	}
}

VAELEN_TEST(Random, ChanceDistribution)
{
	// 100k trials at p = 0.3: sigma of the fraction ~0.00145, tolerance 0.01.
	RandomStream Stream(3030);
	constexpr usize Trials = 100000;
	usize Hits = 0;
	for (usize i = 0; i < Trials; ++i)
	{
		if (Stream.Chance(0.3))
		{
			++Hits;
		}
	}
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{Trials});
	VT_CHECK_NEAR(static_cast<double>(Hits) / 100000.0, 0.3, 0.01);

	// Very small probabilities are rare but not impossible.
	usize RareHits = 0;
	for (usize i = 0; i < Trials; ++i)
	{
		if (Stream.Chance(0.001))
		{
			++RareHits;
		}
	}
	VT_CHECK(RareHits > 50 && RareHits < 200);
}

VAELEN_TEST(Random, NextNormalStatistics)
{
	// 200k samples: sigma of the mean ~0.0022 and of the sample deviation
	// ~0.0016, so 0.02 tolerances are ~9 and ~12 sigma respectively.
	RandomStream Stream(1729);
	constexpr usize Samples = 200000;
	constexpr double SamplesD = 200000.0;
	double Sum = 0.0;
	double SumSquares = 0.0;
	usize WithinOneSigma = 0;
	usize WithinThreeSigma = 0;
	usize NonFinite = 0;
	for (usize i = 0; i < Samples; ++i)
	{
		const double X = Stream.NextNormal();
		if (!std::isfinite(X))
		{
			++NonFinite;
			continue;
		}
		Sum += X;
		SumSquares += X * X;
		const double Magnitude = std::fabs(X);
		if (Magnitude < 1.0)
		{
			++WithinOneSigma;
		}
		if (Magnitude < 3.0)
		{
			++WithinThreeSigma;
		}
	}
	const double Mean = Sum / SamplesD;
	const double Variance = SumSquares / SamplesD - Mean * Mean;
	const double StdDev = std::sqrt(Variance);
	VT_CHECK_EQ(NonFinite, usize{0});
	VT_CHECK_NEAR(Mean, 0.0, 0.02);
	VT_CHECK_NEAR(StdDev, 1.0, 0.02);
	VT_CHECK_NEAR(static_cast<double>(WithinOneSigma) / SamplesD, 0.6827, 0.01);
	VT_CHECK_NEAR(static_cast<double>(WithinThreeSigma) / SamplesD, 0.9973, 0.005);

	// The polar method draws pairs: an even number of draws per call, at least 2,
	// and on average ~2.55 (acceptance rate pi/4).
	{
		RandomStream Counted(1730);
		usize OddOrZero = 0;
		for (usize i = 0; i < 10000; ++i)
		{
			const uint64 Before = Counted.GetDrawCount();
			(void)Counted.NextNormal();
			const uint64 Used = Counted.GetDrawCount() - Before;
			if (Used < 2 || (Used % 2) != 0)
			{
				++OddOrZero;
			}
		}
		VT_CHECK_EQ(OddOrZero, usize{0});
		const double AverageDraws = static_cast<double>(Counted.GetDrawCount()) / 10000.0;
		VT_CHECK_NEAR(AverageDraws, 8.0 / 3.14159265358979323846, 0.1);
	}
}

// ── Jump ────────────────────────────────────────────────────────────────────

VAELEN_TEST(Random, JumpProducesDistinctSequence)
{
	constexpr uint64 Seed = 2718;

	RandomStream Original(Seed);
	RandomStream Jumped(Seed);
	Jumped.Jump();

	// Jump changes the generator state but keeps the root seed.
	VT_CHECK(!SameGeneratorState(Jumped.GetState(), Original.GetState()));
	VT_CHECK_EQ(Jumped.GetSeed(), Seed);

	// Documented actual behaviour: Jump() is implemented with 256 internal
	// NextU64() calls, and DrawCount counts those (it advances by 256).
	VT_CHECK_EQ(Jumped.GetDrawCount(), uint64{256});

	const std::vector<uint64> SeqOriginal = DrawSequence(Original, 1000);
	const std::vector<uint64> SeqJumped = DrawSequence(Jumped, 1000);
	VT_CHECK(SeqJumped != SeqOriginal);

	// Differs from the original simply continuing (the jump is 2^128 draws,
	// not a few hundred).
	RandomStream Continuing(Seed);
	(void)DrawSequence(Continuing, 1000);
	const std::vector<uint64> SeqContinuing = DrawSequence(Continuing, 1000);
	VT_CHECK(SeqContinuing != SeqJumped);
	VT_CHECK(SeqContinuing != SeqOriginal);

	// Two jumps differ from one jump and from none.
	RandomStream TwiceJumped(Seed);
	TwiceJumped.Jump();
	TwiceJumped.Jump();
	const std::vector<uint64> SeqTwice = DrawSequence(TwiceJumped, 1000);
	VT_CHECK(SeqTwice != SeqJumped);
	VT_CHECK(SeqTwice != SeqOriginal);
	VT_CHECK(SeqTwice != SeqContinuing);

	// Jump is deterministic.
	RandomStream JumpedAgain(Seed);
	JumpedAgain.Jump();
	VT_CHECK(DrawSequence(JumpedAgain, 1000) == SeqJumped);

	// Jump matches the reference jump() from xoshiro256starstar.c, both from
	// a fresh seed and after some draws.
	{
		RefXoshiro256StarStar Ref;
		Ref.SeedFrom(Seed);
		Ref.Jump();
		RandomStream Kernel(Seed);
		Kernel.Jump();
		VT_CHECK(SameGeneratorState(Ref, Kernel.GetState()));

		for (usize i = 0; i < 37; ++i)
		{
			(void)Ref.Next();
			(void)Kernel.NextU64();
		}
		Ref.Jump();
		Kernel.Jump();
		VT_CHECK(SameGeneratorState(Ref, Kernel.GetState()));
		usize Mismatches = 0;
		for (usize i = 0; i < 1000; ++i)
		{
			if (Kernel.NextU64() != Ref.Next())
			{
				++Mismatches;
			}
		}
		VT_CHECK_EQ(Mismatches, usize{0});
	}
}

// ── Assertion paths ─────────────────────────────────────────────────────────

VAELEN_TEST(Random, AssertBelowZero)
{
	VaelenTest::ScopedAssertCapture Capture;
	RandomStream Stream(1);
	const uint64 Result = Stream.Below(0);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "Count > 0");
#else
	VT_CHECK_EQ(Capture.CheckCount, 0);
#endif
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	// After the report the call degrades gracefully: returns 0 without drawing.
	VT_CHECK_EQ(Result, uint64{0});
	VT_CHECK_EQ(Stream.GetDrawCount(), uint64{0});
}

VAELEN_TEST(Random, AssertRangeInclusiveInverted)
{
	VaelenTest::ScopedAssertCapture Capture;
	RandomStream Stream(1);
	const int64 Result = Stream.RangeInclusive(5, 1);
	(void)Result;
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "Min <= Max");
#else
	VT_CHECK_EQ(Capture.CheckCount, 0);
#endif
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	// The call must still terminate (the wrapped span rejects almost nothing).
	VT_CHECK(Stream.GetDrawCount() >= 1);
	VT_CHECK(Stream.GetDrawCount() < 16);
}

VAELEN_TEST(Random, AssertRangeInclusiveUInverted)
{
	VaelenTest::ScopedAssertCapture Capture;
	RandomStream Stream(1);
	const uint64 Result = Stream.RangeInclusiveU(5, 1);
	(void)Result;
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "Min <= Max");
#else
	VT_CHECK_EQ(Capture.CheckCount, 0);
#endif
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK(Stream.GetDrawCount() >= 1);
	VT_CHECK(Stream.GetDrawCount() < 16);
}
