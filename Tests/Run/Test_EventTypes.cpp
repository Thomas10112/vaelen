// VAELEN - VaelenRun tests
// Phase 17 task 17.02: the event-type name table, checked against the world
// that declares the types rather than against itself.
//
// AN EVENT CARRIES A HASH AND NOTHING ELSE about its identity - Event.h stores
// `Hash64 TypeHash`, and the name lives at the declaration site in the
// `EventType<T>` constant. So the moment an event is in a log, an image or a
// container, the word is gone. Grepping this tree for `EventTypeName`,
// `NameOfEvent` or `TypeName(` before 17.02 returned nothing at all, which
// means every census, inspector and causal walk Phase 17 plans would have
// printed sixteen hex digits where a person needs a word.
//
// THIS TEST LIVES IN Tests/Run BECAUSE Tests/Run LINKS EVERY MODULE. The table
// spans nine of them, and a check that could only see VaelenSim's types would
// be a check that cannot see 96 of the 115.
//
// STATUS: PROTOTYPE (Phase 17)
#include "VaelenTest.h"

#include "Vaelen/Sim/EventTypeNames.h"

// The declaration sites, sampled across every module that has any. Not all 115
// - enumerating them here would be the hand-written table the generator exists
// to avoid - but at least one from each module, so a table that was internally
// consistent and read from the wrong place still fails.
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Military/Battle.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/History.h"

#include <cstring>
#include <string>

using namespace Vaelen;

namespace
{
	/// The lookup, with the caller's buffer, as production will call it.
	std::string Name(Hash64 TypeHash)
	{
		char Unknown[18];
		return std::string(NameOfEventType(TypeHash, Unknown));
	}
} // namespace

VAELEN_TEST(EventTypes, EveryRowHashesToItsOwnName)
{
	// THE DECISIVE CHECK, and it costs nothing: the generator computes FNV-1a
	// in Python and the kernel computes it in C++, and if the two disagree by
	// so much as a trailing NUL the table is self-consistent, compiles, round
	// trips against itself and agrees with the running world about nothing.
	// Re-hashing every name with the KERNEL's own HashString is what makes
	// that impossible to miss.
	VT_CHECK_MSG(EventTypeNameCount == 115u, "the table has %zu rows; the tree declared 115 when it was generated",
				 EventTypeNameCount);
	for (usize I = 0; I < EventTypeNameCount; ++I)
	{
		const EventTypeName& Row = EventTypeNames[I];
		VT_CHECK_MSG(HashString(Row.Name) == Row.TypeHash, "%s hashes to %016llx, the table says %016llx", Row.Name,
					 static_cast<unsigned long long>(HashString(Row.Name)),
					 static_cast<unsigned long long>(Row.TypeHash));
	}
}

VAELEN_TEST(EventTypes, TheTableIsSortedAndHasNoDuplicates)
{
	// `NameOfEventType` is a binary search. Over an unsorted table it does not
	// fail loudly - it returns the wrong name for some hashes and the right
	// name for others, which is worse than not working at all. Over a table
	// with a duplicate hash it returns whichever the search lands on.
	for (usize I = 1; I < EventTypeNameCount; ++I)
	{
		VT_CHECK_MSG(EventTypeNames[I - 1].TypeHash < EventTypeNames[I].TypeHash,
					 "rows %zu and %zu are out of order or equal: %s %016llx, %s %016llx", I - 1, I,
					 EventTypeNames[I - 1].Name, static_cast<unsigned long long>(EventTypeNames[I - 1].TypeHash),
					 EventTypeNames[I].Name, static_cast<unsigned long long>(EventTypeNames[I].TypeHash));
	}

	// And the search finds every row it contains - a sorted table with a broken
	// search would pass the clause above and fail every lookup.
	for (usize I = 0; I < EventTypeNameCount; ++I)
	{
		VT_CHECK_MSG(Name(EventTypeNames[I].TypeHash) == EventTypeNames[I].Name, "%s was not found by its own hash",
					 EventTypeNames[I].Name);
	}
}

VAELEN_TEST(EventTypes, TheDeclarationSitesAgree)
{
	// One constant from each module that declares any, read from the REAL
	// header rather than from the table. A table generated from the wrong
	// files, or from a stale checkout, is internally consistent and wrong; this
	// is what notices.
	struct Site
	{
		const char* Module;
		Hash64 TypeHash;
		const char* Name;
	};
	const Site Sites[] = {
		{"Sim", History::EraOpenedEvent.TypeHash, History::EraOpenedEvent.Name},
		{"Population", Population::PersonBornEvent.TypeHash, Population::PersonBornEvent.Name},
		{"Economy", Economy::StockTakenEvent.TypeHash, Economy::StockTakenEvent.Name},
		{"Politics", Politics::PolityFoundedEvent.TypeHash, Politics::PolityFoundedEvent.Name},
		{"Military", Military::BattleFoughtEvent.TypeHash, Military::BattleFoughtEvent.Name},
		{"Infrastructure", Infrastructure::RoadLostEvent.TypeHash, Infrastructure::RoadLostEvent.Name},
		{"Colony", Colony::ColonyFoundedEvent.TypeHash, Colony::ColonyFoundedEvent.Name},
	};
	for (const Site& S : Sites)
	{
		VT_CHECK_MSG(S.TypeHash != 0u, "%s: the sampled constant is not a valid event type", S.Module);
		VT_CHECK_MSG(Name(S.TypeHash) == S.Name, "%s: %016llx is '%s' at the declaration, '%s' in the table", S.Module,
					 static_cast<unsigned long long>(S.TypeHash), S.Name, Name(S.TypeHash).c_str());
	}
}

VAELEN_TEST(EventTypes, AnUnknownHashIsTheHashAndNotAnEmptyString)
{
	// THE CONTROL THE GATE ASKS FOR, and the reason it is worth asking: a log
	// written by a build with a type this one does not have IS going to happen
	// - that is what a save format is for - and the three wrong answers are a
	// crash, an empty string, and a plausible name belonging to something else.
	//
	// The honest answer is the hash, prefixed so a reader can see it is not a
	// name, and greppable so a person can find the declaration if it exists
	// anywhere.
	const Hash64 Missing = HashString("ATypeNoBuildInThisTreeDeclares");
	for (usize I = 0; I < EventTypeNameCount; ++I)
	{
		VT_CHECK(EventTypeNames[I].TypeHash != Missing);
	}

	const std::string Said = Name(Missing);
	VT_CHECK_MSG(!Said.empty(), "an unknown hash printed nothing at all");
	VT_CHECK_MSG(Said.size() == 17u, "'%s' is %zu characters, not '?' and sixteen hex digits", Said.c_str(),
				 Said.size());
	VT_CHECK(Said[0] == '?');
	char Want[18];
	std::snprintf(Want, sizeof(Want), "?%016llx", static_cast<unsigned long long>(Missing));
	VT_CHECK_MSG(Said == Want, "'%s', expected '%s'", Said.c_str(), Want);

	// AND THE SECOND ARM, in the same run: a hash the table DOES know must not
	// come back as a question mark. A lookup that returned '?' for everything
	// would pass every clause above.
	VT_CHECK(Name(History::EraOpenedEvent.TypeHash) == std::string(History::EraOpenedEvent.Name));

	// Zero is not a type. `Event::TypeHash` is 0 on a default-constructed
	// event, and a table that answered that with a name would let an
	// uninitialised event print as a real one.
	const std::string Zero = Name(0);
	VT_CHECK_MSG(Zero == "?0000000000000000", "a zero type hash printed '%s'", Zero.c_str());
}

VAELEN_TEST(EventTypes, ItReadsFromTheCallerSBufferAndNowhereElse)
{
	// No static, no allocation, no shared state: two unknown hashes looked up
	// into two buffers must both still be readable afterwards. A static buffer
	// would make the first answer become the second, which is the classic
	// defect of this shape of function and is invisible in single-use tests.
	char A[18];
	char B[18];
	const Hash64 One = HashString("UnknownOne");
	const Hash64 Two = HashString("UnknownTwo");
	const char* SaidA = NameOfEventType(One, A);
	const char* SaidB = NameOfEventType(Two, B);
	VT_CHECK(SaidA == A);
	VT_CHECK(SaidB == B);
	VT_CHECK_MSG(std::strcmp(SaidA, SaidB) != 0, "two different unknown hashes printed the same text: %s", SaidA);

	// A KNOWN hash must not touch the buffer at all - it returns the table's
	// own literal, so a caller may keep the pointer.
	char Untouched[18];
	std::memset(Untouched, 'x', sizeof(Untouched));
	const char* Known = NameOfEventType(History::EraOpenedEvent.TypeHash, Untouched);
	VT_CHECK(Known != Untouched);
	VT_CHECK(Untouched[0] == 'x');
}
