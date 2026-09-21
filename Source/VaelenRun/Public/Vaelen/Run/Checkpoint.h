// VAELEN - VaelenRun
// The container: bytes, and not a file.
//
// STATUS: PROTOTYPE (Phase 16 task 16.04) - round trip, section table and flag
//         halves are tested in Tests/Run/Test_Checkpoint.cpp; the RUN, HOST and
//         STREAM sections are declared here and filled by 16.05-16.07.
//
// WHY A CONTAINER AND NOT A BIGGER IMAGE. `SaveSnapshot`'s trailer is computed
// over bytes that include the format version and the layout digest, and
// `ComputeStateDigest` IS that trailer - so every frozen digest this project
// owns hangs off it. Adding one field to the image moves all of them at once,
// across fifteen phases of gates. The phase's design rule follows from that:
// nothing is ever added to the image, and everything the phase needs lives in a
// container OUTSIDE it. The STATE section below is the output of `SaveSnapshot`
// copied VERBATIM, never re-encoded, which is what keeps that promise checkable
// by `memcmp` rather than by argument.
//
// Container layout (little-endian throughout):
//   Magic       : "VAELENCP"
//   Version     : u32, this container's own, INDEPENDENT of
//                 VAELEN_SAVE_FORMAT_VERSION - the point of a container is that
//                 the two can move separately
//   Flags       : u32, split in half (see below)
//   InnerFormat : u32, the image's format version, copied out of the STATE
//                 section rather than assumed, so a reader can say which of the
//                 two versions it disagrees with
//   Seed        : u64
//   Tick        : u64
//   LogEvents   : u64  \  the log is about three quarters of any image with a
//   LogBytes    : u64  /  history; a reader that wants to know why a save is
//                         large should not have to parse the image to find out
//   SectionCount: u32
//   Table       : SectionCount x { Kind u16, Flags u32, Offset u64, Length u64,
//                 Digest u64 } - offsets are from the first byte of the container
//   Payloads    : the sections, in table order
//   Trailer     : FNV-1a u64 over every preceding byte
//
// THE FLAG HALVES are the channel the image's own `Flags` word was reserved for
// in Phase 01 and never given: it is written 0 and never read (defect 4).
//   low 16 bits  MUST-UNDERSTAND. A reader that meets a set bit it does not
//                know REFUSES, and names the bit. This is how a future section
//                that changes the MEANING of the others can be added without a
//                version bump that invalidates every older file.
//   high 16 bits MAY-IGNORE. Carried through a read-then-write untouched, so a
//                tool that does not understand a bit cannot silently drop it.
//
// NO PATH, NO <fstream>, NO <filesystem>. This module is compiled by UBT as
// well as CMake and the kernel purity checker forbids it an engine header;
// WHERE a checkpoint is written is the host's business (Unreal's SaveGame
// directory on Windows, a temporary file in a test), and it is not decided here.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Version.h"
#include "Vaelen/Run/RunApi.h"

#include <vector>

namespace Vaelen::Run
{
	class Aelvor;

	/// "VAELENCP" - eight bytes, matching the image's own magic in shape so
	/// that a reader handed the wrong one of the two says so immediately.
	inline constexpr char CheckpointMagic[8] = {'V', 'A', 'E', 'L', 'E', 'N', 'C', 'P'};

	/// The container's version, deliberately not the image's.
	inline constexpr uint32 CheckpointVersion = 1u;

	enum class SectionKind : uint16
	{
		/// `SaveSnapshot`'s output, byte for byte. Never re-encoded.
		State = 1,
		/// `Aelvor`'s own state - Begun_, Detail_, Dug_, Near_, Watched_ - which
		/// lives OUTSIDE the image and is the defect this phase exists for.
		/// Declared here, filled by 16.05.
		Run = 2,
		/// What the host was doing: the camera, the cadence. 16.07.
		Host = 3,
		/// The recorded input stream, when a save is asked to carry its own
		/// tape. 16.07, and it is one of the questions still open for the owner.
		Stream = 4,
	};

	enum class CheckpointResult : uint8
	{
		Ok,
		BadMagic,
		/// The CONTAINER's version is not one this build writes.
		VersionMismatch,
		/// The IMAGE's version inside it is not. A separate value from the one
		/// above because the whole point of the container is that they differ.
		InnerVersionMismatch,
		Truncated,
		/// The trailer, or a section digest, disagrees with the bytes.
		Corrupt,
		/// A must-understand flag bit this build does not know. `UnknownBit`
		/// says which one.
		UnknownRequiredFlag,
		/// A section table that does not describe the bytes: overlapping
		/// sections, a section past the end, or lengths that do not add up.
		BadSectionTable,
		/// `SaveSnapshot` refused the world, so there is nothing to put in the
		/// STATE section. Whatever it said is in `Inner`.
		StateRefused,
	};

	VAELEN_RUN_API const char* CheckpointResultToString(CheckpointResult Result) noexcept;

	struct SectionEntry
	{
		uint16 Kind = 0;
		uint32 Flags = 0;
		uint64 Offset = 0;
		uint64 Length = 0;
		/// FNV-1a over this section's bytes alone. IT IS HERE AND NOT IN THE
		/// IMAGE, which is what lets a per-section check be added at zero digest
		/// cost - the image is not touched, so no frozen digest moves.
		uint64 Digest = 0;
	};

	/// What a reader learned. The sections are described, not copied: `Bytes`
	/// still owns them, and `Section` hands back a view into it.
	struct CheckpointView
	{
		uint32 Version = 0;
		uint32 Flags = 0;
		uint32 InnerFormat = 0;
		uint64 Seed = 0;
		uint64 Tick = 0;
		uint64 LogEvents = 0;
		uint64 LogBytes = 0;
		std::vector<SectionEntry> Sections;
		/// The container's first byte, so that Find can hand back a view.
		/// Valid only while the bytes ReadCheckpoint was given are alive.
		const uint8* Base = nullptr;

		/// The bytes of a section, or {nullptr, 0} when it is not present.
		VAELEN_RUN_API const uint8* Find(SectionKind Kind, uint64& OutLength) const noexcept;
	};

	/// Why a read refused, in as much detail as the refusal has.
	struct CheckpointRefusal
	{
		CheckpointResult Result = CheckpointResult::Ok;
		/// For UnknownRequiredFlag: which bit of the low half, 0-15.
		uint32 UnknownBit = 0;
		/// For BadSectionTable and Corrupt: which table entry, or the count
		/// when the fault is the table as a whole.
		uint32 Section = 0;
	};

	/// Builds a checkpoint of the run into Out.
	///
	/// ON ANY REFUSAL Out IS LEFT EXACTLY AS IT WAS FOUND, which is the rule
	/// 16.02 established for `SaveSnapshot` and for the same reason: a caller
	/// that appends, or writes what it gets, must never handle bytes nobody
	/// meant. `MayIgnore` is carried into the high half of the container flags.
	VAELEN_RUN_API CheckpointResult BuildCheckpoint(const Aelvor& Run, std::vector<uint8>& Out,
													uint16 MayIgnore = 0);

	/// Reads a checkpoint's header and section table. It does NOT apply
	/// anything to a world - that is 16.06's `Adopt`, which is the only caller
	/// entitled to decide what a section means.
	VAELEN_RUN_API CheckpointRefusal ReadCheckpoint(const uint8* Bytes, usize Size, CheckpointView& Out);
} // namespace Vaelen::Run
