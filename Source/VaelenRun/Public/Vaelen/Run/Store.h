// VAELEN - VaelenRun
// Where a checkpoint goes, asked of somebody else.
//
// STATUS: PROTOTYPE (Phase 16 task 16.07) - Tests/Run/Test_Store.cpp exercises
//         the stdio implementation, including the arm that fills the disk.
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
		uint64 Digest = 0;
		uint32 ContainerVersion = 0;
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
		virtual StoreResult Write(const char* Name, const uint8* Bytes, usize Size) = 0;

		/// Reads a checkpoint into Out. On any refusal Out is left as it was
		/// found, per the rule this phase has followed since 16.02.
		virtual StoreResult Read(const char* Name, std::vector<uint8>& Out) = 0;

		/// What is here, newest first where the store can tell.
		virtual std::vector<StoreEntry> List() = 0;

		/// Forgets one. NotFound is not an error to a caller trimming a ring.
		virtual StoreResult Forget(const char* Name) = 0;
	};

	/// A name this interface will accept: not empty, no separator, no parent
	/// directory, and nothing a shell or a filesystem would read as a path.
	/// Checked HERE so that every implementation refuses the same names, rather
	/// than each one inventing its own idea of what is safe.
	VAELEN_RUN_API bool IsUsableCheckpointName(const char* Name) noexcept;
} // namespace Vaelen::Run
