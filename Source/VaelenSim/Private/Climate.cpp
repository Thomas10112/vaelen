// VAELEN - VaelenSim
// Phase 18 task 18.03: the current temperature as a function of the mean
// layer, the row and the day; a region's year as the sum of its days.
//
// STATUS: PROTOTYPE (Phase 18 task 18.03)
#include "Vaelen/Sim/Climate.h"

#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

namespace Vaelen::WorldGen
{
	Fix64 SeasonalAmplitude(Fix64 Latitude) noexcept
	{
		// The same expression as SeasonalOffset's (WorldGen.cpp), and
		// Tests/Sim/Test_Climate.cpp holds the two equal at the four midpoints.
		return Fix64::FromInt(4) + Fix64::Abs(Latitude) * 16;
	}

	Fix64 DayOffset(Fix64 Latitude, uint32 DayOfYear, uint32 DaysPerYear) noexcept
	{
		const uint32 Quarter = DaysPerYear / 4u;
		if (Quarter == 0u)
		{
			return Fix64::Zero();
		}
		// Measured from mid-spring, which is where the wave crosses zero going
		// up: DaysPerYear/8 into the year, day 45 of 360.
		const uint32 T = (DayOfYear % DaysPerYear + DaysPerYear - Quarter / 2u) % DaysPerYear;
		const Fix64 A = SeasonalAmplitude(Latitude);
		const Fix64 Q = Fix64::FromInt(static_cast<int32>(Quarter));
		// Each quarter is exact at both ends: A * Quarter / Quarter is A, and
		// A * 0 / Quarter is 0, whatever the fixed point rounds in between.
		if (T < Quarter)
		{
			return A * Fix64::FromInt(static_cast<int32>(T)) / Q;
		}
		if (T < 2u * Quarter)
		{
			return A * Fix64::FromInt(static_cast<int32>(2u * Quarter - T)) / Q;
		}
		if (T < 3u * Quarter)
		{
			return -(A * Fix64::FromInt(static_cast<int32>(T - 2u * Quarter)) / Q);
		}
		return -(A * Fix64::FromInt(static_cast<int32>(4u * Quarter - T)) / Q);
	}

	Fix64 TemperatureOn(Fix64 Mean, Fix64 Latitude, uint32 DayOfYear, uint32 DaysPerYear) noexcept
	{
		return Mean + DayOffset(Latitude, DayOfYear, DaysPerYear);
	}

	Fix64 TileTemperatureOn(const WorldMap& Map, const WorldLayers& Layers, uint32 Tile, uint32 DayOfYear,
							uint32 DaysPerYear) noexcept
	{
		if (!Map.IsReady())
		{
			return Fix64::Zero();
		}
		const WorldGrid& Grid = Map.Grid();
		if (Grid.Width == 0u || Tile >= Grid.Width * Grid.Height)
		{
			return Fix64::Zero();
		}
		const TileLayer<int64>& Mean = Map.GetLayer(Layers.Temperature);
		return TemperatureOn(Fix64::FromRaw(Mean[Tile]), LatitudeOfRow(Grid, Tile / Grid.Width), DayOfYear,
							 DaysPerYear);
	}

	YearShape ShapeYear(Fix64 Mean, Fix64 Latitude, const ClimateRules& Rules, uint32 DaysPerYear) noexcept
	{
		YearShape Out;
		Out.ColdSum = Fix64::Zero();
		Out.Coldest = Fix64::Max();
		Out.Warmest = Fix64::Min();
		for (uint32 Day = 0; Day < DaysPerYear; ++Day)
		{
			const Fix64 T = TemperatureOn(Mean, Latitude, Day, DaysPerYear);
			if (T < Rules.ColdLine)
			{
				++Out.FrostDays;
				Out.ColdSum += Rules.ColdLine - T;
			}
			if (T >= Rules.GrowLine)
			{
				++Out.GrowingDays;
			}
			Out.Coldest = Fix64::MinOf(Out.Coldest, T);
			Out.Warmest = Fix64::MaxOf(Out.Warmest, T);
		}
		if (DaysPerYear == 0u)
		{
			Out.Coldest = Fix64::Zero();
			Out.Warmest = Fix64::Zero();
		}
		return Out;
	}

	Fix64 YearVariation(uint64 Seed, uint64 Year, uint32 Region, Fix64 Amplitude) noexcept
	{
		// LatticeValue is in [-1, 1): every bit of the seed, the year and the
		// region reaches every bit of the answer, and no generator is touched.
		const int32 Y = static_cast<int32>(Year & 0x7fffffffull);
		const int32 R = static_cast<int32>(Region & 0x7fffffffu);
		return Amplitude * Noise::LatticeValue(Seed ^ 0x434c494d41544500ull /* "CLIMATE" */, Y, R);
	}

	namespace
	{
		/// The centroid tile of every region, by index; 0 for a region the pool
		/// does not hold. One pass over the pool, which is how Places.cpp reads
		/// the same thing.
		void CentroidsByRegion(const World& W, const WorldSetup& Setup, std::vector<uint32>& Out)
		{
			Out.assign(1u, 0u);
			W.Components()
				.GetPool(Setup.RegionTypes_.Region)
				.ForEach(
					[&Out](EntityHandle, const RegionInfo& R)
					{
						if (R.Index >= Out.size())
						{
							Out.resize(usize{R.Index} + 1u, 0u);
						}
						Out[R.Index] = R.CentroidTile;
					});
		}

		YearShape ShapeAt(const World& W, const WorldSetup& Setup, uint32 Region, uint32 Tile, uint64 Year,
						  const ClimateRules& Rules)
		{
			const WorldMap& Map = W.Map();
			const WorldGrid& Grid = Map.Grid();
			const TileLayer<int64>& Mean = Map.GetLayer(Setup.Layers.Temperature);
			const Fix64 ThisYear =
				Fix64::FromRaw(Mean[Tile]) + YearVariation(W.Config().Seed, Year, Region, Rules.YearAmplitude);
			return ShapeYear(ThisYear, LatitudeOfRow(Grid, Tile / Grid.Width), Rules,
							 W.Clock().GetRules().DaysPerYear());
		}
	} // namespace

	YearShape RegionYear(const World& W, const WorldSetup& Setup, uint32 Region, uint64 Year, const ClimateRules& Rules)
	{
		YearShape Empty;
		Empty.ColdSum = Fix64::Zero();
		Empty.Coldest = Fix64::Zero();
		Empty.Warmest = Fix64::Zero();
		if (Region == 0u || !W.Map().IsReady())
		{
			return Empty;
		}
		std::vector<uint32> Centroids;
		CentroidsByRegion(W, Setup, Centroids);
		if (Region >= Centroids.size())
		{
			return Empty;
		}
		const WorldGrid& Grid = W.Map().Grid();
		if (Centroids[Region] >= Grid.Width * Grid.Height)
		{
			return Empty;
		}
		return ShapeAt(W, Setup, Region, Centroids[Region], Year, Rules);
	}

	uint32 WinterSeverity(const YearShape& Year, const ClimateRules& Rules) noexcept
	{
		uint32 Severity = 0;
		for (uint32 Band = 0; Band < 3u; ++Band)
		{
			if (Year.ColdSum >= Fix64::FromInt(static_cast<int32>(Rules.SeverityDegreeDays[Band])))
			{
				Severity = Band + 1u;
			}
		}
		return Severity;
	}

	uint32 RegionCentroidTile(const World& W, const WorldSetup& Setup, uint32 Region)
	{
		if (Region == 0u || !W.Map().IsReady())
		{
			return 0u;
		}
		std::vector<uint32> Centroids;
		CentroidsByRegion(W, Setup, Centroids);
		const WorldGrid& Grid = W.Map().Grid();
		if (Region >= Centroids.size() || Centroids[Region] >= Grid.Width * Grid.Height)
		{
			return 0u;
		}
		return Centroids[Region];
	}

	void ShapeRegionYears(const World& W, const WorldSetup& Setup, uint64 Year, const ClimateRules& Rules,
						  std::vector<YearShape>& Out)
	{
		YearShape Empty;
		Empty.ColdSum = Fix64::Zero();
		Empty.Coldest = Fix64::Zero();
		Empty.Warmest = Fix64::Zero();
		Out.assign(1u, Empty);
		if (!W.Map().IsReady())
		{
			return;
		}
		std::vector<uint32> Centroids;
		CentroidsByRegion(W, Setup, Centroids);
		const WorldGrid& Grid = W.Map().Grid();
		Out.assign(Centroids.size(), Empty);
		for (uint32 Region = 1; Region < Centroids.size(); ++Region)
		{
			if (Centroids[Region] < Grid.Width * Grid.Height)
			{
				Out[Region] = ShapeAt(W, Setup, Region, Centroids[Region], Year, Rules);
			}
		}
	}
} // namespace Vaelen::WorldGen
