// VAELEN - VaelenSim
// Phase 18 task 18.03: the current temperature is a FUNCTION, and a region's
// year is its shape.
//
// The Temperature layer of 02.04 is an ANNUAL MEAN: the latitude band, the
// lapse and the noise are already in it (WorldGen.cpp, GenerateClimate).
// SeasonalOffset has existed since 02.04 with no production caller - four
// samples of a year, one per season. What is here puts a DAY under it: a
// triangle wave that is SeasonalOffset's value at the four season midpoints
// (days 45, 135, 225 and 315 of a 360-day year: 0, +A, 0, -A) and linear in
// between, added to the mean. No layer, no component, no state: a tile's
// temperature today is arithmetic over the layer and the row, and Phase 19
// reads it per tile per day for nothing. ADR-0151 says why not a layer - its
// NAME would move WorldMap::LayoutDigest, the image header and every state
// digest before a consumer existed.
//
// A region's YEAR is the 360-day sum at its centroid: frost days (below the
// cold line), growing days (at or above the growing line), the cold sum in
// degree-days, the coldest and the warmest day. ADR-0151 planned an arithmetic
// series; this is the discrete sum itself, because the sum IS the definition
// and 360 additions per region per year cost nothing (robust > performant),
// and because the panel's hand figures for the closed form were wrong once
// already. The pinned figures in Tests/Sim/Test_Climate.cpp are the sums.
//
// Nothing here draws from a generator: the year-to-year variation is a
// lattice hash of (seed, year, region), so the same seed and year give the
// same winter to a replay, to a partial re-simulation and to a save.
//
// STATUS: PROTOTYPE (Phase 18 task 18.03) - built and tested headless; read by
// nothing in production until 18.04 (the view) and 18.06 (the winter).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/FixedPoint.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::WorldGen
{
	/// The lines a climate is judged by. A RULE, in no digest (ADR-0090): a
	/// world declares nothing for these to exist, and the day they change the
	/// world does not.
	struct ClimateRules
	{
		Fix64 ColdLine = Fix64::Zero();		///< degrees; a day below it is a frost day, its deficit is cold
		Fix64 GrowLine = Fix64::FromInt(5); ///< degrees; a day at or above it grows
		uint32 GrowFullDays = 180;			///< growing days that reap a full harvest (18.07)
		uint32 SeverityDegreeDays[3] = {150u, 600u, 1500u}; ///< a winter's cold sum at severity 1, 2 and 3 (18.06)
		Fix64 YearAmplitude = Fix64::FromInt(3);			///< +- degrees a year differs from the mean, by hash
	};

	/// The calendar's year, for the callers that have no clock to ask: 12
	/// months of 30 days (SimClock.h). RegionYear asks the world's clock.
	inline constexpr uint32 DaysOfAYear = 360;

	/// 4 + 16|lat| degrees: SeasonalOffset's amplitude, named so that the two
	/// cannot drift apart without a test saying so.
	VAELEN_SIM_API Fix64 SeasonalAmplitude(Fix64 Latitude) noexcept;

	/// The offset of one day of the year from the annual mean: a triangle
	/// wave, 0 at day DaysPerYear/8 (mid-spring), +amplitude a quarter later
	/// (mid-summer), 0 (mid-autumn), -amplitude (mid-winter), linear between,
	/// so that `DayOffset(Lat, 45 + 90 s) == SeasonalOffset(Lat, s)` exactly on
	/// the raw value for a 360-day year. DayOfYear past the year wraps.
	VAELEN_SIM_API Fix64 DayOffset(Fix64 Latitude, uint32 DayOfYear, uint32 DaysPerYear = DaysOfAYear) noexcept;

	/// The mean plus the day's offset.
	VAELEN_SIM_API Fix64 TemperatureOn(Fix64 Mean, Fix64 Latitude, uint32 DayOfYear,
									   uint32 DaysPerYear = DaysOfAYear) noexcept;

	/// One tile today: its mean from the Temperature layer, its latitude from
	/// its row. Zero for a tile off the map or a map that is not ready.
	VAELEN_SIM_API Fix64 TileTemperatureOn(const WorldMap& Map, const WorldLayers& Layers, uint32 Tile,
										   uint32 DayOfYear, uint32 DaysPerYear = DaysOfAYear) noexcept;

	/// A year at one place, day by day and summed.
	struct YearShape
	{
		uint32 FrostDays = 0;	///< days below ClimateRules::ColdLine
		uint32 GrowingDays = 0; ///< days at or above ClimateRules::GrowLine
		Fix64 ColdSum;			///< degree-days below the cold line, summed over the year
		Fix64 Coldest;			///< the coldest day's temperature
		Fix64 Warmest;			///< the warmest day's
	};

	/// The year of a place whose annual mean is Mean at latitude Latitude: the
	/// DaysPerYear-term sum of TemperatureOn, judged by the rules.
	VAELEN_SIM_API YearShape ShapeYear(Fix64 Mean, Fix64 Latitude, const ClimateRules& Rules,
									   uint32 DaysPerYear = DaysOfAYear) noexcept;

	/// How a particular year differs from the mean at a region: a lattice hash
	/// of (seed, year, region) scaled to [-Amplitude, +Amplitude), never a draw
	/// from the world's generator, so the same seed and year give the same
	/// answer whatever else happened.
	VAELEN_SIM_API Fix64 YearVariation(uint64 Seed, uint64 Year, uint32 Region, Fix64 Amplitude) noexcept;

	/// The year of one region, at its centroid tile: the tile's mean plus the
	/// year's variation, shaped by the world's own calendar. Region 0, a region
	/// the world does not have, or a map that is not ready: an all-zero shape.
	/// The ONE function the harvest (18.07), the winter (18.06) and the view
	/// (18.04) call, so that two systems cannot disagree about the same region.
	VAELEN_SIM_API YearShape RegionYear(const World& W, const WorldSetup& Setup, uint32 Region, uint64 Year,
										const ClimateRules& Rules);

	/// Every region's year in one pass over the region pool, indexed by region
	/// (index 0 unused, all zero). What a yearly system calls; RegionYear is
	/// this for one region.
	VAELEN_SIM_API void ShapeRegionYears(const World& W, const WorldSetup& Setup, uint64 Year,
										 const ClimateRules& Rules, std::vector<YearShape>& Out);

	/// A winter's severity, 0..3, from the year's cold sum against
	/// ClimateRules::SeverityDegreeDays: 0 below the first band, 3 at or above
	/// the last. The one reading of "hard" the winter (18.06), the view
	/// (18.04) and the chronicle share.
	VAELEN_SIM_API uint32 WinterSeverity(const YearShape& Year, const ClimateRules& Rules) noexcept;

	/// The centroid tile of a region, from the region pool; 0 for region 0, a
	/// region the world does not have, or a map that is not ready. 18.04's
	/// view reads today's temperature there.
	VAELEN_SIM_API uint32 RegionCentroidTile(const World& W, const WorldSetup& Setup, uint32 Region);

	/// What RegionYear shapes a region's year from: the centroid's mean with
	/// the year's variation, its latitude, and the world's calendar (18.08).
	/// Valid is false exactly where RegionYear answers the all-zero shape.
	struct YearBasis
	{
		Fix64 Mean;
		Fix64 Latitude;
		uint32 DaysPerYear = DaysOfAYear;
		bool Valid = false;
	};
	VAELEN_SIM_API YearBasis RegionYearBasis(const World& W, const WorldSetup& Setup, uint32 Region, uint64 Year,
											 const ClimateRules& Rules);

	/// The cold sum of the days [0, LastDay] of a year, summed in the order
	/// ShapeYear sums it, so the last day's figure IS the year's ColdSum to
	/// the raw value (18.08).
	VAELEN_SIM_API Fix64 ColdSumThrough(Fix64 Mean, Fix64 Latitude, const ClimateRules& Rules, uint32 LastDay,
										uint32 DaysPerYear = DaysOfAYear) noexcept;

	/// The chill a day puts on a person living at the day (18.08): the
	/// difference of two exact figures, floor(floor(cold sum through today) x
	/// Exposure / 1000 / DegreeDaysPerChill) minus the same through yesterday,
	/// so a year of days sums to what the yearly winter (18.06) puts on a
	/// person of the same region at the same exposure, to the unit, and a day
	/// that is not a frost day takes nothing. The schedule of ShareOfDay
	/// (Needs.h), for a cold that does not fall evenly.
	VAELEN_SIM_API uint32 ChillOfDay(Fix64 Mean, Fix64 Latitude, const ClimateRules& Rules, uint32 DayOfYear,
									 uint32 DaysPerYear, uint32 ExposurePerMille, uint32 DegreeDaysPerChill) noexcept;

	/// What a winter did to a region (18.06): the payload of Winter, the one
	/// just lain, and WinterForeseen, the one coming. DECLARED HERE, in the
	/// layer that owns the climate, for the reason Sim declares
	/// DisasterStruckEvent for everybody: the chronicle's words for it live in
	/// HistoryText.cpp, and Sim cannot read a type a higher layer declares.
	/// Economy's WinterSystem publishes both; Population's need system reads
	/// Winter to name the deaths of the cold.
	struct WinterPayload
	{
		uint32 Region = 0;
		uint32 Severity = 0; ///< 0..3 by ClimateRules::SeverityDegreeDays
		uint32 ColdSum = 0;	 ///< degree-days below the cold line, floored
		uint32 Deaths = 0;	 ///< coarse deaths of the cold; 0 in a detailed region and for the one foreseen
		uint32 People = 0;	 ///< who lived there when it lay
		uint32 Usual = 0;	 ///< the region's severity in a year without variation: the climate, not the news
	};
	inline constexpr EventType<WinterPayload> WinterEvent = MakeEventType<WinterPayload>("Winter");
	inline constexpr EventType<WinterPayload> WinterForeseenEvent = MakeEventType<WinterPayload>("WinterForeseen");

	/// Whether a Winter event is history rather than climate: a winter harder
	/// than the region's usual one, on a peopled region. The chronicle keeps
	/// these and lets the yearly winters of the tundra pass, which at AELVOR
	/// 128 is the difference between a line a year and thirty-five.
	VAELEN_SIM_API bool WinterIsHistory(const Event& E) noexcept;
} // namespace Vaelen::WorldGen
