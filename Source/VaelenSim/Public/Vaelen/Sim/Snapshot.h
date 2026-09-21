// VAELEN - VaelenSim
// Versioned, digest-checked snapshot of a World's state.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// Image layout (all little-endian, see CoreTypes.h):
//   Header  : magic "VAELENSN", FormatVersion u32 (VAELEN_SAVE_FORMAT_VERSION),
//             Flags u32 (0), LayoutDigest u64 (component layout combined with
//             the map layer layout), Seed u64
//   Clock   : tick u64, calendar rules (5 x u32)
//   Root    : RandomStreamState (seed, 4 words, draw count)
//   Ids     : 256 x u64 next serials
//   Entities: free head u32, alive count u32, slot count u64, slots (id u64,
//             generation u32, next-free u32, flags u8 = alive | retired << 1)
//   Pools   : count u32, then per pool: type id u16, name hash u64, element
//             size u32, entities (count u64 + raw), data (count u64 + raw)
//   Map     : WorldGenConfig raw, width u32, height u32, layer count u32, then
//             per layer: name hash u64, element size u32, values (count u64 + raw)
//   Pending : events not yet delivered (count u64 + raw)
//   Log     : count u64, digest u64, events raw
//   Trailer : FNV-1a digest u64 over every preceding byte
// The image of two identical worlds is byte-identical; a restored world
// continues exactly like the original (01.07).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/SimApi.h"

#include <vector>

namespace Vaelen
{
	class World;

	enum class SnapshotResult : uint8
	{
		Ok,
		BadMagic,
		VersionMismatch,
		LayoutMismatch, ///< component type set differs from the loading world's
		MissingPool,	///< the loading world has no pool for a saved type
		Truncated,
		Corrupt,	  ///< trailer digest or an internal consistency check failed
		Inconsistent, ///< the world state itself failed validation (registry, pools)
		/// THE ONE RESULT THAT MEANS THE TARGET IS NOT SAFE TO CARRY ON WITH.
		/// `LoadSnapshot` keeps the world it is about to overwrite and puts it
		/// back when the image refuses partway; this says the putting back
		/// itself failed, so the world is neither what it was nor what the
		/// image held. Every other refusal leaves the target untouched. A
		/// caller that cannot tell these apart cannot decide whether to offer
		/// the player their game back, which is the whole reason it is its own
		/// value and not folded into `Inconsistent`.
		RollbackFailed,
	};

	VAELEN_SIM_API const char* SnapshotResultToString(SnapshotResult Result) noexcept;

	/// Appends the world's state image to Out, and says so when it cannot.
	///
	/// ON ANY REFUSAL Out IS LEFT EXACTLY AS IT WAS FOUND - the function
	/// truncates back to the size it was given - so a caller that appends to a
	/// buffer, or writes what it gets, never handles bytes nobody meant.
	/// `Inconsistent` for a world that is dispatching events; `Truncated` when
	/// the writer ran out; whatever a pool or a layer said, otherwise.
	///
	/// IT RETURNED void UNTIL 2026-09-21, and that was the one live data-loss
	/// path Phase 16's planning found. A failing body was reported by
	/// `VAELEN_CHECKF`, which `Assert.h` compiles to `((void)0)` under `NDEBUG`
	/// and under `UE_BUILD_SHIPPING` - so in a player's build nothing happened
	/// and the trailer was computed over the SHORT bytes that had been written.
	///
	/// AND THE SENTENCE THAT USED TO END THIS PARAGRAPH WAS WRONG. It said the
	/// file was "well-formed, wrong, and validated on load". Measured at four
	/// cut points instead of repeated: a resealed short image is REFUSED,
	/// `Truncated` every time, because the reader runs out inside a section.
	/// The defect was never a corrupted load. It was that the caller learned
	/// nothing at the moment of WRITING - a player told their game was saved,
	/// finding out otherwise only on opening it, with the world it came from
	/// already gone. `Snapshot.AShortImageIsRefusedButTheWriterNeverKnew`
	/// carries the correction where it cannot rot.
	///
	/// NOT `[[nodiscard]]`, and that is a decision rather than an oversight.
	/// Ninety call sites in this repository ignore the result today; marking it
	/// would put ninety mechanical edits into the commit that fixes the defect,
	/// which is exactly the shape of change that hides a mistake. The defect
	/// was that this function COULD NOT report, not that tests do not listen.
	/// 16.04 has the one production caller and checks it; a later task may add
	/// the attribute once the call sites are worth touching for their own sake.
	VAELEN_SIM_API SnapshotResult SaveSnapshot(const World& Source, std::vector<uint8>& Out);

	/// Replaces the world's state with the image, or leaves it exactly alone.
	/// The world must have been set up with the same component types and pools
	/// (the same setup code).
	///
	/// ALL OR NOTHING, SINCE 2026-09-21. This used to say "on failure the
	/// world's state is unspecified (callers discard it)", and that sentence
	/// was doing a great deal of quiet work: `Run::Aelvor::Adopt` owns its
	/// world and has nothing to discard it in favour of, and neither does a
	/// player loading a save over the game they are in.
	///
	/// The reason it said so is that the apply runs section by section - clock,
	/// streams, ids, entities, every component pool, the map - committing each
	/// before the next is read, and pools and the map deserialise IN PLACE. A
	/// refusal at the map left every pool already overwritten. Measured at four
	/// cut points of a resealed image: refused honestly every time, and the
	/// world left on a digest that was neither the one it had nor the one in
	/// the image. A chimera.
	///
	/// Now the world is kept - with `SaveSnapshot`, before the first byte of it
	/// is overwritten - and put back on any refusal. Every result below except
	/// one therefore means the target is untouched. The exception is
	/// `RollbackFailed`, which exists precisely so that the one case where this
	/// promise could not be kept is not silently reported as one where it was.
	///
	/// The keeping costs one image of the TARGET world, and only on a load that
	/// gets past the header: a bad trailer, magic, version, layout or seed is
	/// refused without keeping anything, and those are the common refusals.
	/// Measured at 1.3 MB in 8.7 ms, restored exactly.
	VAELEN_SIM_API SnapshotResult LoadSnapshot(World& Target, const uint8* Bytes, usize Size);

	/// Digest of the world state: the trailer digest of its image.
	VAELEN_SIM_API Hash64 ComputeStateDigest(const World& Source);
} // namespace Vaelen
