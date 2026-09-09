// VAELEN - VaelenPlayer
// Phase 10.04: intent as commands - a queue the player submits to, and a system
// inside the simulation that is the only thing allowed to act on it.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge/replay tests in Tests/Player
//
// The rule of the phase is that the player is a person the world already had,
// and the rule of this task is what makes that testable: NOTHING is ever
// written into the world from outside the simulation. What arrives from outside
// - a keypress, a script, a recorded stream - is an INTENT: a small struct that
// says what the person wants to do, put in a queue and nothing more. It changes
// no region, no store, no standing, no person. A system inside the simulation
// reads that queue on the player's day, decides what the world allows, spends
// the hours 10.03 granted, and publishes what happened.
//
// The reason for the ceremony is replay. A stream of intents with the ticks
// they were submitted on, applied to the same seed, gives the same life -
// because the intents are inputs to the simulation exactly like the seed is,
// and the simulation is the only writer. A player who could write to the world
// directly could not be replayed, and a world that cannot be replayed is not
// this project.
//
// This task builds the queue, the validation and the application, with one cost
// in hours per kind of intent. 10.05 gives each kind its effect, and every one
// of them goes through the system that already owns that part of the world -
// work through 06.01, eat through 04.04, move through the region graph - never
// through this one.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Player
{
	/// What a person can want to do. The seven doings of 10.05 are named here so
	/// that a recorded stream made in this task keeps its meaning in the next;
	/// this task gives them a cost in hours and no effect beyond that.
	enum class Intent : uint8
	{
		None = 0,
		Wait, ///< let the hours go by; the one intent that is complete here
		Work,
		Rest,
		Eat,
		Move,
		Speak,
		Give,
		Take,
		Count
	};
	inline constexpr usize IntentCount = static_cast<usize>(Intent::Count);
	VAELEN_PLAYER_API const char* IntentName(Intent Kind);

	/// Why the world refused an intent. Kept on the command and counted on the
	/// queue, because "nothing happened" is not an answer a player can act on.
	enum class Refusal : uint8
	{
		None = 0,
		NoPlayer, ///< nobody is played, or the played person has no queue
		Dead,	  ///< the person the intent was for is no longer alive
		Unknown,  ///< an intent this build has no name for
		Costly,	  ///< it asks for more hours than a whole day has
		Full,	  ///< the queue is already holding all it can
		Stale,	  ///< it waited so long unapplied that it is no longer meant
		Count
	};
	VAELEN_PLAYER_API const char* RefusalName(Refusal Why);

	/// One intent. Small, flat and copyable, because a recorded stream of these
	/// IS the player's half of a replay: seed plus stream gives the same life.
	struct PlayerCommand
	{
		uint8 Kind = 0;		 ///< Intent
		uint8 Why = 0;		 ///< Refusal, filled in when the world refused it
		uint16 Reserved = 0; //
		uint32 Target = 0;	 ///< what it is aimed at; the kind says what that means
		uint32 Amount = 0;	 ///< how much of it
		uint32 Hours = 0;	 ///< hours asked; 0 takes the rule's cost for the kind
		uint64 Issued = 0;	 ///< the tick it was submitted on
	};
	static_assert(sizeof(PlayerCommand) == 24, "PlayerCommand must stay padding free");

	/// How many intents can wait at once. A queue and not a list: a person acts
	/// on what they meant recently, and an unbounded backlog is not a life.
	inline constexpr usize MostOrders = 8;

	/// Component on the played person: what they mean to do, and what came of it.
	struct PlayerOrders
	{
		uint32 First = 0;	///< index in Ring of the next one to apply
		uint32 Held = 0;	///< how many are waiting
		uint32 Taken = 0;	///< applied over the life
		uint32 Refused = 0; ///< refused by the world over the life
		uint32 Dropped = 0; ///< never queued at all: the queue was full
		uint32 Last = 0;	///< Refusal of the last refusal, for the why of 10.07
		uint64 Since = 0;	///< tick the queue was opened
		PlayerCommand Ring[MostOrders];
	};
	static_assert(sizeof(PlayerOrders) == 224, "PlayerOrders must stay padding free");

	struct OrderTypes
	{
		ComponentType<PlayerOrders> Orders;
		static VAELEN_PLAYER_API OrderTypes Declare(World& W);
	};

	struct OrderRules
	{
		/// Hours one intent of each kind costs, by Intent.
		uint32 HoursOf[16] = {0, 1, 4, 2, 1, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
		uint32 MostHeld = static_cast<uint32>(MostOrders);
		/// Ticks an intent may wait unapplied before it is no longer meant. A
		/// month of hours; 0 lets one wait forever.
		uint32 StaleAfter = 720;
	};

	/// What the played person did (Amount = the hours it cost).
	struct ActPayload
	{
		uint32 Person = 0;
		uint32 Kind = 0;   ///< Intent
		uint32 Target = 0; ///< as the intent gave it
		uint32 Amount = 0;
	};
	inline constexpr EventType<ActPayload> PlayerActedEvent = MakeEventType<ActPayload>("PlayerActed");
	/// What the world would not let them do (Amount = the Refusal).
	inline constexpr EventType<ActPayload> PlayerRefusedEvent = MakeEventType<ActPayload>("PlayerRefused");

	/// Daily, after PlayerDay: the ONLY thing in the project that acts on an
	/// intent. It takes them in the order they were meant, refuses what the
	/// world does not allow, spends the hours the day granted, and stops when
	/// the day is gone - what is left waits for tomorrow, which is why a person
	/// who means eight things at dawn does not do all eight at dawn.
	class VAELEN_PLAYER_API PlayerOrderSystem final : public ISystem
	{
	public:
		PlayerOrderSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
						  PlayerTypes InPlayer, HourTypes InHours, OrderTypes InOrders, OrderRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Player(InPlayer), Hours(InHours), Orders(InOrders),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "PlayerOrders"; }
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override
		{
			// After the day has turned, so that an intent submitted yesterday
			// evening is paid for out of this morning's hours.
			std::vector<std::string_view> Out{"PlayerDay"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		PlayerTypes Player;
		HourTypes Hours;
		OrderTypes Orders;
		OrderRules Rules;
	};

	/// Opens the queue on the played person. False when nobody is played or the
	/// queue is already open.
	VAELEN_PLAYER_API bool BeginOrders(World& W, const PlayerTypes& Player, const OrderTypes& Orders, SimTick Now);
	/// Closes it, dropping whatever was waiting.
	VAELEN_PLAYER_API bool EndOrders(World& W, const OrderTypes& Orders);
	/// Submits an intent. This is the whole of what the outside world may do: it
	/// queues a struct and changes NOTHING else - no store, no standing, no
	/// person, not even an hour. Refusal::None means queued, not done.
	VAELEN_PLAYER_API Refusal Submit(World& W, const PlayerTypes& Player, const OrderTypes& Orders,
									 const OrderRules& Rules, const PlayerCommand& Command);
	/// A shorthand for the common case: a kind, a target, an amount, this tick.
	VAELEN_PLAYER_API Refusal Order(World& W, const PlayerTypes& Player, const OrderTypes& Orders,
									const OrderRules& Rules, Intent Kind, uint32 Target, uint32 Amount, SimTick Now);
	/// The queue itself (nullptr when it was never opened).
	VAELEN_PLAYER_API const PlayerOrders* OrdersOf(const World& W, const OrderTypes& Orders);
	/// How many intents are waiting, 0 when there is no queue.
	VAELEN_PLAYER_API uint32 OrdersHeld(const World& W, const OrderTypes& Orders);

	struct OrderStats
	{
		uint32 Queues = 0;	///< queues in the world; more than one is incoherent
		uint32 Held = 0;	///< waiting now
		uint32 Taken = 0;	///< applied over the life
		uint32 Refused = 0; ///< refused over the life
		uint32 Dropped = 0; ///< never queued: the queue was full
		uint32 Bad = 0;		///< incoherent: see MeasureOrders
		Hash64 Digest = 0;
	};

	/// Counts the intents and checks what the queue must keep: one to a world,
	/// on the person being played, never holding more than it can, and never a
	/// waiting intent of a kind this build has no name for.
	VAELEN_PLAYER_API OrderStats MeasureOrders(const World& W, const Population::PersonTypes& Persons,
											   const PlayerTypes& Player, const OrderTypes& Orders,
											   const OrderRules& Rules);
} // namespace Vaelen::Player
