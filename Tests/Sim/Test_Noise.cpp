// VAELEN - VaelenSim tests
// Noise: lattice hash and values, value/gradient noise bounds and lattice
// behaviour, continuity, fractal statistics, domain warp, frozen values.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"

#include <cmath>

using namespace Vaelen;
using namespace Vaelen::Noise;

#define VAELEN_NOISE_FROZEN_HASH 0x516788d7e02e2f1bull
#define VAELEN_NOISE_FROZEN_VALUE 0xffffffffb142c66dull
#define VAELEN_NOISE_FROZEN_GRADIENT 0xffffffffa9c48292ull
#define VAELEN_NOISE_FROZEN_FRACTAL 0x1ee3d621ull
#define VAELEN_NOISE_FROZEN_WARPED 0xffffffffee2de4f6ull
#define VAELEN_NOISE_FROZEN_FIELD 0xfb6c12708f1e869aull

static_assert(LatticeHash(1, 0, 0) != LatticeHash(1, 1, 0) && LatticeHash(1, 0, 0) != LatticeHash(2, 0, 0));
static_assert(LatticeValue(7, 3, -4).Raw >= -(int64{1} << 32) && LatticeValue(7, 3, -4).Raw < (int64{1} << 32));

namespace
{
	constexpr Fix64 Half = Fix64::FromRatio(1, 2);
	double D(Fix64 F)
	{
		return static_cast<double>(F.Raw) / 4294967296.0;
	}
	Fix64 Q(int32 Whole, int32 Num, int32 Den)
	{
		return Fix64::FromInt(Whole) + Fix64::FromRatio(Num, Den);
	}
} // namespace

VAELEN_TEST(Noise, ValueNoiseInterpolatesLatticeValuesWithinBounds)
{
	const uint64 Seed = 0x5eed;
	// At lattice points the value is the lattice value itself.
	for (int32 Y = -3; Y <= 3; ++Y)
	{
		for (int32 X = -3; X <= 3; ++X)
		{
			VT_CHECK(Value2D(Seed, Fix64::FromInt(X), Fix64::FromInt(Y)) == LatticeValue(Seed, X, Y));
		}
	}
	// Between lattice points the value stays within the range of the corners.
	Fix64 Lo = Fix64::Max();
	Fix64 Hi = Fix64::Min();
	for (int32 i = 0; i < 4096; ++i)
	{
		const Fix64 X = Q(i % 64, i % 17, 17);
		const Fix64 Y = Q(i / 64, i % 13, 13);
		const Fix64 V = Value2D(Seed, X, Y);
		VT_CHECK(V.Raw >= -(int64{1} << 32) && V.Raw < (int64{1} << 32));
		Lo = Fix64::MinOf(Lo, V);
		Hi = Fix64::MaxOf(Hi, V);
	}
	VT_CHECK(D(Lo) < -0.5 && D(Hi) > 0.5); // the lattice spread is used
	// Negative coordinates floor correctly: the cell of -0.25 is [-1, 0).
	VT_CHECK(Value2D(Seed, Fix64::FromRatio(-1, 4), Fix64::Zero()) !=
			 Value2D(Seed, Fix64::FromRatio(1, 4), Fix64::Zero()));
}

VAELEN_TEST(Noise, GradientNoiseIsZeroOnTheLatticeAndBounded)
{
	const uint64 Seed = 0x6ead;
	for (int32 Y = -2; Y <= 2; ++Y)
	{
		for (int32 X = -2; X <= 2; ++X)
		{
			VT_CHECK(Gradient2D(Seed, Fix64::FromInt(X), Fix64::FromInt(Y)) == Fix64::Zero());
		}
	}
	double Lo = 1e9;
	double Hi = -1e9;
	for (int32 i = 0; i < 65536; ++i)
	{
		const Fix64 X = Q(i % 256, i % 19, 19);
		const Fix64 Y = Q(i / 256, i % 23, 23);
		const double V = D(Gradient2D(Seed, X, Y));
		Lo = V < Lo ? V : Lo;
		Hi = V > Hi ? V : Hi;
	}
	// With these 8 gradients the theoretical bound is sqrt(2)/2 * ... < 1.0; the
	// measured band is asserted loosely and logged by its value in the check.
	VT_CHECK(Lo >= -1.0 && Hi <= 1.0);
	VT_CHECK(Lo < -0.3 && Hi > 0.3);
}

VAELEN_TEST(Noise, NeighbouringSamplesAreContinuous)
{
	const uint64 Seed = 0xc0;
	const Fix64 Step = Fix64::FromRatio(1, 64);
	double WorstJump = 0.0;
	Fix64 Previous = Gradient2D(Seed, Fix64::Zero(), Half);
	for (int32 i = 1; i < 64 * 32; ++i)
	{
		const Fix64 X = Step * i;
		const Fix64 Current = Gradient2D(Seed, X, Half);
		const double Jump = std::fabs(D(Current) - D(Previous));
		WorstJump = Jump > WorstJump ? Jump : WorstJump;
		Previous = Current;
	}
	// The gradient slope is bounded by the gradient magnitude (<= sqrt 2) times the
	// smoothstep derivative peak (1.5): a 1/64 step moves the value by less than 0.1.
	VT_CHECK(WorstJump < 0.1);
}

VAELEN_TEST(Noise, FractalStatisticsAndSeedSensitivity)
{
	FractalParams P;
	P.Octaves = 5;
	const uint64 Seed = 0xfaceull;
	double Sum = 0.0;
	double SumSq = 0.0;
	double Lo = 1e9;
	double Hi = -1e9;
	const int32 N = 256;
	const Fix64 Scale = Fix64::FromRatio(1, 32); // 8 lattice cells across the sample
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const double V = D(Fractal2D(Seed, Scale * X, Scale * Y, P));
			Sum += V;
			SumSq += V * V;
			Lo = V < Lo ? V : Lo;
			Hi = V > Hi ? V : Hi;
		}
	}
	const double Mean = Sum / (N * N);
	const double StdDev = std::sqrt(SumSq / (N * N) - Mean * Mean);
	VT_CHECK(std::fabs(Mean) < 0.05);
	VT_CHECK(StdDev > 0.08 && StdDev < 0.4);
	VT_CHECK(Lo >= -1.0 && Hi <= 1.0);
	VT_CHECK(Lo < -0.25 && Hi > 0.25);

	// Determinism and seed sensitivity, away from cell centres where gradient
	// noise takes quantised values that two seeds can share.
	const Fix64 PX = Q(0, 3, 7);
	const Fix64 PY = Q(0, 2, 5);
	VT_CHECK(Fractal2D(Seed, PX, PY, P) == Fractal2D(Seed, PX, PY, P));
	VT_CHECK(Fractal2D(Seed, PX, PY, P) != Fractal2D(Seed + 1, PX, PY, P));
	FractalParams One;
	One.Octaves = 1;
	VT_CHECK(Fractal2D(Seed, PX, PY, One) != Fractal2D(Seed, PX, PY, P));
	FractalParams Zero;
	Zero.Octaves = 0; // treated as 1
	VT_CHECK(Fractal2D(Seed, PX, PY, Zero) == Fractal2D(Seed, PX, PY, One));
	FractalParams ValueBased = P;
	ValueBased.UseGradient = false;
	VT_CHECK(Fractal2D(Seed, PX, PY, ValueBased) != Fractal2D(Seed, PX, PY, P));
	// Warping changes the field and stays bounded.
	const Fix64 W = Warped2D(Seed, PX, PY, Fix64::FromInt(2), P);
	VT_CHECK(W != Fractal2D(Seed, PX, PY, P));
	VT_CHECK(D(W) >= -1.0 && D(W) <= 1.0);
	VT_CHECK(Warped2D(Seed, PX, PY, Fix64::Zero(), P) == Fractal2D(Seed, PX, PY, P));
}

VAELEN_TEST(Noise, FrozenValuesAreReproducedByEveryCompilerAndPlatform)
{
	// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.02). A change here
	// changes every generated world and must be deliberate.
	FractalParams P;
	P.Octaves = 6;
	const Fix64 X = Q(12, 3, 7);
	const Fix64 Y = Q(-5, 1, 3);
	VT_CHECK_EQ(LatticeHash(0x1234, 5, -6), uint64{VAELEN_NOISE_FROZEN_HASH});
	VT_CHECK_EQ(static_cast<uint64>(Value2D(0x1234, X, Y).Raw), uint64{VAELEN_NOISE_FROZEN_VALUE});
	VT_CHECK_EQ(static_cast<uint64>(Gradient2D(0x1234, X, Y).Raw), uint64{VAELEN_NOISE_FROZEN_GRADIENT});
	VT_CHECK_EQ(static_cast<uint64>(Fractal2D(0x1234, X, Y, P).Raw), uint64{VAELEN_NOISE_FROZEN_FRACTAL});
	VT_CHECK_EQ(static_cast<uint64>(Warped2D(0x1234, X, Y, Fix64::FromInt(3), P).Raw),
				uint64{VAELEN_NOISE_FROZEN_WARPED});
	// A 128 x 128 fractal field digest.
	Hash64 Digest = HashConstants::Fnv1a64Offset;
	const Fix64 Scale = Fix64::FromRatio(1, 16);
	for (int32 J = 0; J < 128; ++J)
	{
		for (int32 I = 0; I < 128; ++I)
		{
			Digest =
				HashCombine(Digest, HashUInt64(static_cast<uint64>(Fractal2D(0x1234, Scale * I, Scale * J, P).Raw)));
		}
	}
	VT_CHECK_EQ(Digest, Hash64{VAELEN_NOISE_FROZEN_FIELD});
}
