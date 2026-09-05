// VAELEN - VaelenSim
// Simulation time as world state: an integer tick counter and the calendar
// of AELVOR derived from it.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// Rules (master prompt section 33 and 35):
//   - Time is a count of fixed-duration ticks (SimTick, uint64). There is no
//     floating-point delta time anywhere in the simulation: accelerating the
//     game means running more ticks per real second, never longer ticks.
//   - The wall clock is never consulted (purity rules R1/R4 forbid <chrono>).
//   - The calendar is a pure function of the tick and of the CalendarRules,
//     which are data (defaults below, overridable per world in later phases).
//
// Default AELVOR calendar: 1 tick = 1 hour; 24 hours per day; 30 days per
// month; 12 months per year (360 days); 4 seasons of 3 months. These are
// deliberately regular: irregular rules (leap days) are a data decision for
// Phase 02/03, not a kernel invariant.
#pragma once

#include "Vaelen/Core/CoreTypes.h"

namespace Vaelen
{
	/// Number of ticks since the world's epoch (tick 0 = start of year 0).
	using SimTick = uint64;

	struct CalendarRules
	{
		uint32 TicksPerHour = 1;
		uint32 HoursPerDay = 24;
		uint32 DaysPerMonth = 30;
		uint32 MonthsPerYear = 12;
		uint32 MonthsPerSeason = 3;

		constexpr uint64 TicksPerDay() const noexcept { return uint64{TicksPerHour} * HoursPerDay; }
		constexpr uint64 TicksPerMonth() const noexcept { return TicksPerDay() * DaysPerMonth; }
		constexpr uint64 TicksPerYear() const noexcept { return TicksPerMonth() * MonthsPerYear; }
		constexpr uint32 DaysPerYear() const noexcept { return DaysPerMonth * MonthsPerYear; }
		constexpr uint32 SeasonsPerYear() const noexcept
		{
			return MonthsPerSeason == 0 ? 0 : MonthsPerYear / MonthsPerSeason;
		}

		/// All factors non-zero and the season length divides the year.
		constexpr bool IsValid() const noexcept
		{
			return TicksPerHour > 0 && HoursPerDay > 0 && DaysPerMonth > 0 && MonthsPerYear > 0 &&
				   MonthsPerSeason > 0 && MonthsPerYear % MonthsPerSeason == 0;
		}

		constexpr bool operator==(const CalendarRules&) const noexcept = default;
	};

	/// A calendar date, every field zero-based.
	struct CalendarDate
	{
		uint64 Year = 0;
		uint32 Season = 0; ///< 0..SeasonsPerYear-1
		uint32 Month = 0;  ///< 0..MonthsPerYear-1 (within the year)
		uint32 Day = 0;	   ///< 0..DaysPerMonth-1 (within the month)
		uint32 DayOfYear = 0;
		uint32 Hour = 0; ///< 0..HoursPerDay-1
		uint32 TickOfHour = 0;

		constexpr bool operator==(const CalendarDate&) const noexcept = default;
	};

	/// Converts a tick to a date and back. Pure, constexpr, no state.
	struct Calendar
	{
		CalendarRules Rules;

		constexpr explicit Calendar(CalendarRules InRules = CalendarRules{}) noexcept : Rules(InRules) {}

		constexpr CalendarDate ToDate(SimTick Tick) const noexcept
		{
			CalendarDate Date;
			const uint64 TicksPerYear = Rules.TicksPerYear();
			Date.Year = Tick / TicksPerYear;
			const uint64 TickOfYear = Tick % TicksPerYear;
			const uint64 TicksPerDay = Rules.TicksPerDay();
			Date.DayOfYear = static_cast<uint32>(TickOfYear / TicksPerDay);
			Date.Month = Date.DayOfYear / Rules.DaysPerMonth;
			Date.Day = Date.DayOfYear % Rules.DaysPerMonth;
			Date.Season = Date.Month / Rules.MonthsPerSeason;
			const uint64 TickOfDay = TickOfYear % TicksPerDay;
			Date.Hour = static_cast<uint32>(TickOfDay / Rules.TicksPerHour);
			Date.TickOfHour = static_cast<uint32>(TickOfDay % Rules.TicksPerHour);
			return Date;
		}

		/// Inverse of ToDate for in-range fields (Month, Day, Hour, TickOfHour
		/// below their limits); Season and DayOfYear are derived and ignored.
		constexpr SimTick ToTick(uint64 Year, uint32 Month, uint32 Day, uint32 Hour = 0,
								 uint32 TickOfHour = 0) const noexcept
		{
			return Year * Rules.TicksPerYear() + (uint64{Month} * Rules.DaysPerMonth + Day) * Rules.TicksPerDay() +
				   uint64{Hour} * Rules.TicksPerHour + TickOfHour;
		}

		constexpr uint64 YearOf(SimTick Tick) const noexcept { return Tick / Rules.TicksPerYear(); }
		constexpr uint32 SeasonOf(SimTick Tick) const noexcept { return ToDate(Tick).Season; }
		constexpr uint64 DayIndexOf(SimTick Tick) const noexcept { return Tick / Rules.TicksPerDay(); }
		constexpr bool IsFirstTickOfDay(SimTick Tick) const noexcept { return Tick % Rules.TicksPerDay() == 0; }
		constexpr bool IsFirstTickOfYear(SimTick Tick) const noexcept { return Tick % Rules.TicksPerYear() == 0; }

		/// Highest tick that ToDate can represent without overflow in Year
		/// arithmetic (SimTick is exhaustive: any uint64 is a valid tick).
		static constexpr SimTick MaxTick = ~SimTick{0};
	};

	/// The world clock: the current tick and how many ticks have been
	/// advanced. Part of the world state; saved and restored verbatim.
	class SimClock
	{
	public:
		constexpr SimClock() noexcept = default;
		constexpr explicit SimClock(SimTick StartTick, CalendarRules InRules = CalendarRules{}) noexcept
			: Current(StartTick), Rules(InRules)
		{
		}

		constexpr SimTick Now() const noexcept { return Current; }
		constexpr Calendar GetCalendar() const noexcept { return Calendar(Rules); }
		constexpr const CalendarRules& GetRules() const noexcept { return Rules; }
		constexpr CalendarDate Date() const noexcept { return Calendar(Rules).ToDate(Current); }

		/// Advances by exactly one tick. The only way time moves forward.
		constexpr void Advance() noexcept { ++Current; }

		/// Restores a saved tick (snapshots only; never used to skip time).
		constexpr void Restore(SimTick Tick) noexcept { Current = Tick; }

		constexpr bool operator==(const SimClock&) const noexcept = default;

	private:
		SimTick Current = 0;
		CalendarRules Rules{};
	};
} // namespace Vaelen
