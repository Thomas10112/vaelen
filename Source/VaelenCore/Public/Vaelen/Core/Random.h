// VAELEN - VaelenCore
// Deterministic random streams.
//
// STATUS: VALIDATED (Phase 00)
//
// Generator: xoshiro256** (Blackman & Vigna) seeded through SplitMix64.
//   - 256-bit state, period 2^256 - 1, excellent statistical quality.
//   - No floating point in the generator itself; NextDouble() derives a
//     53-bit uniform from integer bits, so sequences are bit-identical across
//     platforms and compilers.
//
// Streams are hierarchical: every simulation domain derives its own stream
// from its parent by NAME (`Derive("hydrology")`) or by index (`Fork(i)`),
// so adding a random call in one system can never perturb another system's
// sequence. This is the foundation of the determinism rule (Master Prompt §35).
//
// Never use rand(), std::random_device or std::mt19937 in simulation code.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"

#include <string_view>

namespace Vaelen
{
	/// Serializable state of a RandomStream (persisted verbatim in saves).
	struct RandomStreamState
	{
		uint64 Seed = 0;	   ///< Root seed of this stream (used for derivation).
		uint64 S[4] = {0, 0, 0, 0}; ///< xoshiro256** state.
		uint64 DrawCount = 0;  ///< Number of NextU64() calls since seeding (diagnostics/replay).

		constexpr bool operator==(const RandomStreamState&) const = default;
	};

	class VAELENCORE_API RandomStream
	{
	public:
		/// A stream seeded with 0 is valid but MUST be reseeded before use in
		/// simulation code; callers should always pass an explicit seed.
		RandomStream() noexcept;
		explicit RandomStream(uint64 Seed) noexcept;
		explicit RandomStream(const RandomStreamState& State) noexcept;

		/// Re-initialises the stream from a 64-bit seed (SplitMix64 expansion).
		void Reseed(uint64 Seed) noexcept;

		/// Creates a child stream whose seed depends on this stream's ROOT SEED
		/// and the given name. Does not advance this stream.
		RandomStream Derive(std::string_view Name) const noexcept;
		RandomStream Derive(Hash64 NameHash) const noexcept;

		/// Creates a child stream indexed by an integer (e.g. one per region).
		/// Does not advance this stream.
		RandomStream Fork(uint64 Index) const noexcept;

		// ── Draws ────────────────────────────────────────────────────────────
		uint64 NextU64() noexcept;
		uint32 NextU32() noexcept;

		/// Uniform integer in [Min, Max] (inclusive). Unbiased (Lemire).
		/// Requires Min <= Max.
		int64 RangeInclusive(int64 Min, int64 Max) noexcept;
		uint64 RangeInclusiveU(uint64 Min, uint64 Max) noexcept;

		/// Uniform integer in [0, Count). Requires Count > 0.
		uint64 Below(uint64 Count) noexcept;

		/// Uniform double in [0, 1) with 53 bits of randomness.
		double NextDouble() noexcept;
		/// Uniform float in [0, 1) with 24 bits of randomness.
		float NextFloat() noexcept;

		/// Uniform double in [Min, Max).
		double RangeDouble(double Min, double Max) noexcept;

		/// Returns true with the given probability in [0, 1].
		bool Chance(double Probability) noexcept;

		/// Standard normal (mean 0, deviation 1), Marsaglia polar method.
		double NextNormal() noexcept;

		// ── State ────────────────────────────────────────────────────────────
		const RandomStreamState& GetState() const noexcept { return State; }
		void SetState(const RandomStreamState& InState) noexcept { State = InState; }
		uint64 GetSeed() const noexcept { return State.Seed; }
		uint64 GetDrawCount() const noexcept { return State.DrawCount; }

		/// Advances the stream by 2^128 draws; use to create non-overlapping
		/// sub-sequences when hierarchical derivation is not applicable.
		void Jump() noexcept;

	private:
		RandomStreamState State;
	};

	/// SplitMix64 step: returns the next output and advances the state.
	VAELENCORE_API uint64 SplitMix64Next(uint64& State) noexcept;
} // namespace Vaelen
