// VAELEN - VaelenMilitary
// Phase 08.07: war in the chronicle.
//
// STATUS: PROTOTYPE (Phase 08) - integration/text/deterministic tests in Tests/Military
//
// The phase has spent six tasks making war happen and none of it saying so. An
// event log is a record and not a chronicle: it holds every hop of every march
// and every measure of grain, which is exactly what nobody remembers. A
// chronicle is the small part of that a century keeps - a levy called, a host
// broken, a capital stormed, a war begun and a war ended - and it is the thing
// a person in the world could have been told.
//
// So the same shape as 07.07, one layer up: a listener that turns the few
// military events that matter into records, a sentence for every military event
// whether it was recorded or not, and the two exports - the whole chronicle in
// order, and the why of one event walked back to its root. The describer of the
// topmost layer speaks for every layer under it, so a military chronicle can
// tell you about a harvest.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/Battle.h"
#include "Vaelen/Military/March.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Military/Siege.h"
#include "Vaelen/Military/Toll.h"
#include "Vaelen/Military/War.h"
#include "Vaelen/Politics/PoliticsHistory.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"

#include <string>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Military
{
	struct MilitaryChronicleRules
	{
		uint32 RecordLevies = 1;  ///< ArmyRaised, ArmyStarved
		uint32 RecordBattles = 1; ///< BattleFought, ArmyBroken
		uint32 RecordSieges = 1;  ///< SiegeLaid, SeatTaken
		uint32 RecordWars = 1;	  ///< WarBegan, WarEnded
		uint32 RecordTolls = 1;	  ///< WarDead and PeopleFled, past the marks below
		/// A region losing two or three men is not history; a region losing
		/// eight in a year is the year it is remembered for.
		uint32 DeadWorthRecording = 8;
		uint32 FledWorthRecording = 8;
		uint32 MaxRecordsPerYear = 16; ///< per region
	};

	/// Singleton component: the listener's tallies.
	struct MilitaryChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 Region = 0;
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(MilitaryChronicleState) == 24, "MilitaryChronicleState must stay padding free");

	struct MilitaryChronicleTypes
	{
		ComponentType<MilitaryChronicleState> State;
		static VAELEN_MILITARY_API MilitaryChronicleTypes Declare(World& W);
	};

	/// Everything the military text needs to name things.
	struct MilitaryContext
	{
		ArmyTypes Armies;
		MarchTypes Marches;
		BattleTypes Battles;
		SiegeTypes Sieges;
		WarTypes Wars;
		TollTypes Toll;
		/// Optional: with it, an event of the politics layer (and, through it,
		/// of every layer under that) gets its own line rather than the plain
		/// one, and a polity can be named by its seat rather than its number.
		const Politics::PoliticsContext* Politics = nullptr;
	};

	/// Listener: the military events that matter become chronicle records.
	class VAELEN_MILITARY_API MilitaryChronicle final : public IEventListener
	{
	public:
		MilitaryChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, MilitaryContext InContext,
						  MilitaryChronicleTypes InState, MilitaryChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Context(InContext), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "MilitaryChronicle"; }
		void OnEvent(const Event& E) override;
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Region) const;
		World* Owner;
		History::PreHistoryTypes Types;
		MilitaryContext Context;
		MilitaryChronicleTypes State;
		MilitaryChronicleRules Rules;
	};

	/// "the host of the polity of Edavaken", or "host 2" when it is unknown.
	VAELEN_MILITARY_API void NameArmy(const World& W, const History::PreHistoryTypes& Types,
									  const MilitaryContext& Context, uint32 Army, std::string& Out);
	/// "the war between the polity of Edavaken and the polity of Ekum", or "war 3".
	VAELEN_MILITARY_API void NameWar(const World& W, const History::PreHistoryTypes& Types,
									 const MilitaryContext& Context, uint32 War, std::string& Out);
	/// One line for any event: the military events get their own sentence, every
	/// other event goes through the politics text (and so down to the person).
	VAELEN_MILITARY_API void DescribeMilitaryEvent(const World& W, const History::PreHistoryTypes& Types,
												   const MilitaryContext& Context, const Event& E, std::string& Out,
												   const Population::PersonIndex* Index = nullptr);
	/// The whole chronicle as text, in tick order, one line each.
	VAELEN_MILITARY_API uint32 ExportChronicleWithMilitary(const World& W, const History::PreHistoryTypes& Types,
														   const MilitaryContext& Context, std::string& Out,
														   uint32 MaxLines = 0);
	/// The why of an event id as text: the event, then "because ..." lines to the root.
	VAELEN_MILITARY_API uint32 ExportWhyWithMilitary(const World& W, const History::PreHistoryTypes& Types,
													 const MilitaryContext& Context, PersistentId Id, std::string& Out);

	struct MilitaryChronicleStats
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Described = 0; ///< records whose event has a military line
		uint32 WithRegion = 0;
		uint32 EraConsistent = 0;
		uint32 ByType[5] = {}; ///< levy, battle, siege, war, toll
	};
	VAELEN_MILITARY_API MilitaryChronicleStats CheckMilitaryChronicle(const World& W,
																	  const History::PreHistoryTypes& Types,
																	  const MilitaryContext& Context,
																	  const MilitaryChronicleTypes& State);
} // namespace Vaelen::Military
