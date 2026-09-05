// VAELEN - VaelenSim
// Q32.32 fixed-point arithmetic for world generation: bit-identical on every
// compiler and platform, no floating point, no libm.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Fix64 holds a signed 64-bit raw value with 32 fractional bits: range
// [-2^31, 2^31), resolution 2^-32. Every operation is constexpr and defined
// for every input: arithmetic wraps in two's complement (computed on unsigned
// values, so never undefined), division by zero yields the saturated value of
// the dividend's sign, square roots of negatives are zero. The 64 x 64 -> 128
// multiplication uses 32-bit halves, not __int128, so MSVC and the others agree
// bit for bit.
#pragma once

#include "Vaelen/Core/CoreTypes.h"

namespace Vaelen
{
	struct Fix64
	{
		static constexpr uint32 FractionBits = 32;
		static constexpr int64 OneRaw = int64{1} << FractionBits;

		int64 Raw = 0;

		constexpr Fix64() noexcept = default;
		static constexpr Fix64 FromRaw(int64 InRaw) noexcept
		{
			Fix64 F;
			F.Raw = InRaw;
			return F;
		}
		static constexpr Fix64 FromInt(int32 Value) noexcept { return FromRaw(int64{Value} << FractionBits); }
		/// Numerator / Denominator, rounded towards zero at 2^-32 resolution.
		/// A zero denominator saturates.
		static constexpr Fix64 FromRatio(int32 Numerator, int32 Denominator) noexcept
		{
			return Div(FromInt(Numerator), FromInt(Denominator));
		}
		static constexpr Fix64 Zero() noexcept { return Fix64{}; }
		static constexpr Fix64 One() noexcept { return FromRaw(OneRaw); }
		static constexpr Fix64 Max() noexcept { return FromRaw(0x7fffffffffffffffll); }
		static constexpr Fix64 Min() noexcept { return FromRaw(-0x7fffffffffffffffll - 1); }

		/// Integer part, rounded towards negative infinity.
		constexpr int32 FloorToInt() const noexcept { return static_cast<int32>(Raw >> FractionBits); }
		/// Fractional part in [0, 1) as a raw Q32.32 value.
		constexpr Fix64 Fraction() const noexcept { return FromRaw(Raw & (OneRaw - 1)); }
		constexpr Fix64 Floor() const noexcept { return FromRaw(Raw & ~(OneRaw - 1)); }
		constexpr bool IsNegative() const noexcept { return Raw < 0; }

		constexpr bool operator==(const Fix64&) const noexcept = default;
		constexpr bool operator<(Fix64 O) const noexcept { return Raw < O.Raw; }
		constexpr bool operator>(Fix64 O) const noexcept { return Raw > O.Raw; }
		constexpr bool operator<=(Fix64 O) const noexcept { return Raw <= O.Raw; }
		constexpr bool operator>=(Fix64 O) const noexcept { return Raw >= O.Raw; }

		static constexpr int64 WrapAdd(int64 A, int64 B) noexcept
		{
			return static_cast<int64>(static_cast<uint64>(A) + static_cast<uint64>(B));
		}
		static constexpr int64 WrapSub(int64 A, int64 B) noexcept
		{
			return static_cast<int64>(static_cast<uint64>(A) - static_cast<uint64>(B));
		}
		static constexpr int64 WrapNeg(int64 A) noexcept
		{
			return static_cast<int64>(uint64{0} - static_cast<uint64>(A));
		}

		constexpr Fix64 operator+(Fix64 O) const noexcept { return FromRaw(WrapAdd(Raw, O.Raw)); }
		constexpr Fix64 operator-(Fix64 O) const noexcept { return FromRaw(WrapSub(Raw, O.Raw)); }
		constexpr Fix64 operator-() const noexcept { return FromRaw(WrapNeg(Raw)); }
		constexpr Fix64& operator+=(Fix64 O) noexcept
		{
			Raw = WrapAdd(Raw, O.Raw);
			return *this;
		}
		constexpr Fix64& operator-=(Fix64 O) noexcept
		{
			Raw = WrapSub(Raw, O.Raw);
			return *this;
		}
		constexpr Fix64 operator*(Fix64 O) const noexcept { return Mul(*this, O); }
		constexpr Fix64 operator/(Fix64 O) const noexcept { return Div(*this, O); }
		/// Exact scaling by an integer (wraps).
		constexpr Fix64 operator*(int32 K) const noexcept
		{
			return FromRaw(static_cast<int64>(static_cast<uint64>(Raw) * static_cast<uint64>(static_cast<int64>(K))));
		}
		/// Exact division by a non-zero power of two via shift (rounds towards -inf).
		constexpr Fix64 ShiftRight(uint32 Bits) const noexcept { return FromRaw(Raw >> Bits); }
		constexpr Fix64 ShiftLeft(uint32 Bits) const noexcept
		{
			return FromRaw(static_cast<int64>(static_cast<uint64>(Raw) << Bits));
		}

		/// (A * B) >> 32 with a full 128-bit intermediate; rounds towards -inf; wraps
		/// when the result leaves the range.
		static constexpr Fix64 Mul(Fix64 A, Fix64 B) noexcept
		{
			const bool Negative = (A.Raw < 0) != (B.Raw < 0);
			const uint64 UA = A.Raw < 0 ? uint64{0} - static_cast<uint64>(A.Raw) : static_cast<uint64>(A.Raw);
			const uint64 UB = B.Raw < 0 ? uint64{0} - static_cast<uint64>(B.Raw) : static_cast<uint64>(B.Raw);
			uint64 High = 0;
			uint64 Low = 0;
			MulU64(UA, UB, High, Low);
			// Product >> 32: take the low 32 bits of High and the high 32 bits of Low.
			const uint64 Magnitude = (High << 32) | (Low >> 32);
			if (!Negative)
			{
				return FromRaw(static_cast<int64>(Magnitude));
			}
			// Negative results round towards -inf: any discarded fraction bumps the magnitude.
			const uint64 Discarded = Low & 0xffffffffull;
			return FromRaw(static_cast<int64>(uint64{0} - (Magnitude + (Discarded != 0 ? 1u : 0u))));
		}

		/// (A << 32) / B with a 128-bit dividend; truncates towards zero; a zero
		/// divisor saturates to Max or Min by the dividend's sign (zero stays zero).
		static constexpr Fix64 Div(Fix64 A, Fix64 B) noexcept
		{
			if (B.Raw == 0)
			{
				return A.Raw == 0 ? Zero() : (A.Raw < 0 ? Min() : Max());
			}
			const bool Negative = (A.Raw < 0) != (B.Raw < 0);
			const uint64 UA = A.Raw < 0 ? uint64{0} - static_cast<uint64>(A.Raw) : static_cast<uint64>(A.Raw);
			const uint64 UB = B.Raw < 0 ? uint64{0} - static_cast<uint64>(B.Raw) : static_cast<uint64>(B.Raw);
			// Dividend = UA << 32 as (High, Low); long division bit by bit.
			uint64 High = UA >> 32;
			uint64 Low = UA << 32;
			uint64 Quotient = 0;
			uint64 Remainder = 0;
			// Divide the 128-bit (High:Low) by UB producing a 128-bit quotient; only
			// the low 64 bits are kept (wrap), matching Mul's wrapping contract.
			for (int Bit = 127; Bit >= 0; --Bit)
			{
				const uint64 Carry = Remainder >> 63;
				Remainder = (Remainder << 1) | (Bit >= 64 ? ((High >> (Bit - 64)) & 1u) : ((Low >> Bit) & 1u));
				if (Carry != 0 || Remainder >= UB)
				{
					Remainder -= UB;
					if (Bit < 64)
					{
						Quotient |= uint64{1} << Bit;
					}
				}
			}
			return FromRaw(Negative ? static_cast<int64>(uint64{0} - Quotient) : static_cast<int64>(Quotient));
		}

		/// Square root by integer Newton iteration on the raw value (exact for
		/// perfect squares, otherwise the floor at 2^-32 resolution). Negative
		/// inputs give zero.
		static constexpr Fix64 Sqrt(Fix64 A) noexcept
		{
			if (A.Raw <= 0)
			{
				return Zero();
			}
			// sqrt(raw / 2^32) * 2^32 = sqrt(raw * 2^32): compute isqrt of the 96-bit value
			// (raw << 32) with a digit-by-digit method on (High:Low).
			uint64 High = static_cast<uint64>(A.Raw) >> 32;
			uint64 Low = static_cast<uint64>(A.Raw) << 32;
			uint64 Result = 0;
			uint64 Rem = 0;
			for (int i = 0; i < 64; ++i)
			{
				// Bring down the next two bits of the 128-bit value.
				const uint64 TopTwo = High >> 62;
				High = (High << 2) | (Low >> 62);
				Low <<= 2;
				Rem = (Rem << 2) | TopTwo;
				const uint64 Trial = (Result << 2) | 1u;
				Result <<= 1;
				if (Rem >= Trial)
				{
					Rem -= Trial;
					Result |= 1u;
				}
			}
			return FromRaw(static_cast<int64>(Result));
		}

		static constexpr Fix64 Abs(Fix64 A) noexcept { return A.Raw < 0 ? -A : A; }
		static constexpr Fix64 MinOf(Fix64 A, Fix64 B) noexcept { return A.Raw < B.Raw ? A : B; }
		static constexpr Fix64 MaxOf(Fix64 A, Fix64 B) noexcept { return A.Raw > B.Raw ? A : B; }
		static constexpr Fix64 Clamp(Fix64 V, Fix64 Lo, Fix64 Hi) noexcept { return MinOf(MaxOf(V, Lo), Hi); }
		/// A + (B - A) * T for T in [0, 1] (T outside extrapolates).
		static constexpr Fix64 Lerp(Fix64 A, Fix64 B, Fix64 T) noexcept { return A + Mul(B - A, T); }
		/// 3t^2 - 2t^3 on [0, 1], clamped.
		static constexpr Fix64 SmoothStep(Fix64 T) noexcept
		{
			const Fix64 X = Clamp(T, Zero(), One());
			const Fix64 X2 = Mul(X, X);
			return Mul(X2, FromInt(3) - X * 2);
		}

	private:
		/// Portable 64 x 64 -> 128 unsigned multiply.
		static constexpr void MulU64(uint64 A, uint64 B, uint64& High, uint64& Low) noexcept
		{
			const uint64 A0 = A & 0xffffffffull;
			const uint64 A1 = A >> 32;
			const uint64 B0 = B & 0xffffffffull;
			const uint64 B1 = B >> 32;
			const uint64 P00 = A0 * B0;
			const uint64 P01 = A0 * B1;
			const uint64 P10 = A1 * B0;
			const uint64 P11 = A1 * B1;
			const uint64 Middle = (P00 >> 32) + (P01 & 0xffffffffull) + (P10 & 0xffffffffull);
			Low = (P00 & 0xffffffffull) | (Middle << 32);
			High = P11 + (P01 >> 32) + (P10 >> 32) + (Middle >> 32);
		}
	};
	static_assert(sizeof(Fix64) == 8);

	inline constexpr Fix64 operator""_fx(unsigned long long Value) noexcept // PURITY-ALLOW(R7): literal operators take
																			// unsigned long long by language rule
	{
		return Fix64::FromInt(static_cast<int32>(Value));
	}
} // namespace Vaelen
