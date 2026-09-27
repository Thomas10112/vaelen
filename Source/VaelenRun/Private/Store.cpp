// VAELEN - VaelenRun
// The name rule, and the words for a refusal. Everything else about a store is
// the host's - see Public/Vaelen/Run/Store.h.
//
// STATUS: PROTOTYPE (Phase 16 task 16.07; 16.15 the rename-aside)
#include "Vaelen/Run/Store.h"

#include "Vaelen/Run/Checkpoint.h"

#include <cstring>

namespace Vaelen::Run
{
	const char* StoreResultToString(StoreResult Result) noexcept
	{
		switch (Result)
		{
		case StoreResult::Ok:
			return "Ok";
		case StoreResult::NotFound:
			return "NotFound";
		case StoreResult::BadName:
			return "BadName";
		case StoreResult::CannotWrite:
			return "CannotWrite";
		case StoreResult::DiskFull:
			return "DiskFull";
		case StoreResult::ShortRead:
			return "ShortRead";
		}
		return "Unknown";
	}

	bool IsUsableCheckpointName(const char* Name) noexcept
	{
		if (Name == nullptr || Name[0] == '\0')
		{
			return false;
		}
		// A leading dot hides the file on one family of systems and means the
		// current directory on all of them.
		if (Name[0] == '.')
		{
			return false;
		}
		usize Length = 0;
		for (const char* At = Name; *At != '\0'; ++At, ++Length)
		{
			const char C = *At;
			// Separators BOTH ways round: a name checked only against '/' walks
			// out of the store on Windows, which is the one platform this
			// project ships on.
			if (C == '/' || C == '\\' || C == ':')
			{
				return false;
			}
			// Control characters and the shell's punctuation. A store is handed
			// names from a save-game menu, and a menu is handed names by a
			// person.
			if (C < 0x20 || C == 0x7F || C == '*' || C == '?' || C == '"' || C == '<' || C == '>' || C == '|')
			{
				return false;
			}
			if (Length > 200u)
			{
				return false;
			}
		}
		// A WRITE IN PROGRESS. Every store of this interface writes under
		// `<name>.writing` and moves the file into place when whole, so a name
		// ending that way is either a temporary an interrupted write left
		// behind - which a listing must skip - or a save that the next write
		// of the name it hides behind would replace without a word.
		const usize SuffixLength = std::strlen(WritingSuffix);
		if (Length >= SuffixLength && std::strcmp(Name + (Length - SuffixLength), WritingSuffix) == 0)
		{
			return false;
		}
		// A SAVE SET ASIDE (16.15). Every store renames the old save to
		// `<name>.previous` while the new one is moved into place and gives it
		// back under its own name if that move fails; a listing must not show
		// it twice and a caller must not be able to write over it by name.
		const usize AsideLength = std::strlen(PreviousSuffix);
		if (Length >= AsideLength && std::strcmp(Name + (Length - AsideLength), PreviousSuffix) == 0)
		{
			return false;
		}
		return true;
	}
	StoreResult ICheckpointStore::ReadPart(const char* Name, uint64 Offset, usize Length, std::vector<uint8>& Out)
	{
		std::vector<uint8> Whole;
		const StoreResult Got = Read(Name, Whole);
		if (Got != StoreResult::Ok)
		{
			return Got;
		}
		if (Offset > Whole.size() || Length > Whole.size() - static_cast<usize>(Offset))
		{
			return StoreResult::ShortRead;
		}
		Out.assign(Whole.begin() + static_cast<std::ptrdiff_t>(Offset),
				   Whole.begin() + static_cast<std::ptrdiff_t>(Offset + Length));
		return StoreResult::Ok;
	}

	void DescribeSave(ICheckpointStore& Store, const char* Name, uint64 Bytes, StoreEntry& Out)
	{
		constexpr uint64 TrailerBytes = 8u;
		Out = StoreEntry{};
		Out.Name = Name;
		Out.Bytes = Bytes;
		const usize Prefix = Bytes < CheckpointListingBytes ? static_cast<usize>(Bytes) : CheckpointListingBytes;
		std::vector<uint8> Head;
		if (Store.ReadPart(Name, 0, Prefix, Head) != StoreResult::Ok)
		{
			return;
		}
		CheckpointHeading Heading;
		if (ReadCheckpointHeading(Head.data(), Head.size(), Heading).Result != CheckpointResult::Ok)
		{
			return;
		}
		// A file shorter than its own table claims - the first half of a save,
		// what a write interrupted mid-copy leaves under a plain name - is no
		// container: listed by name and size, every field 0, as a whole read
		// (Truncated) would have it (Run.StoreColdProcess's control).
		if (Heading.PayloadEnd > Bytes || Bytes - Heading.PayloadEnd < TrailerBytes)
		{
			return;
		}
		Out.Tick = Heading.Tick;
		Out.ContainerVersion = Heading.Version;
		Out.SectionCount = Heading.SectionCount;
		// The image's trailer: the STATE section's last eight bytes, where
		// ImageTrailer reads them, little-endian. 0 without a STATE section or
		// one too short to hold them, as ImageTrailer answers.
		if (!Heading.HasState || Heading.StateLength < sizeof(uint64) ||
			Heading.StateOffset + Heading.StateLength > Bytes)
		{
			return;
		}
		std::vector<uint8> Tail;
		if (Store.ReadPart(Name, Heading.StateOffset + Heading.StateLength - sizeof(uint64), sizeof(uint64), Tail) !=
				StoreResult::Ok ||
			Tail.size() != sizeof(uint64))
		{
			return;
		}
		uint64 Value = 0;
		for (usize Index = 0; Index < sizeof(uint64); ++Index)
		{
			Value |= static_cast<uint64>(Tail[Index]) << (8u * Index);
		}
		Out.Digest = Value;
	}

} // namespace Vaelen::Run
