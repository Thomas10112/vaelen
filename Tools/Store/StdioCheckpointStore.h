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
	class StdioCheckpointStore final : public Run::ICheckpointStore
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
			Remember(Name);
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

		std::vector<Run::StoreEntry> List() override
		{
			// NO <filesystem> AND NO <dirent.h>: this store knows what it has
			// written and is asked to list it, which is all the tests and Atlas
			// need. A host that must enumerate a directory it did not fill -
			// Unreal's, in 16.14 - has its own platform call for that and its
			// own implementation of this interface.
			std::vector<Run::StoreEntry> Out;
			Out.reserve(Written.size());
			for (const std::string& Name : Written)
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
					uint64 Length = 0;
					if (View.Find(Run::SectionKind::State, Length) != nullptr && !View.Sections.empty())
					{
						Entry.Digest = View.Sections.front().Digest;
					}
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
			for (usize Index = 0; Index < Written.size(); ++Index)
			{
				if (Written[Index] == Name)
				{
					Written.erase(Written.begin() + static_cast<long>(Index));
					break;
				}
			}
			return Run::StoreResult::Ok;
		}

		/// Tells the store a name exists without writing it - for a host that
		/// enumerated a directory by some other means.
		void Remember(const char* Name)
		{
			for (const std::string& Had : Written)
			{
				if (Had == Name)
				{
					return;
				}
			}
			Written.emplace_back(Name);
		}

	private:
		std::string Directory;
		std::vector<std::string> Written;
	};
} // namespace VaelenHost
