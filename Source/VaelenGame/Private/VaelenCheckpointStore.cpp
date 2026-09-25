// VAELEN - VaelenGame
// Phase 16 task 16.14: the checkpoint store over the engine's file manager.
//
// Three calls here are the engine's, and the parse proves only their shape:
// IFileManager::Move with Replace (a delete and a rename, not one step - see
// Write), FindFiles over a directory, and FFileHelper's byte pair. Everything
// else is the stdio store's logic with the engine's verbs, kept line for line
// where it could be so that a defect found in one can be looked for in the
// other.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-24 (16.15's rename-aside on 2026-09-25), not yet built by
// UnrealBuildTool nor run.
#include "VaelenCheckpointStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Vaelen/Run/Checkpoint.h"

#include <algorithm>
#include <string>
#include <utility>

FVaelenCheckpointStore::FVaelenCheckpointStore(FString InDirectory) : Directory(MoveTemp(InDirectory)) {}

FString FVaelenCheckpointStore::PathOf(const char* Name) const
{
	return FPaths::Combine(Directory, FString(ANSI_TO_TCHAR(Name)));
}

Vaelen::Run::StoreResult FVaelenCheckpointStore::Write(const char* Name, const Vaelen::uint8* Bytes, Vaelen::usize Size)
{
	using Vaelen::Run::StoreResult;
	if (!Vaelen::Run::IsUsableCheckpointName(Name))
	{
		return StoreResult::BadName;
	}
	if (Bytes == nullptr && Size != 0)
	{
		return StoreResult::CannotWrite;
	}
	// Made here and not in the constructor: a store pointed at a folder the
	// player has not created yet lists it as empty, and creates it the first
	// time there is something worth keeping - the stdio store's behaviour.
	IFileManager& Files = IFileManager::Get();
	if (!Files.MakeDirectory(*Directory, true))
	{
		return StoreResult::CannotWrite;
	}
	const FString Final = PathOf(Name);
	FString Temp = Final;
	Temp += ANSI_TO_TCHAR(Vaelen::Run::WritingSuffix);
	FString Aside = Final;
	Aside += ANSI_TO_TCHAR(Vaelen::Run::PreviousSuffix);

	TArray<Vaelen::uint8> Staged;
	Staged.Reserve(static_cast<int32>(Size));
	for (Vaelen::usize Index = 0; Index < Size; ++Index)
	{
		Staged.Add(Bytes[Index]);
	}
	// SaveArrayToFile writes the whole array or answers false: the flush and
	// the close are inside it, so a short write cannot come out of it as a
	// success. Whatever the reason, the FINAL name was never opened, so what
	// was there is still there and still whole.
	if (!FFileHelper::SaveArrayToFile(Staged, *Temp))
	{
		Files.Delete(*Temp, false, true, true);
		return StoreResult::DiskFull;
	}
	// Replace. NOT one step the filesystem makes whole or not at all, which
	// is what this comment said until the 19.10 audit read the engine:
	// FFileManagerGeneric::Move with Replace DELETES the old save and then
	// renames the temporary over its name. Between the two, and after a
	// rename that fails, the last good save of that name is gone and the
	// temporary is all there is. So (16.15, the stdio store line for line)
	// the old save is SET ASIDE first - moved to `<name>.previous`, which the
	// name rule hides from every listing - the temporary moved into the
	// empty place, and the aside forgotten. Should the second move fail, the
	// old save is whole under the aside and the new one whole under
	// `.writing`, both KEPT, and Read gives the old one back under its own
	// name. Should the first fail, nothing has moved: the old save is where
	// it was, and only the temporary goes. Deleting the temporary on a failed
	// replace, as 16.14 did, would have left neither.
	const bool HadOne = Files.FileExists(*Final);
	if (HadOne)
	{
		// Move with Replace: any aside a failed forget left is deleted and
		// the old save takes its name.
		if (!Files.Move(*Aside, *Final, true, false, false, false))
		{
			Files.Delete(*Temp, false, true, true);
			return StoreResult::CannotWrite;
		}
	}
	if (!Files.Move(*Final, *Temp, true, false, false, false))
	{
		return StoreResult::CannotWrite;
	}
	if (HadOne)
	{
		// Best effort: an aside that lingers is hidden, never restored while
		// its name is there, and replaced by the next write of the name.
		Files.Delete(*Aside, false, true, true);
	}
	return StoreResult::Ok;
}

bool FVaelenCheckpointStore::Restore(const FString& Final, const FString& Aside) const
{
	IFileManager& Files = IFileManager::Get();
	if (Files.FileExists(*Final))
	{
		return true;
	}
	if (!Files.FileExists(*Aside))
	{
		return false;
	}
	return Files.Move(*Final, *Aside, true, false, false, false);
}

Vaelen::Run::StoreResult FVaelenCheckpointStore::Read(const char* Name, std::vector<Vaelen::uint8>& Out)
{
	using Vaelen::Run::StoreResult;
	if (!Vaelen::Run::IsUsableCheckpointName(Name))
	{
		return StoreResult::BadName;
	}
	// 16.15: a name that is missing while its aside is there is what a
	// failed or interrupted replace left; the aside is moved back and read
	// under the player's name. If even that move fails the aside is read
	// where it lies - the bytes are the same bytes.
	const FString Final = PathOf(Name);
	FString Aside = Final;
	Aside += ANSI_TO_TCHAR(Vaelen::Run::PreviousSuffix);
	const FString Path = Restore(Final, Aside) ? Final : Aside;
	if (!IFileManager::Get().FileExists(*Path))
	{
		return StoreResult::NotFound;
	}
	TArray<Vaelen::uint8> Got;
	if (!FFileHelper::LoadFileToArray(Got, *Path))
	{
		return StoreResult::ShortRead;
	}
	// Into a scratch vector and swapped in only when whole, so a failed read
	// leaves the caller's vector as it was found.
	std::vector<Vaelen::uint8> Scratch(static_cast<Vaelen::usize>(Got.Num()));
	for (int32 Index = 0; Index < Got.Num(); ++Index)
	{
		Scratch[static_cast<Vaelen::usize>(Index)] = Got[Index];
	}
	Out.swap(Scratch);
	return StoreResult::Ok;
}

std::vector<Vaelen::Run::StoreEntry> FVaelenCheckpointStore::List()
{
	std::vector<Vaelen::Run::StoreEntry> Out;
	// A folder that is not there is an EMPTY listing and not an error, which
	// is what FindFiles answers for one.
	TArray<FString> Found;
	IFileManager& Files = IFileManager::Get();
	Files.FindFiles(Found, *Directory, nullptr);
	std::vector<std::string> Names;
	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		// The LEAF, whatever FindFiles handed back: the engine's overloads
		// differ in whether the directory is prepended, and a store must not.
		const FString Clean = FPaths::GetCleanFilename(Found[Index]);
		const std::string Leaf(TCHAR_TO_UTF8(*Clean));
		// The name rule is the store's (Run::IsUsableCheckpointName): a stray
		// file dropped in the folder is skipped rather than reported as a
		// save, and so is a `.writing` temporary an interrupted write left.
		if (Vaelen::Run::IsUsableCheckpointName(Leaf.c_str()))
		{
			Names.push_back(Leaf);
			continue;
		}
		// 16.15: a save that exists only as `<name>.previous` - what a failed
		// replace left - is listed as `<name>`, and reading it (below)
		// restores the name. One whose name IS there is not a second save.
		const std::string Suffix(Vaelen::Run::PreviousSuffix);
		if (Leaf.size() > Suffix.size() && Leaf.compare(Leaf.size() - Suffix.size(), Suffix.size(), Suffix) == 0)
		{
			const std::string Stem = Leaf.substr(0, Leaf.size() - Suffix.size());
			if (Vaelen::Run::IsUsableCheckpointName(Stem.c_str()) && !Files.FileExists(*PathOf(Stem.c_str())))
			{
				Names.push_back(Stem);
			}
		}
	}
	// Sorted, so two hosts listing the same folder agree on the order.
	std::sort(Names.begin(), Names.end());
	Names.erase(std::unique(Names.begin(), Names.end()), Names.end());
	Out.reserve(Names.size());
	for (const std::string& Name : Names)
	{
		std::vector<Vaelen::uint8> Bytes;
		if (Read(Name.c_str(), Bytes) != Vaelen::Run::StoreResult::Ok)
		{
			continue;
		}
		Vaelen::Run::StoreEntry Entry;
		Entry.Name = Name;
		Entry.Bytes = static_cast<Vaelen::uint64>(Bytes.size());
		Vaelen::Run::CheckpointView View;
		if (Vaelen::Run::ReadCheckpoint(Bytes.data(), Bytes.size(), View).Result == Vaelen::Run::CheckpointResult::Ok)
		{
			Entry.Tick = View.Tick;
			Entry.ContainerVersion = View.Version;
			Entry.SectionCount = static_cast<Vaelen::uint32>(View.Sections.size());
			Entry.Digest = Vaelen::Run::ImageTrailer(View);
		}
		Out.push_back(std::move(Entry));
	}
	return Out;
}

Vaelen::Run::StoreResult FVaelenCheckpointStore::Forget(const char* Name)
{
	using Vaelen::Run::StoreResult;
	if (!Vaelen::Run::IsUsableCheckpointName(Name))
	{
		return StoreResult::BadName;
	}
	// 16.15: the name, its aside and its temporary alike, so a save that was
	// forgotten cannot come back through Read's restore.
	const FString Path = PathOf(Name);
	FString Aside = Path;
	Aside += ANSI_TO_TCHAR(Vaelen::Run::PreviousSuffix);
	FString Temp = Path;
	Temp += ANSI_TO_TCHAR(Vaelen::Run::WritingSuffix);
	IFileManager& Files = IFileManager::Get();
	const bool HadName = Files.FileExists(*Path);
	const bool HadAside = Files.FileExists(*Aside);
	const bool HadTemp = Files.FileExists(*Temp);
	if (!HadName && !HadAside && !HadTemp)
	{
		return StoreResult::NotFound;
	}
	bool Gone = true;
	Gone = (!HadName || Files.Delete(*Path, true, false, true)) && Gone;
	Gone = (!HadAside || Files.Delete(*Aside, true, false, true)) && Gone;
	Gone = (!HadTemp || Files.Delete(*Temp, true, false, true)) && Gone;
	return Gone ? StoreResult::Ok : StoreResult::CannotWrite;
}
