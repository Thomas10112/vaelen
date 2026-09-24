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
// Included rather than forward-declared since 16.05: RunState is Aelvor's
// nested type and ReadRunSection hands one back by value. No cycle - Aelvor.h
// knows nothing about the container.
#include "Vaelen/Player/Start.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Version.h"
#include "Vaelen/Run/RunApi.h"

#include <vector>

namespace Vaelen::Run
{
	/// "VAELENCP" - eight bytes, matching the image's own magic in shape so
	/// that a reader handed the wrong one of the two says so immediately.
	inline constexpr char CheckpointMagic[8] = {'V', 'A', 'E', 'L', 'E', 'N', 'C', 'P'};

	/// The container's version, deliberately not the image's.
	///
	/// 2 since 16.11, which added the STREAM section. The bump is deliberate
	/// even though the section table is variable-length and would have carried
	/// it silently: a build that did not understand STREAM would load the world
	/// and DROP ITS PROVENANCE without a word, and a save that quietly forgets
	/// how it came to exist is the thing this task exists to prevent. Refusing
	/// is the honest answer. Nothing has shipped, so no v1 container exists
	/// outside a test run and `BuiltInUpgrades()` stays empty.
	inline constexpr uint32 CheckpointVersion = 2u;

	enum class SectionKind : uint16
	{
		/// `SaveSnapshot`'s output, byte for byte. Never re-encoded.
		State = 1,
		/// `Aelvor::RunState` - Begun, Detail, Dug, Eyes, Near, Watched - which
		/// lives OUTSIDE the image and is the defect this phase exists for.
		/// Declared here, filled by 16.05.
		Run = 2,
		/// The `Options` the saving host DECLARED - size, pre-history, years,
		/// seed and the four wiring flags. 16.10, and it cost no container
		/// version because the table is variable-length.
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
	VAELEN_RUN_API CheckpointResult BuildCheckpoint(const Aelvor& Run, std::vector<uint8>& Out, uint16 MayIgnore = 0);

	/// The same, carrying the tape this world was played on.
	///
	/// The `Door` is not taken directly, so this header goes on knowing nothing
	/// about it; what travels is named rather than implied.
	VAELEN_RUN_API CheckpointResult BuildCheckpoint(const Aelvor& Run, const Player::InputStream& Tape,
													const Player::StartRules& Rules, std::vector<uint8>& Out,
													uint16 MayIgnore = 0);

	/// Decodes the STREAM section. False when there is none, or its bytes are
	/// not a stream this build reads.
	VAELEN_RUN_API bool ReadStreamSection(const CheckpointView& View, Player::InputStream& Tape,
										  Player::StartRules& Rules);

	/// ONE STEP OF A MIGRATION: it takes a container at version `From` and
	/// leaves it at `From + 1`, or returns false having changed nothing.
	///
	/// Steps are never allowed to skip. A save four versions old goes through
	/// every upgrader in between, in order, because each was written knowing
	/// only what the one before it produced.
	struct Upgrade
	{
		uint32 From = 0;
		bool (*Step)(std::vector<uint8>& Bytes) = nullptr;
		/// What this step changed, for the log line a failed migration writes.
		const char* Why = nullptr;
	};

	/// A table of steps. Passed in rather than registered globally: a global
	/// registry would need a way for tests to clear it, and a seam that exists
	/// only for tests is a seam production can fall through.
	struct UpgradePath
	{
		const Upgrade* Steps = nullptr;
		usize Count = 0;
	};

	enum class MigrateResult : uint8
	{
		Ok,
		/// Already at the version asked for. Not an error.
		NothingToDo,
		/// The container is NEWER than this build. Refused and never touched -
		/// a newer format is not a thing an older build may guess at.
		FromTheFuture,
		/// The chain has a gap: no step from the version in `At`.
		NoUpgrader,
		/// A step ran and refused. `At` is the version it was leaving.
		StepFailed,
	};

	VAELEN_RUN_API const char* MigrateResultToString(MigrateResult Result) noexcept;

	struct MigrateReport
	{
		MigrateResult Result = MigrateResult::Ok;
		/// Where it stopped, for NoUpgrader and StepFailed.
		uint32 At = 0;
		/// How many steps ran.
		uint32 Ran = 0;
	};

	/// The upgraders this build ships. EMPTY TODAY, and that is a fact rather
	/// than an omission: `VAELEN_SAVE_FORMAT_VERSION` is 3, the container is at
	/// 1, and this project has never shipped a game - so no save older than the
	/// current one exists anywhere in the world. The machinery is here because
	/// it has to exist BEFORE the version that needs it, not after.
	VAELEN_RUN_API UpgradePath BuiltInUpgrades() noexcept;

	/// Walks `Bytes` from `From` up to `To`, one step at a time.
	///
	/// ON ANY REFUSAL Bytes IS LEFT EXACTLY AS IT WAS FOUND, which is the rule
	/// this file has followed since 16.02: a half-migrated container is a
	/// container nobody can read and nobody knows not to trust.
	VAELEN_RUN_API MigrateReport Migrate(std::vector<uint8>& Bytes, uint32 From, uint32 To, const UpgradePath& Path);

	/// Decodes the HOST section: the world the saving host DECLARED. False when
	/// the container has none, or its bytes are not an `Options`.
	VAELEN_RUN_API bool ReadHostSection(const CheckpointView& View, Options& Out);

	/// Decodes the RUN section into a `RunState`. False when the checkpoint has
	/// no RUN section, or when its bytes do not describe one - which, because
	/// the section digest has already agreed with them, means the section was
	/// written by a build whose `RunState` is a different shape.
	///
	/// It does NOT apply the state: `Aelvor::SetRunState` decides whether this
	/// world can hold it, and refuses a region the map does not have.
	VAELEN_RUN_API bool ReadRunSection(const CheckpointView& View, Aelvor::RunState& Out);

	/// Reads a checkpoint's header and section table. It does NOT apply
	/// anything to a world - that is 16.06's `Adopt`, which is the only caller
	/// entitled to decide what a section means.
	VAELEN_RUN_API CheckpointRefusal ReadCheckpoint(const uint8* Bytes, usize Size, CheckpointView& Out);

	/// THE IMAGE'S OWN TRAILER: the last eight bytes of the STATE section,
	/// little-endian - what `ComputeStateDigest` returns and what every frozen
	/// digest in this repository is. NOT the STATE row's section digest, which
	/// is a different number over the same bytes (17.03 measured
	/// e0614906cb8a5676 against 0f6fa26b35d09a70 on one ordinary container).
	///
	/// 0 when there is no STATE section or it is too short to hold one - the
	/// same answer a default field gives, and not mistakable for a digest.
	///
	/// In the kernel since 16.14, because that task's engine-side store needs
	/// the same eight bytes read the same way as the host-side one, and the
	/// store's first version got this number wrong by reading it its own way.
	/// One reader; Tests/Run/Test_Containers.cpp keeps an independent one and
	/// requires the two to agree on every file of the corpus.
	VAELEN_RUN_API uint64 ImageTrailer(const CheckpointView& View) noexcept;
} // namespace Vaelen::Run
