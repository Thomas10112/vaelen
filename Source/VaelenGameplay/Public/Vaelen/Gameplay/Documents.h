// VAELEN - VaelenGameplay
// Phase 12 task 12.03: documents.
//
// A document is hearsay that outlives the teller. 12.02 let one person tell
// another what they think of a third, and that story lives exactly as long as
// the people in it: the teller forgets a little every year, and dies. A
// document does neither. It freezes what its writer believed AT THE MOMENT OF
// WRITING, and goes on saying that after the writer has changed their mind,
// after they are dead, and after the person it speaks of has become somebody
// else entirely.
//
// So a document is not a store of truth. It is a store of one person's opinion,
// with a date on it, and the whole of what makes it interesting is that the two
// come apart. That is what makes one worth forging, worth keeping, and worth
// burning.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/edge tests in Tests/Gameplay/Test_Documents.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"

#include <string>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Gameplay
{
	/// Component of a document entity (ids of kind Document).
	struct DocumentInfo
	{
		uint32 Index = 0;  ///< 1-based, in order of writing
		uint32 Writer = 0; ///< person who wrote it; they may be long dead
		uint32 About = 0;  ///< person it speaks of
		int32 Says = 0;	   ///< what the writer thought of them, frozen at the writing
		uint32 Region = 0; ///< where it is
		uint32 Holder = 0; ///< person who has it, 0 = it is in the place and not in a hand
		uint32 Copies = 0; ///< how many times it has been copied out
		uint32 From = 0;   ///< the document this was copied from, 0 when it is an original
		uint64 Written = 0;
		uint64 Lost = 0;	 ///< tick it was lost or burnt, 0 while it exists
		Hash64 Identity = 0; ///< from the world seed
	};
	static_assert(sizeof(DocumentInfo) == 56, "DocumentInfo must stay padding free");

	struct DocumentTypes
	{
		ComponentType<DocumentInfo> Document;

		static VAELEN_GAMEPLAY_API DocumentTypes Declare(World& W);
	};

	struct DocumentRules
	{
		/// What reading one is worth against having been there, per mille. Less
		/// than being told (12.02's HeardPerMille), because a page cannot be
		/// asked what it meant.
		uint32 ReadPerMille = 200;
		uint32 LoseAfterYears = 0; ///< 0 = documents do not rot on their own
	};

	inline constexpr EventType<Player::ActPayload> DocumentWrittenEvent =
		MakeEventType<Player::ActPayload>("DocumentWritten");
	inline constexpr EventType<Player::ActPayload> DocumentReadEvent =
		MakeEventType<Player::ActPayload>("DocumentRead");
	inline constexpr EventType<Player::ActPayload> DocumentCopiedEvent =
		MakeEventType<Player::ActPayload>("DocumentCopied");
	inline constexpr EventType<Player::ActPayload> DocumentLostEvent =
		MakeEventType<Player::ActPayload>("DocumentLost");

	/// Writes down what one person thinks of another, as they think it now.
	/// Returns the document's index, or 0 when the writer has no opinion of the
	/// person to set down - nobody writes about somebody they have never met.
	VAELEN_GAMEPLAY_API uint32 WriteDocument(World& W, const Population::PersonTypes& Persons,
											 const ReputeTypes& Repute, const DocumentTypes& Documents, uint32 Writer,
											 uint32 About, SimTick Now);
	/// Copies one. The copy says exactly what the original said, whatever has
	/// happened since to the writer, to the subject, or to the truth.
	VAELEN_GAMEPLAY_API uint32 CopyDocument(World& W, const DocumentTypes& Documents, uint32 Document, uint32 Holder,
											SimTick Now);
	/// Somebody reads one, and it moves what they think of the person it speaks
	/// of. False for a lost document, an unknown reader, or a reader who is the
	/// subject - nobody learns what they are from a page.
	VAELEN_GAMEPLAY_API bool ReadDocument(World& W, const Population::PersonTypes& Persons, const ReputeTypes& Repute,
										  const DocumentTypes& Documents, const DocumentRules& Rules, uint32 Document,
										  uint32 Reader, SimTick Now);
	/// Burns or mislays one. What it said is gone; what it caused is not.
	VAELEN_GAMEPLAY_API bool LoseDocument(World& W, const DocumentTypes& Documents, uint32 Document, SimTick Now);

	VAELEN_GAMEPLAY_API const DocumentInfo* DocumentOf(const World& W, const DocumentTypes& Documents, uint32 Document);
	/// "the account of Aren, written in the year 312", for the chronicle.
	VAELEN_GAMEPLAY_API void NameDocument(const World& W, const History::PreHistoryTypes& Types,
										  const DocumentTypes& Documents, uint32 Document, std::string& Out);

	struct DocumentStats
	{
		uint32 Written = 0;
		uint32 Standing = 0; ///< not lost
		uint32 Copies = 0;
		uint32 Orphaned = 0; ///< whose writer is dead, and which go on speaking
		uint32 Read = 0;	 ///< readings, from the log
		Hash64 Digest = 0;
	};
	VAELEN_GAMEPLAY_API DocumentStats MeasureDocuments(const World& W, const Population::PersonTypes& Persons,
													   const DocumentTypes& Documents);
} // namespace Vaelen::Gameplay
