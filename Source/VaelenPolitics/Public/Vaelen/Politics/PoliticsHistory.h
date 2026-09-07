// VAELEN - VaelenPolitics
// Phase 07.07: politics in the chronicle - polities founded and ended, seats
// that fell empty, successions the custom did not name, laws at their bounds,
// provinces that threw off their masters, wars, and ground that changed hands.
//
// STATUS: VALIDATED (Phase 07) - integration/text/deterministic tests in Tests/Politics
//
// The politics events reach the chronicle the way the economy's did (06.07): a
// listener turns what matters into RecordInfo documents, bounded per region and
// year, and DescribePoliticsEvent gives every one of the twenty politics events
// a line in the words of the world. What matters stays narrow on purpose. A
// settled succession is not history; a disputed one is. A tax moving a notch is
// not history; a tax at its floor or its ceiling is. A faction forming is not
// history; a province throwing off its master is.
//
// The describer of the topmost layer speaks for every layer under it, so a
// chronicle of this world still says what its economy, its society and its
// people did - the politics text carries the economy context through.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Factions.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/PoliticsApi.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Politics/Succession.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Politics
{
	struct PoliticsChronicleRules
	{
		uint32 RecordFoundings = 1;	   ///< PolityFounded, PolityDissolved
		uint32 RecordSuccessions = 1;  ///< SeatFellVacant, SuccessionDisputed - never a settled one
		uint32 RecordExtremeLaws = 1;  ///< LawChanged at the floor or the ceiling
		uint32 RecordRevolts = 1;	   ///< FactionRevolted - never a forming or a fading
		uint32 RecordWars = 1;		   ///< ContactMade, StanceChanged to Pact or War
		uint32 RecordAnnexations = 1;  ///< RegionAnnexed
		uint32 MaxRecordsPerYear = 16; ///< per region
	};

	/// Singleton component: the listener's tallies.
	struct PoliticsChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 Region = 0;
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(PoliticsChronicleState) == 24, "PoliticsChronicleState must stay padding free");

	struct PoliticsChronicleTypes
	{
		ComponentType<PoliticsChronicleState> State;
		static VAELEN_POLITICS_API PoliticsChronicleTypes Declare(World& W);
	};

	/// Everything the politics text needs to name things.
	struct PoliticsContext
	{
		Population::PersonTypes Persons;
		PolityTypes Polities;
		LawTypes Laws;
		LawRules LawBounds;
		ReachTypes Reaches;
		SuccessionTypes Lines;
		FactionTypes Factions;
		DiplomacyTypes Relations;
		/// Optional: with it, an event of the economy layer (and, through it, of
		/// the society and person layers) gets its own line rather than the plain
		/// one. The describer of the topmost layer speaks for every layer under it.
		const Economy::EconomyContext* Economy = nullptr;
	};

	/// Listener: the politics events that matter become chronicle records.
	class VAELEN_POLITICS_API PoliticsChronicle final : public IEventListener
	{
	public:
		PoliticsChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, PoliticsContext InContext,
						  PoliticsChronicleTypes InState, PoliticsChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Context(InContext), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "PoliticsChronicle"; }
		void OnEvent(const Event& E) override;
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Region) const;
		World* Owner;
		History::PreHistoryTypes Types;
		PoliticsContext Context;
		PoliticsChronicleTypes State;
		PoliticsChronicleRules Rules;
	};

	/// "the polity of Edavaken", or "polity 3" when its seat is unknown.
	VAELEN_POLITICS_API void NamePolity(const World& W, const History::PreHistoryTypes& Types,
										const PoliticsContext& Context, uint32 Polity, std::string& Out);
	/// "the faction of Ekum", or "faction 2".
	VAELEN_POLITICS_API void NameFaction(const World& W, const History::PreHistoryTypes& Types,
										 const PoliticsContext& Context, uint32 Faction, std::string& Out);
	/// One line for any event: the politics events get their own sentence, every
	/// other event goes through the economy text (and so down to the person).
	VAELEN_POLITICS_API void DescribePoliticsEvent(const World& W, const History::PreHistoryTypes& Types,
												   const PoliticsContext& Context, const Event& E, std::string& Out,
												   const Population::PersonIndex* Index = nullptr);
	/// The whole chronicle as text, in tick order, one line each.
	VAELEN_POLITICS_API uint32 ExportChronicleWithPolitics(const World& W, const History::PreHistoryTypes& Types,
														   const PoliticsContext& Context, std::string& Out,
														   uint32 MaxLines = 0);
	/// The why of an event id as text: the event, then "because ..." lines to the root.
	VAELEN_POLITICS_API uint32 ExportWhyWithPolitics(const World& W, const History::PreHistoryTypes& Types,
													 const PoliticsContext& Context, PersistentId Id, std::string& Out);

	struct PoliticsChronicleStats
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Described = 0; ///< records whose event has a politics line
		uint32 WithRegion = 0;
		uint32 EraConsistent = 0;
		uint32 ByType[6] = {}; ///< founding, succession, law, revolt, war, annexation
	};
	VAELEN_POLITICS_API PoliticsChronicleStats CheckPoliticsChronicle(const World& W,
																	  const History::PreHistoryTypes& Types,
																	  const PoliticsContext& Context,
																	  const PoliticsChronicleTypes& State);
} // namespace Vaelen::Politics
