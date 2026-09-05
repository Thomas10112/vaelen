// VAELEN - VaelenSim
// Deterministic lattice noise over Fix64 coordinates: value noise, gradient
// noise, fractal sums and domain warping, all integer arithmetic.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Lattice values come from a 64-bit mixer of (seed, x, y); interpolation is
// bilinear with SmoothStep weights in Q32.32. Every function is a pure
// function of its arguments, so the same seed and coordinates give the same
// value on every compiler and platform (guarded by frozen values in the tests).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/FixedPoint.h"
#include "Vaelen/Sim/SimApi.h"

namespace Vaelen
{
	namespace Noise
	{
		/// SplitMix64-style mixer of a lattice point; every bit depends on every input bit.
		constexpr uint64 LatticeHash(uint64 Seed, int32 X, int32 Y) noexcept
		{
			uint64 Z = Seed + 0x9e3779b97f4a7c15ull * (static_cast<uint64>(static_cast<uint32>(X)) + 1u) +
					   0xbf58476d1ce4e5b9ull * (static_cast<uint64>(static_cast<uint32>(Y)) + 1u);
			Z = (Z ^ (Z >> 30)) * 0xbf58476d1ce4e5b9ull;
			Z = (Z ^ (Z >> 27)) * 0x94d049bb133111ebull;
			return Z ^ (Z >> 31);
		}

		/// Lattice value in [-1, 1): the top 33 bits of the hash as a signed Q32.32 fraction.
		constexpr Fix64 LatticeValue(uint64 Seed, int32 X, int32 Y) noexcept
		{
			const uint64 H = LatticeHash(Seed, X, Y);
			// 33 bits: sign + 32 fraction bits -> raw in [-2^32, 2^32).
			return Fix64::FromRaw(static_cast<int64>(H >> 31) - (int64{1} << 32));
		}

		/// Value noise at (X, Y): smooth bilinear blend of the four surrounding
		/// lattice values, in [-1, 1).
		VAELEN_SIM_API Fix64 Value2D(uint64 Seed, Fix64 X, Fix64 Y) noexcept;

		/// Gradient (Perlin-style) noise with 8 integer gradient directions, in
		/// about [-1, 1] (exact bound depends on the direction set; the tests
		/// measure it). Zero at every lattice point.
		VAELEN_SIM_API Fix64 Gradient2D(uint64 Seed, Fix64 X, Fix64 Y) noexcept;

		struct FractalParams
		{
			uint32 Octaves = 4;
			Fix64 Lacunarity = Fix64::FromInt(2); ///< frequency multiplier per octave
			Fix64 Gain = Fix64::FromRatio(1, 2);  ///< amplitude multiplier per octave
			bool UseGradient = true;
		};

		/// Sum of octaves, normalised so the result stays in the base range.
		/// Each octave derives its seed from the base seed and the octave index.
		VAELEN_SIM_API Fix64 Fractal2D(uint64 Seed, Fix64 X, Fix64 Y, const FractalParams& Params) noexcept;

		/// Domain warp: offsets (X, Y) by Strength * fractal noise from two
		/// derived seeds, then samples the fractal at the warped point.
		VAELEN_SIM_API Fix64 Warped2D(uint64 Seed, Fix64 X, Fix64 Y, Fix64 Strength,
									  const FractalParams& Params) noexcept;
	} // namespace Noise
} // namespace Vaelen
