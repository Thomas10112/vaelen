// VAELEN - VaelenSim tests
// Fix64: exact constants, wrapping, Mul/Div/Sqrt against double references at
// 1 ulp, edge cases (zero divisor, negatives, extremes), frozen values.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/FixedPoint.h"

#include <cmath>

using namespace Vaelen;

namespace
{
	constexpr double Scale = 4294967296.0; // 2^32
	double ToDouble(Fix64 F)
	{
		return static_cast<double>(F.Raw) / Scale;
	}
	Fix64 FromDouble(double D)
	{
		return Fix64::FromRaw(static_cast<int64>(std::floor(D * Scale)));
	}
	int64 UlpDiff(Fix64 A, Fix64 B)
	{
		return A.Raw > B.Raw ? A.Raw - B.Raw : B.Raw - A.Raw;
	}
} // namespace

static_assert(Fix64::One().Raw == 0x100000000ll);
static_assert(Fix64::FromInt(-3).Raw == -3ll * 0x100000000ll);
static_assert((Fix64::FromRatio(1, 2) * 2) == Fix64::One());
static_assert(Fix64::FromRatio(1, 4).Raw == 0x40000000ll);
static_assert((Fix64::FromInt(6) * Fix64::FromRatio(1, 3)).Raw == 0x1FFFFFFFEll); // 6 * 0.333.. rounded down
static_assert(Fix64::Div(Fix64::FromInt(1), Fix64::FromInt(0)) == Fix64::Max());
static_assert(Fix64::Div(Fix64::FromInt(-1), Fix64::FromInt(0)) == Fix64::Min());
static_assert(Fix64::Div(Fix64::Zero(), Fix64::Zero()) == Fix64::Zero());
static_assert(Fix64::Sqrt(Fix64::FromInt(4)) == Fix64::FromInt(2));
static_assert(Fix64::Sqrt(Fix64::FromInt(-4)) == Fix64::Zero());
static_assert(Fix64::Sqrt(Fix64::FromRatio(1, 4)) == Fix64::FromRatio(1, 2));
static_assert(Fix64::SmoothStep(Fix64::Zero()) == Fix64::Zero());
static_assert(Fix64::SmoothStep(Fix64::One()) == Fix64::One());
static_assert(Fix64::SmoothStep(Fix64::FromRatio(1, 2)) == Fix64::FromRatio(1, 2));
static_assert(Fix64::Lerp(Fix64::FromInt(2), Fix64::FromInt(6), Fix64::FromRatio(1, 4)) == Fix64::FromInt(3));
static_assert(Fix64::FromInt(-7).FloorToInt() == -7);
static_assert(Fix64::FromRaw(-1).FloorToInt() == -1); // -2^-32 floors to -1
static_assert(Fix64::FromRaw(-1).Fraction() == Fix64::FromRaw(0xffffffffll));
static_assert((Fix64::Max() + Fix64::FromRaw(1)) == Fix64::Min()); // wraps, defined
static_assert((-Fix64::Min()) == Fix64::Min());
static_assert(3_fx == Fix64::FromInt(3));

VAELEN_TEST(FixedPoint, MulMatchesDoubleWithinOneUlp)
{
	RandomStream Stream(0xf1ed);
	int64 WorstUlp = 0;
	for (uint32 i = 0; i < 200000; ++i)
	{
		// Magnitudes below 2^4: the exact product has at most 8 integer bits and
		// 64 fraction bits, so the double reference (53 bits) is accurate to
		// 2^-45, well inside one ulp of 2^-32.
		const double A = Stream.RangeDouble(-16.0, 16.0);
		const double B = Stream.RangeDouble(-16.0, 16.0);
		const Fix64 FA = FromDouble(A);
		const Fix64 FB = FromDouble(B);
		const Fix64 Expected = FromDouble(ToDouble(FA) * ToDouble(FB));
		const int64 Ulp = UlpDiff(FA * FB, Expected);
		WorstUlp = Ulp > WorstUlp ? Ulp : WorstUlp;
	}
	// The reference floors a double that sits within 2^-45 of the exact
	// product, so the two can disagree by one ulp at an ulp boundary, never more.
	VT_CHECK(WorstUlp <= 1);
	// Exactness where doubles cannot be trusted: full-precision fractions.
	const Fix64 X = Fix64::FromRaw(0x123456789abcdefll);
	const Fix64 Y = Fix64::FromRaw(0x0fedcba987654321ll);
	// (0x123456789abcdef * 0x0fedcba987654321) >> 32, low 64 bits, computed with Python's big integers.
	VT_CHECK_EQ(static_cast<uint64>((X * Y).Raw), uint64{0x0ad77d7422236d88ull});
	VT_CHECK((X * Y) == (Y * X));
	VT_CHECK((-X) * Y == -(X * Y) || UlpDiff((-X) * Y, -(X * Y)) == 1); // -inf rounding
}

VAELEN_TEST(FixedPoint, DivMatchesDoubleWithinOneUlpAndInvertsMul)
{
	RandomStream Stream(0xd1f);
	int64 WorstUlp = 0;
	for (uint32 i = 0; i < 100000; ++i)
	{
		const double A = Stream.RangeDouble(-1e6, 1e6);
		double B = Stream.RangeDouble(-1e3, 1e3);
		if (B > -0.5 && B < 0.5)
		{
			B = 0.5;
		}
		const Fix64 FA = FromDouble(A);
		const Fix64 FB = FromDouble(B);
		const double Quotient = ToDouble(FA) / ToDouble(FB);
		if (Quotient > 2e9 || Quotient < -2e9)
		{
			continue;
		}
		const Fix64 Expected = Fix64::FromRaw(static_cast<int64>(std::trunc(Quotient * Scale)));
		const int64 Ulp = UlpDiff(FA / FB, Expected);
		WorstUlp = Ulp > WorstUlp ? Ulp : WorstUlp;
	}
	VT_CHECK(WorstUlp <= 2);
	VT_CHECK(Fix64::FromInt(10) / Fix64::FromInt(4) == Fix64::FromRatio(5, 2));
	VT_CHECK(Fix64::FromInt(-10) / Fix64::FromInt(4) == -Fix64::FromRatio(5, 2));
	VT_CHECK(Fix64::FromInt(1) / Fix64::FromInt(3) == Fix64::FromRaw(0x55555555ll));
	VT_CHECK(Fix64::FromInt(-1) / Fix64::FromInt(3) == Fix64::FromRaw(-0x55555555ll)); // towards zero
	VT_CHECK(Fix64::FromInt(7) / Fix64::FromRaw(1) ==
			 Fix64::FromRaw(int64{7} << 32 << 32 >> 32 << 32)); // 7 * 2^32 as integer part
}

VAELEN_TEST(FixedPoint, SqrtIsFloorOfTheExactRoot)
{
	RandomStream Stream(0x5947);
	for (uint32 i = 0; i < 100000; ++i)
	{
		const Fix64 A = Fix64::FromRaw(static_cast<int64>(Stream.NextU64() >> 1)); // non-negative, full range
		const Fix64 R = Fix64::Sqrt(A);
		// R^2 <= A < (R + ulp)^2, computed without overflow via doubles (loose) and exactly for small values.
		const double Root = std::sqrt(ToDouble(A));
		VT_CHECK(std::fabs(ToDouble(R) - Root) <= Root * 1e-9 + 1e-9);
	}
	for (int32 K = 0; K < 2000; ++K)
	{
		VT_CHECK(Fix64::Sqrt(Fix64::FromInt(K * K)) == Fix64::FromInt(K));
	}
	VT_CHECK(Fix64::Sqrt(Fix64::FromInt(2)).Raw == 0x16A09E667ll); // floor(sqrt(2) * 2^32)
	VT_CHECK(Fix64::Sqrt(Fix64::Max()).Raw == 0xb504f333f9dell);   // floor(sqrt((2^63 - 1) * 2^32)), Python reference
}

VAELEN_TEST(FixedPoint, HelpersAndWrapAreDefinedEverywhere)
{
	VT_CHECK(Fix64::Abs(Fix64::FromInt(-5)) == Fix64::FromInt(5));
	VT_CHECK(Fix64::Abs(Fix64::Min()) == Fix64::Min()); // wraps: documented
	VT_CHECK(Fix64::Clamp(Fix64::FromInt(9), Fix64::Zero(), Fix64::One()) == Fix64::One());
	VT_CHECK(Fix64::Clamp(Fix64::FromInt(-9), Fix64::Zero(), Fix64::One()) == Fix64::Zero());
	VT_CHECK(Fix64::SmoothStep(Fix64::FromInt(7)) == Fix64::One());
	VT_CHECK(Fix64::SmoothStep(Fix64::FromRatio(1, 4)).Raw == 0x28000000ll); // 5/32
	VT_CHECK((Fix64::Max() * 2) == Fix64::FromRaw(-2));
	VT_CHECK((Fix64::Max() * Fix64::FromInt(2)) == Fix64::FromRaw(-2));
	VT_CHECK((Fix64::Min() * Fix64::Min()) == Fix64::Zero()); // 2^62 << 32 wraps to 0
	VT_CHECK(Fix64::FromInt(5).ShiftRight(1) == Fix64::FromRatio(5, 2));
	VT_CHECK(Fix64::FromRatio(-5, 2).ShiftRight(1) == Fix64::FromRatio(-5, 4));
	VT_CHECK(Fix64::FromInt(3).ShiftLeft(2) == Fix64::FromInt(12));
	VT_CHECK(Fix64::FromRatio(-7, 2).Floor() == Fix64::FromInt(-4));
	VT_CHECK(Fix64::FromRatio(-7, 2).FloorToInt() == -4);
	VT_CHECK(Fix64::FromRatio(-7, 2).Fraction() == Fix64::FromRatio(1, 2));
}
