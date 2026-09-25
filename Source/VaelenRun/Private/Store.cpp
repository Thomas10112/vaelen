// VAELEN - VaelenRun
// The name rule, and the words for a refusal. Everything else about a store is
// the host's - see Public/Vaelen/Run/Store.h.
//
// STATUS: PROTOTYPE (Phase 16 task 16.07)
#include "Vaelen/Run/Store.h"

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
		return true;
	}
} // namespace Vaelen::Run
