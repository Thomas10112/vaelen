// VAELEN - VaelenSim tests
// SimClock and the AELVOR calendar: pure tick <-> date conversion, boundaries,
// custom rules, constexpr evaluation.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/SimClock.h"

using namespace Vaelen;

namespace
{
	constexpr Calendar DefaultCalendar{};
	constexpr CalendarRules DefaultRules{};
} // namespace

static_assert(DefaultRules.IsValid());
static_assert(DefaultRules.TicksPerDay() == 24);
static_assert(DefaultRules.TicksPerMonth() == 720);
static_assert(DefaultRules.TicksPerYear() == 8640);
static_assert(DefaultRules.DaysPerYear() == 360);
static_assert(DefaultRules.SeasonsPerYear() == 4);
static_assert(DefaultCalendar.ToDate(0) == CalendarDate{});
static_assert(DefaultCalendar.ToDate(8640).Year == 1);
static_assert(DefaultCalendar.ToTick(3, 11, 29, 23) == 3 * 8640 + 8639);
static_assert(DefaultCalendar.YearOf(Calendar::MaxTick) == Calendar::MaxTick / 8640);

VAELEN_TEST(SimClock, DefaultCalendarBoundaries)
{
	const CalendarDate Last = DefaultCalendar.ToDate(8639);
	VT_CHECK_EQ(Last.Year, uint64{0});
	VT_CHECK_EQ(Last.Season, uint32{3});
	VT_CHECK_EQ(Last.Month, uint32{11});
	VT_CHECK_EQ(Last.Day, uint32{29});
	VT_CHECK_EQ(Last.DayOfYear, uint32{359});
	VT_CHECK_EQ(Last.Hour, uint32{23});
	VT_CHECK_EQ(Last.TickOfHour, uint32{0});

	const CalendarDate First = DefaultCalendar.ToDate(8640);
	VT_CHECK_EQ(First.Year, uint64{1});
	VT_CHECK_EQ(First.Season, uint32{0});
	VT_CHECK_EQ(First.Month, uint32{0});
	VT_CHECK_EQ(First.Day, uint32{0});
	VT_CHECK_EQ(First.Hour, uint32{0});

	// Season boundaries: months 0-2, 3-5, 6-8, 9-11.
	VT_CHECK_EQ(DefaultCalendar.ToDate(DefaultCalendar.ToTick(0, 2, 29, 23)).Season, uint32{0});
	VT_CHECK_EQ(DefaultCalendar.ToDate(DefaultCalendar.ToTick(0, 3, 0, 0)).Season, uint32{1});
	VT_CHECK_EQ(DefaultCalendar.ToDate(DefaultCalendar.ToTick(0, 9, 0, 0)).Season, uint32{3});
	VT_CHECK(DefaultCalendar.IsFirstTickOfDay(48));
	VT_CHECK(!DefaultCalendar.IsFirstTickOfDay(49));
	VT_CHECK(DefaultCalendar.IsFirstTickOfYear(17280));
	VT_CHECK(!DefaultCalendar.IsFirstTickOfYear(17281));
	VT_CHECK_EQ(DefaultCalendar.DayIndexOf(8640 + 24 * 5 + 7), uint64{365});
}

VAELEN_TEST(SimClock, TickDateRoundTripAcrossTheWholeRange)
{
	const SimTick Samples[] = {0,
							   1,
							   23,
							   24,
							   8639,
							   8640,
							   12345678,
							   1000000000ull,
							   uint64{1} << 40,
							   uint64{1} << 63,
							   Calendar::MaxTick - 1,
							   Calendar::MaxTick};
	for (const SimTick Tick : Samples)
	{
		const CalendarDate Date = DefaultCalendar.ToDate(Tick);
		VT_CHECK_EQ(DefaultCalendar.ToTick(Date.Year, Date.Month, Date.Day, Date.Hour, Date.TickOfHour), Tick);
		VT_CHECK(Date.Month < 12 && Date.Day < 30 && Date.Hour < 24 && Date.Season < 4);
		VT_CHECK_EQ(Date.DayOfYear, Date.Month * 30 + Date.Day);
	}
	// Every tick of the first two years round-trips (exhaustive small range).
	for (SimTick Tick = 0; Tick < 2 * 8640; ++Tick)
	{
		const CalendarDate Date = DefaultCalendar.ToDate(Tick);
		VT_REQUIRE_EQ(DefaultCalendar.ToTick(Date.Year, Date.Month, Date.Day, Date.Hour, Date.TickOfHour), Tick);
	}
}

VAELEN_TEST(SimClock, CustomRulesAndValidation)
{
	CalendarRules Rules;
	Rules.TicksPerHour = 4;
	Rules.HoursPerDay = 10;
	Rules.DaysPerMonth = 20;
	Rules.MonthsPerYear = 8;
	Rules.MonthsPerSeason = 2;
	VT_REQUIRE(Rules.IsValid());
	const Calendar Cal(Rules);
	VT_CHECK_EQ(Rules.TicksPerDay(), uint64{40});
	VT_CHECK_EQ(Rules.TicksPerYear(), uint64{6400});
	VT_CHECK_EQ(Rules.SeasonsPerYear(), uint32{4});
	const CalendarDate D = Cal.ToDate(6400 + 40 * 21 + 4 * 3 + 2);
	VT_CHECK_EQ(D.Year, uint64{1});
	VT_CHECK_EQ(D.Month, uint32{1});
	VT_CHECK_EQ(D.Day, uint32{1});
	VT_CHECK_EQ(D.Hour, uint32{3});
	VT_CHECK_EQ(D.TickOfHour, uint32{2});
	VT_CHECK_EQ(D.Season, uint32{0});
	VT_CHECK_EQ(Cal.ToTick(D.Year, D.Month, D.Day, D.Hour, D.TickOfHour), uint64{6400 + 40 * 21 + 4 * 3 + 2});

	CalendarRules Zero;
	Zero.DaysPerMonth = 0;
	VT_CHECK(!Zero.IsValid());
	CalendarRules UnevenSeasons;
	UnevenSeasons.MonthsPerSeason = 5; // 12 % 5 != 0
	VT_CHECK(!UnevenSeasons.IsValid());
	CalendarRules NoSeason;
	NoSeason.MonthsPerSeason = 0;
	VT_CHECK(!NoSeason.IsValid());
	VT_CHECK_EQ(NoSeason.SeasonsPerYear(), uint32{0});
}

VAELEN_TEST(SimClock, ClockAdvancesOneTickAtATime)
{
	SimClock Clock;
	VT_CHECK_EQ(Clock.Now(), SimTick{0});
	for (int i = 0; i < 8640; ++i)
	{
		Clock.Advance();
	}
	VT_CHECK_EQ(Clock.Now(), SimTick{8640});
	VT_CHECK_EQ(Clock.Date().Year, uint64{1});
	VT_CHECK(Clock.GetRules() == CalendarRules{});
	VT_CHECK(Clock.GetCalendar().Rules == CalendarRules{});

	SimClock Restored;
	Restored.Restore(8640);
	VT_CHECK(Restored == Clock);
	Restored.Advance();
	VT_CHECK(!(Restored == Clock));

	constexpr SimClock Started(100);
	static_assert(Started.Now() == 100);
	static_assert(Started.Date().Hour == 4);
}
