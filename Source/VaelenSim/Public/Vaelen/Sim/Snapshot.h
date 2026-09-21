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
	/// and under `UE_BUILD_SHIPPING` - so in a player's build nothing happened,
	/// the trailer was computed over the SHORT bytes that had been written, and
	/// the file was well-formed, wrong, and validated on load.
	///
	/// NOT `[[nodiscard]]`, and that is a decision rather than an oversight.
	/// Ninety call sites in this repository ignore the result today; marking it
	/// would put ninety mechanical edits into the commit that fixes the defect,
	/// which is exactly the shape of change that hides a mistake. The defect
	/// was that this function COULD NOT report, not that tests do not listen.
	/// 16.04 has the one production caller and checks it; a later task may add
	/// the attribute once the call sites are worth touching for their own sake.
	VAELEN_SIM_API SnapshotResult SaveSnapshot(const World& Source, std::vector<uint8>& Out);

	/// Replaces the world's state with the image. The world must have been set
	/// up with the same component types and pools (the same setup code); on
	/// failure the result says why and the world's state is unspecified
	/// (callers discard it).
	VAELEN_SIM_API SnapshotResult LoadSnapshot(World& Target, const uint8* Bytes, usize Size);

	/// Digest of the world state: the trailer digest of its image.
	VAELEN_SIM_API Hash64 ComputeStateDigest(const World& Source);
} // namespace Vaelen
