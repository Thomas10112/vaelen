// VAELEN - VaelenPlayer
// Phase 10.07: the player in the chronicle - a life as records, and the why of
// anything that happened to them walked back through every layer below.
//
// STATUS: PROTOTYPE (Phase 10) - integration/text/deterministic tests in Tests/Player
//
// The same shape as 09.07 one layer up, and for the same reason. The event log
// holds every hour the played person spent and every unit of grain that moved
// because of it, which is exactly what nobody remembers. A chronicle is the
// small part of that a life keeps: what they gave, what they took, where they
// walked, what the world would not let them do.
//
// A day of work is not history and a meal is not history, so neither is
// recorded by default. What is recorded is what touched somebody else, which is
// the same line 10.06 draws for the same reason: those are the acts the world
// has an opinion about.
//
// Every event still has a sentence whether the chronicle kept it or not,
// stamped with the year in the same hand as every layer below, falling through
// to the works text (and so to the military text, and so down to the person)
// for everything else. A player's chronicle can therefore tell you about a
// battle, and through that about a harvest, and through that about a person -
// which is the whole point of having built it in that order.
//
// And the why runs the other way, all the way down. 10.05 passes the act's own
// event as the cause of everything a doing moves, so the grain that left a
// house points at the giving that moved it. Walk further and the chain leaves
// the player entirely: a famine that took their family is the drought of 03.05
// through the stores of 04.04 through the granary of 09.02 that fell in and was
// never rebuilt. The player is a person this world happened to, and the why of
// what happened to them is the why of anything else in it.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Infrastructure/InfrastructureHistory.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Player
{
	struct LifeChronicleRules
	{
		/// Giving, taking and walking: the acts that touch somebody or somewhere.
		uint32 RecordDoings = 1;
		/// Working, eating, resting, waiting and speaking. A day of work is not
		/// history; set this and every hour of the life becomes a record.
		uint32 RecordSmallDoings = 0;
		/// What the world would not let them do. Off by default - a life is what
		/// happened, not what was attempted - and worth having while playing.
		uint32 RecordRefusals = 0;
		uint32 MaxRecordsPerYear = 12; ///< of a life
	};

	/// Singleton component: the listener's tallies.
	struct LifeChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 Person = 0; ///< who the records are of
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(LifeChronicleState) == 24, "LifeChronicleState must stay padding free");

	struct LifeChronicleTypes
	{
		ComponentType<LifeChronicleState> State;
		static VAELEN_PLAYER_API LifeChronicleTypes Declare(World& W);
	};

	/// Everything the text of a life needs to name things.
	struct LifeContext
	{
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		PlayerTypes Player;
		RegardTypes Regard;
		/// Optional: with it, an event of the infrastructure layer (and, through
		/// it, of every layer under that) gets its own line rather than the
		/// person one. The describer of the topmost layer speaks for every layer
		/// under it, which is why this is the one worth wiring.
		const Infrastructure::WorksContext* Works = nullptr;
		/// Optional, and only used when Works is not given: the economy layer,
		/// which is what a life mostly moves - the grain that left a house
		/// because somebody gave it away. Without either, the fall-through stops
		/// at 04.07, which still reaches every layer that has a person in it.
		const Economy::EconomyContext* Goods = nullptr;
	};

	/// Listener: the acts of a life that matter become chronicle records.
	class VAELEN_PLAYER_API LifeChronicle final : public IEventListener
	{
	public:
		LifeChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, LifeContext InContext,
					  LifeChronicleTypes InState, LifeChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Context(InContext), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "LifeChronicle"; }
		void OnEvent(const Event& E) override;
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Person) const;
		World* Owner;
		History::PreHistoryTypes Types;
		LifeContext Context;
		LifeChronicleTypes State;
		LifeChronicleRules Rules;
	};

	/// "Umamissar", the played person, or "nobody" when there is none.
	VAELEN_PLAYER_API void NamePlayed(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context,
									  std::string& Out, const Population::PersonIndex* Index = nullptr);
	/// One line for any event: the acts of a life get their own sentence, every
	/// other event falls through to the layer below and so on down.
	VAELEN_PLAYER_API void DescribeLifeEvent(const World& W, const History::PreHistoryTypes& Types,
											 const LifeContext& Context, const Event& E, std::string& Out,
											 const Population::PersonIndex* Index = nullptr);
	/// The whole chronicle as text, in tick order, one line each.
	VAELEN_PLAYER_API uint32 ExportChronicleWithLife(const World& W, const History::PreHistoryTypes& Types,
													 const LifeContext& Context, std::string& Out, uint32 MaxLines = 0);
	/// The why of an event id as text: the event, then "because ..." lines to
	/// the root, through every layer under the player.
	VAELEN_PLAYER_API uint32 ExportWhyWithLife(const World& W, const History::PreHistoryTypes& Types,
											   const LifeContext& Context, PersistentId Id, std::string& Out);
	/// Every event about the played person, in log order: what they did, what
	/// was refused them, where they walked, and what the world did to them.
	VAELEN_PLAYER_API void LifeTimeline(const World& W, const LifeContext& Context, std::vector<const Event*>& Out);
	/// The life as text: who they are, what they did in order, who knows them
	/// and what those people make of them, and then the why of the last thing
	/// that happened because of them, walked back to its root. Returns the lines.
	VAELEN_PLAYER_API uint32 ExportLife(const World& W, const History::PreHistoryTypes& Types,
										const LifeContext& Context, std::string& Out, uint32 MaxActs = 0);

	struct LifeChronicleStats
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Described = 0;	///< records whose event has a line of this layer
		uint32 OfThePlayer = 0; ///< records whose subject is the played person
		uint32 EraConsistent = 0;
		uint32 ByType[3] = {}; ///< done, refused, walked
	};
	VAELEN_PLAYER_API LifeChronicleStats CheckLifeChronicle(const World& W, const History::PreHistoryTypes& Types,
															const LifeContext& Context,
															const LifeChronicleTypes& State);
} // namespace Vaelen::Player
