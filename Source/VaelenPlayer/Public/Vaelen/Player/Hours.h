// VAELEN - VaelenPlayer
// Phase 10.03: the player's grain - a day at a time, in a world that runs at
// the year.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player
//
// Every system in this project so far runs at SimLod::World: once every 8640
// ticks, which the calendar of 01.04 calls a year. That is right for a harvest,
// a levy and a polity, and it is useless to somebody living a life. A person
// eats today, works today and sleeps tonight.
//
// The scheduler has had the finer grains since 01.03 and nothing has used them:
// Period[] is {1, 4, 24, 720, 8640}, so the tick IS the hour, and
// SimLod::Aggregate is the day. 10.03 is the first system in the project to run
// at one of them, and it runs for exactly one person - the one being played.
//
// What it does is grant time. A day gives the person a number of waking hours,
// and 10.04's commands will spend them; nothing else in this task spends
// anything. What it must not do is change the world, and the test that keeps it
// honest is the event log: the player's day publishes nothing, so a world with
// somebody living an hour at a time writes exactly the history a world without
// one writes.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Population/Persons.h"
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
	/// Component on the played person: the day they are in and what is left of it.
	struct PlayerHours
	{
		uint32 Day = 0;	   ///< day of the world, from the first tick
		uint32 Awake = 0;  ///< waking hours this day gives
		uint32 Spent = 0;  ///< of those, already spent
		uint32 Slept = 0;  ///< hours slept last night
		uint32 Days = 0;   ///< days lived under the fine grain
		uint32 Missed = 0; ///< days the grain could not run: the person was gone
		uint64 Since = 0;  ///< tick the fine grain began for them
	};
	static_assert(sizeof(PlayerHours) == 32, "PlayerHours must stay padding free");

	struct HourTypes
	{
		ComponentType<PlayerHours> Hours;
		static VAELEN_PLAYER_API HourTypes Declare(World& W);
	};

	struct HourRules
	{
		uint32 SleepHours = 8; ///< of a day; the rest is the person's to spend
		uint32 HoursPerDay = 24;
		uint32 TicksPerHour = 1; ///< the calendar of 01.04; the tick is the hour
	};

	/// Daily, for the played person only: a new day, its waking hours, and
	/// nothing else. Publishes nothing at all - a world with somebody living an
	/// hour at a time writes exactly the history a world without one writes.
	class VAELEN_PLAYER_API PlayerDaySystem final : public ISystem
	{
	public:
		PlayerDaySystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
						PlayerTypes InPlayer, HourTypes InHours, HourRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Player(InPlayer), Hours(InHours), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "PlayerDay"; }
		/// The day. Every other system in the project is at World, once a year;
		/// this is the first thing in nine phases to want a finer one.
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out;
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
		HourRules Rules;
	};

	/// The day the played person is in (nullptr when nobody is played, or before
	/// their first day has turned).
	VAELEN_PLAYER_API const PlayerHours* HoursOf(const World& W, const HourTypes& Hours);
	/// Hours of this day the player has not spent yet, 0 when nobody is played.
	VAELEN_PLAYER_API uint32 HoursLeft(const World& W, const HourTypes& Hours);
	/// Spends hours out of this day. Returns what was actually spent, which is
	/// less than asked when the day is nearly gone and 0 when nobody is played:
	/// a day is a budget and never an overdraft.
	VAELEN_PLAYER_API uint32 SpendHours(World& W, const HourTypes& Hours, uint32 Wanted);

	struct HourStats
	{
		uint32 Records = 0; ///< hour records in the world; more than one is incoherent
		uint32 Days = 0;	///< days lived under the fine grain
		uint32 Missed = 0;	///< days it could not run
		uint32 Left = 0;	///< hours of the current day still unspent
		uint32 Bad = 0;		///< incoherent: see MeasureHours
		Hash64 Digest = 0;
	};

	/// Counts the days and checks what the grain must keep: one record to a
	/// world, on the person being played, never more hours spent than the day
	/// gave, and never a day that has not turned.
	VAELEN_PLAYER_API HourStats MeasureHours(const World& W, const Population::PersonTypes& Persons,
											 const PlayerTypes& Player, const HourTypes& Hours, const HourRules& Rules);
} // namespace Vaelen::Player
