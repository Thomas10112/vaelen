// VAELEN - host-side, deliberately OUTSIDE the kernel.
//
// The stdio implementation of Run::ICheckpointStore, shared by the headless
// tests and Tools/Atlas. It lives here and not in VaelenRun because WHERE a
// save goes is a decision about a machine, and Source/ is where the simulation
// lives. The purity checker would in fact allow <cstdio> - VaelenCore's own
// StdioLogSink uses it - so this placement is the layering rule's doing rather
// than the checker's.
//
// STATUS: PROTOTYPE (Phase 16 task 16.07; 16.15 the rename-aside)
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
	/// A WRITE GOES SOMEWHERE ELSE FIRST AND IS MOVED INTO PLACE, AND SINCE
	/// 16.15 THE OLD SAVE IS SET ASIDE RATHER THAN REPLACED (`Write`). That is the
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
			const std::string Temp = Final + Run::WritingSuffix;
			const std::string Aside = Final + Run::PreviousSuffix;

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
			// 16.15: THE OLD SAVE IS SET ASIDE, NOT REPLACED IN PLACE. A rename
			// over an existing name is one step on POSIX and is refused outright
			// by the C library on Windows, and the engine twin's Move is a delete
			// and then a rename; the one rule that holds on all three is to move
			// the old save to `<name>.previous` first, put the new one where it
			// was, and forget the aside. Should the second rename fail, the old
			// save is whole under the aside and the new one whole under
			// `.writing`, both KEPT, and Read gives the old one back under its
			// own name. Should the FIRST rename fail, nothing has moved: the old
			// save is where it was, and only the temporary goes.
			const bool HadOne = Exists(Final);
			if (HadOne)
			{
				std::remove(Aside.c_str());
				if (!Rename(Final, Aside))
				{
					std::remove(Temp.c_str());
					return Run::StoreResult::CannotWrite;
				}
			}
			if (!Rename(Temp, Final))
			{
				return Run::StoreResult::CannotWrite;
			}
			if (HadOne)
			{
				// Best effort: an aside that lingers is hidden by the name rule
				// and never restored while its name is there, and the next
				// write of the name removes it before setting the new one aside.
				std::remove(Aside.c_str());
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
			// 16.15: a name that is missing while its aside is there is what a
			// failed or interrupted replace left; the aside is renamed back and
			// read under the player's name. If even that rename fails the aside
			// is read where it lies - the bytes are the same bytes.
			const std::string Final = Directory + Name;
			const std::string Aside = Final + Run::PreviousSuffix;
			const std::string Path = Restore(Final, Aside) ? Final : Aside;
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

		/// Since 16.14 the reading itself is `Run::ImageTrailer`, in the kernel,
		/// so that the engine-side store of that task could not read it a THIRD
		/// way; this is kept as the name the tests call. What follows is the
		/// record of why one reader matters.
		///
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
		static uint64 TrailerOf(const Run::CheckpointView& View) noexcept { return Run::ImageTrailer(View); }

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
				if (!Entry.is_regular_file(Code) || Code)
				{
					Code.clear();
					continue;
				}
				// The name rule is the store's, so a stray file somebody
				// dropped in the folder is skipped rather than reported as a
				// save - and `.writing` temporaries from an interrupted write
				// are skipped by the same rule. TRUE SINCE 16.14, which gave
				// the rule `Run::WritingSuffix`; before that this sentence was
				// a claim the rule did not keep, and such a leftover was
				// listed as a save of tick 0.
				if (Run::IsUsableCheckpointName(Name.c_str()))
				{
					Names.push_back(Name);
					continue;
				}
				// 16.15: a save that exists only as `<name>.previous` - what a
				// failed replace left - is listed as `<name>`, and reading it
				// (below) restores the name. One whose name IS there is the
				// aside a write is in the middle of, or one it failed to
				// remove, and is not a second save.
				const std::string Stem = StemOfAside(Name);
				if (!Stem.empty() && Run::IsUsableCheckpointName(Stem.c_str()) && !Exists(Directory + Stem))
				{
					Names.push_back(Stem);
				}
			}
			std::sort(Names.begin(), Names.end());
			Names.erase(std::unique(Names.begin(), Names.end()), Names.end());

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
			// 16.15: the name, its aside and its temporary alike, so a save
			// that was forgotten cannot come back through Read's restore.
			const std::string Path = Directory + Name;
			const bool HadName = std::remove(Path.c_str()) == 0;
			const bool HadAside = std::remove((Path + Run::PreviousSuffix).c_str()) == 0;
			const bool HadTemp = std::remove((Path + Run::WritingSuffix).c_str()) == 0;
			if (!HadName && !HadAside && !HadTemp)
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
	protected:
		/// The one rename, as a seam: Tests/Run/Test_SaveAside.cpp derives a
		/// store whose second rename fails, which is the failure this class
		/// exists to survive and one no test can arrange on a real disk.
		virtual bool Rename(const std::string& From, const std::string& To)
		{
			return std::rename(From.c_str(), To.c_str()) == 0;
		}

		static bool Exists(const std::string& Path)
		{
			std::error_code Code;
			return std::filesystem::is_regular_file(Path, Code) && !Code;
		}

		/// True when `Final` is there to be read - as it was, or renamed back
		/// from `Aside` just now. False when neither is, or the rename back
		/// failed and the caller should read the aside where it lies.
		bool Restore(const std::string& Final, const std::string& Aside)
		{
			if (Exists(Final))
			{
				return true;
			}
			if (!Exists(Aside))
			{
				return false;
			}
			return Rename(Aside, Final);
		}

		/// `<name>` of a `<name>.previous`, or empty when the name is no aside.
		static std::string StemOfAside(const std::string& Name)
		{
			const std::string Suffix(Run::PreviousSuffix);
			if (Name.size() <= Suffix.size() || Name.compare(Name.size() - Suffix.size(), Suffix.size(), Suffix) != 0)
			{
				return std::string();
			}
			return Name.substr(0, Name.size() - Suffix.size());
		}

	private:
		std::string Directory;
	};
} // namespace VaelenHost
