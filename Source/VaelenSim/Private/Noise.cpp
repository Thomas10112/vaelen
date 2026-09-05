// VAELEN - VaelenSim
// Lattice noise implementation.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_Noise.cpp
#include "Vaelen/Sim/Noise.h"

namespace Vaelen::Noise
{
	namespace
	{
		struct Cell
		{
			int32 X0 = 0;
			int32 Y0 = 0;
			Fix64 FX; ///< fraction in [0, 1)
			Fix64 FY;
			Fix64 WX; ///< smoothstep weights
			Fix64 WY;
		};

		Cell Locate(Fix64 X, Fix64 Y) noexcept
		{
			Cell C;
			C.X0 = X.FloorToInt();
			C.Y0 = Y.FloorToInt();
			C.FX = X.Fraction();
			C.FY = Y.Fraction();
			C.WX = Fix64::SmoothStep(C.FX);
			C.WY = Fix64::SmoothStep(C.FY);
			return C;
		}

		/// Eight integer gradient directions (unit-ish: axis and diagonal).
		constexpr int32 GradientX[8] = {1, -1, 0, 0, 1, -1, 1, -1};
		constexpr int32 GradientY[8] = {0, 0, 1, -1, 1, 1, -1, -1};

		Fix64 DotGradient(uint64 Seed, int32 LX, int32 LY, Fix64 DX, Fix64 DY) noexcept
		{
			const uint32 G = static_cast<uint32>(LatticeHash(Seed, LX, LY) >> 61); // top 3 bits
			return DX * GradientX[G] + DY * GradientY[G];
		}

		constexpr uint64 OctaveSeed(uint64 Seed, uint32 Octave) noexcept
		{
			return LatticeHash(Seed ^ 0x6f63746176655f5full, static_cast<int32>(Octave), 0x4e4f4953);
		}
	} // namespace

	Fix64 Value2D(uint64 Seed, Fix64 X, Fix64 Y) noexcept
	{
		const Cell C = Locate(X, Y);
		const Fix64 V00 = LatticeValue(Seed, C.X0, C.Y0);
		const Fix64 V10 = LatticeValue(Seed, C.X0 + 1, C.Y0);
		const Fix64 V01 = LatticeValue(Seed, C.X0, C.Y0 + 1);
		const Fix64 V11 = LatticeValue(Seed, C.X0 + 1, C.Y0 + 1);
		const Fix64 Top = Fix64::Lerp(V00, V10, C.WX);
		const Fix64 Bottom = Fix64::Lerp(V01, V11, C.WX);
		return Fix64::Lerp(Top, Bottom, C.WY);
	}

	Fix64 Gradient2D(uint64 Seed, Fix64 X, Fix64 Y) noexcept
	{
		const Cell C = Locate(X, Y);
		const Fix64 One = Fix64::One();
		const Fix64 D00 = DotGradient(Seed, C.X0, C.Y0, C.FX, C.FY);
		const Fix64 D10 = DotGradient(Seed, C.X0 + 1, C.Y0, C.FX - One, C.FY);
		const Fix64 D01 = DotGradient(Seed, C.X0, C.Y0 + 1, C.FX, C.FY - One);
		const Fix64 D11 = DotGradient(Seed, C.X0 + 1, C.Y0 + 1, C.FX - One, C.FY - One);
		const Fix64 Top = Fix64::Lerp(D00, D10, C.WX);
		const Fix64 Bottom = Fix64::Lerp(D01, D11, C.WX);
		return Fix64::Lerp(Top, Bottom, C.WY);
	}

	Fix64 Fractal2D(uint64 Seed, Fix64 X, Fix64 Y, const FractalParams& Params) noexcept
	{
		Fix64 Sum;
		Fix64 Amplitude = Fix64::One();
		Fix64 Total;
		Fix64 Frequency = Fix64::One();
		const uint32 Octaves = Params.Octaves == 0 ? 1u : Params.Octaves;
		for (uint32 i = 0; i < Octaves; ++i)
		{
			const uint64 S = OctaveSeed(Seed, i);
			const Fix64 SX = X * Frequency;
			const Fix64 SY = Y * Frequency;
			const Fix64 N = Params.UseGradient ? Gradient2D(S, SX, SY) : Value2D(S, SX, SY);
			Sum += N * Amplitude;
			Total += Amplitude;
			Amplitude = Amplitude * Params.Gain;
			Frequency = Frequency * Params.Lacunarity;
		}
		return Total.Raw == 0 ? Fix64::Zero() : Sum / Total;
	}

	Fix64 Warped2D(uint64 Seed, Fix64 X, Fix64 Y, Fix64 Strength, const FractalParams& Params) noexcept
	{
		const Fix64 OX = Fractal2D(Seed ^ 0x77617270585f5f5full, X, Y, Params) * Strength;
		const Fix64 OY = Fractal2D(Seed ^ 0x77617270595f5f5full, X, Y, Params) * Strength;
		return Fractal2D(Seed, X + OX, Y + OY, Params);
	}
} // namespace Vaelen::Noise
