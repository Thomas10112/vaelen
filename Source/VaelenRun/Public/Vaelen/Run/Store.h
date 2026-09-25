// VAELEN - VaelenRun
// Where a checkpoint goes, asked of somebody else.
//
// STATUS: PROTOTYPE (Phase 16 task 16.07; 16.15 the rename-aside) -
//         Tests/Run/Test_Store.cpp exercises the stdio implementation, including
//         the arm that fills the disk; Tests/Run/Test_SaveAside.cpp the replace
//         that fails halfway.
//
// Checkpoint.h deals in BYTES and says nothing about where they live. This is
// the other half of that sentence: an interface the kernel can call and cannot
// implement. `ILogSink` and `AssertHandler` are the precedent - a pure
// interface here, a host implementation outside.
//
// NO OS HEADER, and not even <cstdio>, though the purity checker would allow it
// (VaelenCore's own StdioLogSink uses it). The reason is the layering rule
// rather than the checker: WHERE a save goes is a decision about directories,
// permissions and what a player's machine looks like, and the simulation is not
// entitled to an opinion about any of it. Unreal's SaveGame directory on
// Windows and a temporary folder in a test are the same question answered
// twice, by the two people who are allowed to answer it.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Run/RunApi.h"

#include <string>
#include <vector>

namespace Vaelen::Run
{
	enum class StoreResult : uint8
	{
		Ok,
		/// No checkpoint by that name.
		NotFound,
		/// The name is one this store will not accept - empty, or reaching out
		/// of the store with a separator or a parent directory.
		BadName,
		/// The place cannot be written to at all: permissions, a missing
		/// directory, a read-only volume.
		CannotWrite,
		/// There was not room. THE PREVIOUS CHECKPOINT OF THAT NAME IS STILL
		/// THERE AND STILL WHOLE - that is the promise this whole interface
		/// exists to make, and Save.AtomicAndComplete is what holds it to it.
		DiskFull,
		/// The bytes came back shorter than they went in, or not at all.
		ShortRead,
	};

	VAELEN_RUN_API const char* StoreResultToString(StoreResult Result) noexcept;

	/// What a store knows about one checkpoint without reading it.
	struct StoreEntry
	{
		std::string Name;
		uint64 Bytes = 0;
		/// From the container's own header, so a chooser can show a save's tick
		/// and version without loading two gigabytes to find out.
		uint64 Tick = 0;
		/// THE IMAGE'S TRAILER: the last eight bytes of the STATE section, which
		/// is what `ComputeStateDigest` returns and what every frozen digest in
		/// this repository is.
		///
		/// It is NOT the STATE row's section digest from the table beside it -
		/// a different number over the same bytes. 16.07 reported that one, by
		/// POSITION, after discarding the result of the `Find` it had just
		/// called.
		///
		/// AND IT WAS WRONG ALWAYS, not only for an unusual layout. I first
		/// wrote this down as "right today because STATE happens to be section
		/// 0"; measuring it says otherwise. On an ORDINARY container the old
		/// code reported e0614906cb8a5676 where `ComputeStateDigest` returns
		/// 0f6fa26b35d09a70. Position was the second fault; the first was that
		/// the section digest is simply not the number every other instrument
		/// in this repository means by a save's digest. A host comparing a
		/// listed digest against a logged one was told two identical saves were
		/// different worlds. 17.03.
		uint64 Digest = 0;
		uint32 ContainerVersion = 0;
		/// How many sections the container has, so a caller can see its SHAPE -
		/// whether a save carries its own input tape, above all - without
		/// opening the file a second time.
		uint32 SectionCount = 0;
	};

	/// Somewhere checkpoints are kept. Implementations live OUTSIDE the kernel.
	///
	/// EVERY FAILURE IS A RETURNED RESULT. There is no path here that reports
	/// by assertion, because 16.02 established what an assertion is worth in a
	/// shipping build: `Assert.h` compiles it to `((void)0)` under `NDEBUG` and
	/// under `UE_BUILD_SHIPPING`, which is every build a player runs.
	class VAELEN_RUN_API ICheckpointStore
	{
	public:
		virtual ~ICheckpointStore() = default;

		/// Writes bytes under a name, replacing what was there.
		///
		/// AN IMPLEMENTATION MUST NOT DESTROY THE PREVIOUS CHECKPOINT ON ITS
		/// WAY TO FAILING. Write somewhere else and move it into place; a
		/// truncating open over the only good save is how a full disk takes a
		/// player's game rather than merely refusing them a new one.
		///
		/// AND THE PREVIOUS CHECKPOINT KEEPS ITS NAME (16.15). The replace is
		/// not one step on every filesystem: the engine's Move is a delete of
		/// the old file and then a rename, and a rename that fails after that
		/// delete leaves the last good save nameless. So every store of this
		/// interface renames the old save ASIDE first, to `<name>.previous`
		/// (`PreviousSuffix`, refused by the name rule like `.writing`), then
		/// moves the new one into place, then forgets the aside. A replace
		/// that fails between those steps leaves the old save whole under the
		/// aside name and the new one whole under `.writing`, and `Read` and
		/// `List` give the old one back under its own name (below). Nothing a
		/// player named is ever the file that is missing.
		virtual StoreResult Write(const char* Name, const uint8* Bytes, usize Size) = 0;

		/// Reads a checkpoint into Out. On any refusal Out is left as it was
		/// found, per the rule this phase has followed since 16.02.
		///
		/// A name that is missing while `<name>.previous` is there is RESTORED
		/// first - the aside is renamed back - and then read: what a failed or
		/// interrupted replace left behind comes back under the name the
		/// player gave it, without a word.
		virtual StoreResult Read(const char* Name, std::vector<uint8>& Out) = 0;

		/// What is here, newest first where the store can tell. A save that
		/// exists only as `<name>.previous` is listed as `<name>`.
		virtual std::vector<StoreEntry> List() = 0;

		/// Forgets one: the name, its `.previous` and its `.writing` alike, so
		/// that a forgotten save cannot come back through `Read`'s restore.
		/// NotFound is not an error to a caller trimming a ring.
		virtual StoreResult Forget(const char* Name) = 0;
	};

	/// The suffix EVERY store of this interface writes under while a checkpoint
	/// is being written: `<name>.writing`, in the same directory, moved into
	/// place only when whole (Tools/Store/StdioCheckpointStore.h and the
	/// engine's Source/VaelenGame/Private/VaelenCheckpointStore.cpp). Named
	/// HERE so that `IsUsableCheckpointName` can refuse it, which is what keeps
	/// a temporary an interrupted write left behind out of every listing. The
	/// stdio store's comment said the name rule already did that; it did not,
	/// and 16.14 found so while writing the second store (a listing reported
	/// such a leftover as a save of tick 0).
	inline constexpr const char* WritingSuffix = ".writing";

	/// The suffix EVERY store renames the old save to while the new one is
	/// moved into its place (16.15, `ICheckpointStore::Write`): `<name>.previous`,
	/// in the same directory, forgotten once the new save is in place, and
	/// renamed back by `Read` when the name it stands for is missing. Named
	/// HERE for the same reason as `WritingSuffix`: the rule refuses it, so no
	/// listing shows it and no caller can write under it.
	inline constexpr const char* PreviousSuffix = ".previous";

	/// A name this interface will accept: not empty, no separator, no parent
	/// directory, nothing a shell or a filesystem would read as a path, and
	/// not the name of a write in progress (`WritingSuffix`) nor of a save set
	/// aside (`PreviousSuffix`). Checked HERE so that every implementation
	/// refuses the same names, rather than each one inventing its own idea of
	/// what is safe.
	VAELEN_RUN_API bool IsUsableCheckpointName(const char* Name) noexcept;
} // namespace Vaelen::Run
