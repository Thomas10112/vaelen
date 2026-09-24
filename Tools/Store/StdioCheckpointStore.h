// VAELEN - host-side, deliberately OUTSIDE the kernel.
//
// The stdio implementation of Run::ICheckpointStore, shared by the headless
// tests and Tools/Atlas. It lives here and not in VaelenRun because WHERE a
// save goes is a decision about a machine, and Source/ is where the simulation
// lives. The purity checker would in fact allow <cstdio> - VaelenCore's own
// StdioLogSink uses it - so this placement is the layering rule's doing rather
// than the checker's.
//
// STATUS: PROTOTYPE (Phase 16 task 16.07)
#pragma once

#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Store.h"

#include <cstdio>
#include <cstring>
// 17.03: LISTING READS THE DIRECTORY, so the directory has to be readable.
//
// <filesystem> is used with the `error_code` overloads throughout, never the
// throwing ones, because this tree builds with -fno-exceptions; an operation
// that fails hands back a code and the listing goes on without that entry.
//
// The header this replaced said "NO <filesystem> AND NO <dirent.h>", and that
// sentence is the defect's own explanation: the rule it was obeying belongs to
// the KERNEL, where <filesystem> is banned because the simulation must not know
// what a path is. This file is not the kernel - its first line says so - and a
// host-side store that cannot enumerate the directory it was handed is a store
// that cannot do the one thing a save browser needs.
//
// Chosen over #ifdef'd <dirent.h> and FindFirstFileW deliberately: two platform
// paths means the Windows one is compiled by one CI leg and exercised by none,
// and this is the file whose whole point is being trusted about what is on a
// disk.
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace VaelenHost
{
	using namespace Vaelen;

	/// Checkpoints as files in one directory.
	///
	/// A WRITE GOES SOMEWHERE ELSE FIRST AND IS MOVED INTO PLACE. That is the
	/// whole reason this class is worth testing: fopen("wb") on the final name
	/// truncates it before a single byte of the new save exists, so a disk that
	/// fills halfway through has taken the player's previous game as well as
	/// refused them a new one. Writing to a temporary in the SAME directory and
	/// renaming makes the replacement atomic on every filesystem this project
	/// targets - and same-directory matters, because a rename across devices is
	/// a copy, which can fail halfway just like the write did.
	/// NOT `final` since 17.03: Tests/Run/Test_StoreColdProcess.cpp derives the
	/// PRE-FIX listing from it, so that the defect this task removed stays
	/// visible in a test after the code is gone. A subclass that overrides
	/// `List` and `Write` and inherits everything else differs from the fixed
	/// store in exactly the defect and in nothing else.
	class StdioCheckpointStore : public Run::ICheckpointStore
	{
	public:
		explicit StdioCheckpointStore(std::string InDirectory) : Directory(std::move(InDirectory))
		{
			if (!Directory.empty() && Directory.back() != '/')
			{
				Directory.push_back('/');
			}
		}

		Run::StoreResult Write(const char* Name, const uint8* Bytes, usize Size) override
		{
			if (!Run::IsUsableCheckpointName(Name))
			{
				return Run::StoreResult::BadName;
			}
			if (Bytes == nullptr && Size != 0)
			{
				return Run::StoreResult::CannotWrite;
			}
			const std::string Final = Directory + Name;
			const std::string Temp = Final + ".writing";

			std::FILE* F = std::fopen(Temp.c_str(), "wb");
			if (F == nullptr)
			{
				return Run::StoreResult::CannotWrite;
			}
			const usize Wrote = Size == 0 ? 0u : std::fwrite(Bytes, 1, Size, F);
			// THE FLUSH IS CHECKED, not just called. fwrite can succeed into a
			// buffer that fclose then fails to drain, which is exactly what a
			// full disk looks like from here.
			const bool Flushed = std::fflush(F) == 0;
			const bool Closed = std::fclose(F) == 0;
			if (Wrote != Size || !Flushed || !Closed)
			{
				// The partial temporary is removed and the FINAL NAME WAS NEVER
				// OPENED, so whatever was there is still there and still whole.
				std::remove(Temp.c_str());
				return Run::StoreResult::DiskFull;
			}
			if (std::rename(Temp.c_str(), Final.c_str()) != 0)
			{
				std::remove(Temp.c_str());
				return Run::StoreResult::CannotWrite;
			}
			// The rename IS the record. Nothing else needs telling: `List`
			// reads the directory, so what is on the disk is what is listed.
			return Run::StoreResult::Ok;
		}

		Run::StoreResult Read(const char* Name, std::vector<uint8>& Out) override
		{
			if (!Run::IsUsableCheckpointName(Name))
			{
				return Run::StoreResult::BadName;
			}
			const std::string Path = Directory + Name;
			std::FILE* F = std::fopen(Path.c_str(), "rb");
			if (F == nullptr)
			{
				return Run::StoreResult::NotFound;
			}
			std::fseek(F, 0, SEEK_END);
			const long End = std::ftell(F);
			std::fseek(F, 0, SEEK_SET);
			if (End < 0)
			{
				std::fclose(F);
				return Run::StoreResult::ShortRead;
			}
			// Read into a scratch buffer and hand it over only when whole, so a
			// short read leaves the caller's vector as it was found.
			std::vector<uint8> Scratch(static_cast<usize>(End));
			const usize Got = Scratch.empty() ? 0u : std::fread(Scratch.data(), 1, Scratch.size(), F);
			std::fclose(F);
			if (Got != Scratch.size())
			{
				return Run::StoreResult::ShortRead;
			}
			Out.swap(Scratch);
			return Run::StoreResult::Ok;
		}

		/// 17.03: THE IMAGE'S OWN TRAILER, and the reason this is a function.
		///
		/// What stood here was `View.Sections.front().Digest`, reported after
		/// the result of `View.Find(SectionKind::State, Length)` had been
		/// called and THROWN AWAY. Two faults in one line, and the ORDER of
		/// their seriousness is the opposite of what I first wrote down.
		///
		/// The one I called secondary is the real one: the SECTION digest is
		/// not the image TRAILER. They are different numbers over the same
		/// bytes, on every container, whatever the section order - measured on
		/// an ordinary one, e0614906cb8a5676 against `ComputeStateDigest`'s
		/// 0f6fa26b35d09a70. So a host comparing a listed digest against a
		/// logged one was told two identical saves were different worlds, and
		/// it was told that ALWAYS.
		///
		/// The one I called primary, reading by POSITION rather than by the
		/// `Find` whose answer was discarded, is ADR-0150's "safe by accident"
		/// and would have started mattering the day a writer put a cheap
		/// section first. It had not started mattering yet.
		///
		/// 0 when there is no STATE section or it is too short to hold one,
		/// which is the same answer the field's default gives and is not
		/// mistakable for a digest.
		static uint64 TrailerOf(const Run::CheckpointView& View) noexcept
		{
			uint64 Length = 0;
			const uint8* State = View.Find(Run::SectionKind::State, Length);
			if (State == nullptr || Length < sizeof(uint64))
			{
				return 0;
			}
			uint64 Value = 0;
			for (usize Index = 0; Index < sizeof(uint64); ++Index)
			{
				Value |= static_cast<uint64>(State[Length - sizeof(uint64) + Index]) << (8u * Index);
			}
			return Value;
		}

		std::vector<Run::StoreEntry> List() override
		{
			// 17.03: THE DIRECTORY, NOT THE `Written` VECTOR.
			//
			// This method used to iterate a private vector that only `Write`
			// and `Remember` fill, so a process which had written nothing
			// listed nothing - and a host started fresh and pointed at a
			// directory full of saves was told it was empty. Run.Store passed
			// because it wrote and listed in ONE process: an instrument blind
			// to the only dimension that matters.
			//
			// Names are sorted, so two hosts listing the same directory agree
			// on the order. `directory_iterator` gives no order at all.
			std::vector<Run::StoreEntry> Out;

			std::error_code Code;
			const std::filesystem::path Where(Directory.empty() ? std::string(".") : Directory);
			std::filesystem::directory_iterator It(Where, Code);
			if (Code)
			{
				// A directory that cannot be read is an EMPTY listing and not a
				// crash: a host may point this at a save folder the player has
				// not created yet.
				return Out;
			}

			std::vector<std::string> Names;
			for (const std::filesystem::directory_entry& Entry : It)
			{
				const std::string Name = Entry.path().filename().string();
				// The name rule is the store's, so a stray file somebody
				// dropped in the folder is skipped rather than reported as a
				// save - and `.writing` temporaries from an interrupted write
				// are skipped by the same rule.
				if (!Run::IsUsableCheckpointName(Name.c_str()))
				{
					continue;
				}
				if (!Entry.is_regular_file(Code) || Code)
				{
					Code.clear();
					continue;
				}
				Names.push_back(Name);
			}
			std::sort(Names.begin(), Names.end());

			Out.reserve(Names.size());
			for (const std::string& Name : Names)
			{
				std::vector<uint8> Bytes;
				if (Read(Name.c_str(), Bytes) != Run::StoreResult::Ok)
				{
					continue;
				}
				Run::StoreEntry Entry;
				Entry.Name = Name;
				Entry.Bytes = static_cast<uint64>(Bytes.size());
				Run::CheckpointView View;
				if (Run::ReadCheckpoint(Bytes.data(), Bytes.size(), View).Result == Run::CheckpointResult::Ok)
				{
					Entry.Tick = View.Tick;
					Entry.ContainerVersion = View.Version;
					Entry.SectionCount = static_cast<uint32>(View.Sections.size());
					Entry.Digest = TrailerOf(View);
				}
				Out.push_back(std::move(Entry));
			}
			return Out;
		}

		Run::StoreResult Forget(const char* Name) override
		{
			if (!Run::IsUsableCheckpointName(Name))
			{
				return Run::StoreResult::BadName;
			}
			const std::string Path = Directory + Name;
			if (std::remove(Path.c_str()) != 0)
			{
				return Run::StoreResult::NotFound;
			}
			return Run::StoreResult::Ok;
		}

		// 17.03 DELETED `Written` AND `Remember`. The vector was the store's own
		// idea of what the directory held, and `Remember` existed so a caller
		// could correct it - a seam whose only purpose was to patch up a list
		// that was wrong by construction. `List` reads the directory now, so a
		// cache of names can only ever disagree with it, and the one thing
		// worse than a store that lists nothing is a store that lists something
		// that is not there.
	private:
		std::string Directory;
	};
} // namespace VaelenHost
