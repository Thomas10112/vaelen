// VAELEN - VaelenSim
// Phase 03.02: cultures and coarse population per region.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// Population is counted per region and per culture (integers), never per
// person (Phase 04). Cultures are entities seeded on the most fertile regions;
// a RegionPopulation component on every region entity holds up to MaxCultures
// (culture, count) slots and the region's carrying capacity derived from its
// biomes and deposits. The yearly PopulationSystem grows counts logistically
// toward capacity, assimilates minorities below a share and splits far-flung
// majorities into new cultures; the monthly MigrationSystem moves a share of
// crowded regions to their least crowded neighbour along the region graph.
// Every step is integer arithmetic ordered by index; all state is components.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	/// Component of a culture entity (ids of kind Culture).
	struct CultureInfo
	{
		uint32 Index = 0;	   ///< 1-based, in order of founding
		uint32 HomeRegion = 0; ///< region index where it was founded
		uint32 Parent = 0;	   ///< culture it split from (0 for a root culture)
		uint32 Generation = 0; ///< 0 root, parent's + 1 otherwise
		uint64 Founded = 0;	   ///< tick
		Hash64 Identity = 0;   ///< seed for names and traits (03.03+), derived from the world seed
	};

	/// Component added to every region entity: counts per culture.
	struct RegionPopulation
	{
		static constexpr uint32 MaxCultures = 6;

		uint32 Culture[MaxCultures] = {}; ///< culture index per slot, 0 = free
		uint32 Count[MaxCultures] = {};	  ///< people per slot
		uint32 Total = 0;				  ///< sum of Count
		uint32 Capacity = 0;			  ///< carrying capacity from biomes and deposits
		uint32 Majority = 0;			  ///< culture with the most people (0 when empty)
		uint32 SettledSince = 0;		  ///< years of continuous settlement (saturates)

		uint32 SlotOf(uint32 CultureIndex) const noexcept;
		/// Adds people of a culture; returns false when no slot is free.
		bool Add(uint32 CultureIndex, uint32 People) noexcept;
		/// Removes people of a culture (clamped); frees the slot at zero.
		uint32 Remove(uint32 CultureIndex, uint32 People) noexcept;
		void Recount() noexcept;
	};

	struct PopulationTypes
	{
		ComponentType<CultureInfo> Culture;
		ComponentType<RegionPopulation> Population;

		static PopulationTypes Declare(World& W);
	};

	struct PopulationRules
	{
		uint32 SeedCultures = 4; ///< cultures founded at the start
		uint32 SeedPeople = 200; ///< people per founding region
		uint32 CapacityPerTile[static_cast<uint32>(WorldGen::Biome::Count)] = {
			0,	// Ocean
			0,	// Ice
			2,	// Tundra
			6,	// BorealForest
			4,	// ColdSteppe
			14, // TemperateForest
			16, // Grassland
			6,	// Scrubland
			12, // TropicalForest
			10, // Savanna
			1,	// Desert
			1,	// Alpine
		};
		uint32 CapacityPerFertileDeposit = 120;
		uint32 CapacityPerRiverTile = 8;
		uint32 GrowthPerMille = 25;				 ///< yearly logistic growth rate
		uint32 DeclinePerMille = 300;			 ///< yearly share of the excess above capacity that dies
		uint32 MigrationThresholdPerMille = 800; ///< crowding above which a region emits migrants
		uint32 MigrationSharePerMille = 40;		 ///< monthly share of the source that leaves
		uint32 MinimumWave = 10;				 ///< migrants below this stay home
		uint32 AssimilationSharePerMille = 50;	 ///< minorities below this share of the region assimilate
		uint32 SplitDistance = 3;				 ///< graph distance from home beyond which a majority may split
		uint32 SplitYears = 100;				 ///< years of settlement before a split
	};

	// ── Events ─────────────────────────────────────────────────────────────
	struct RegionPeople
	{
		uint32 Region = 0;
		uint32 Culture = 0;
		uint32 People = 0;
		uint32 Reserved = 0;
	};
	struct CulturePayload
	{
		uint32 Culture = 0;
		uint32 Region = 0;
		uint32 Parent = 0;
		uint32 Reserved = 0;
	};
	inline constexpr EventType<CulturePayload> CultureFoundedEvent = MakeEventType<CulturePayload>("CultureFounded");
	inline constexpr EventType<CulturePayload> CultureSplitEvent = MakeEventType<CulturePayload>("CultureSplit");
	inline constexpr EventType<RegionPeople> RegionSettledEvent = MakeEventType<RegionPeople>("RegionSettled");
	inline constexpr EventType<RegionPeople> RegionAbandonedEvent = MakeEventType<RegionPeople>("RegionAbandoned");
	inline constexpr EventType<RegionPeople> MigrationWaveEvent = MakeEventType<RegionPeople>("MigrationWave");

	/// Adds a RegionPopulation with its capacity to every region entity, then
	/// founds Rules.SeedCultures cultures on the highest-capacity regions (one
	/// per landmass-spread choice: the best region, then the best region not
	/// adjacent to a chosen one, and so on). Call once on a fresh world after
	/// GenerateWorld. Returns the number of cultures founded.
	VAELEN_SIM_API uint32 SeedCultures(World& W, const WorldGen::WorldSetup& Setup, const PopulationTypes& Types,
									   const PopulationRules& Rules, SimTick Tick);

	/// Yearly (LOD World): logistic growth, decline above capacity,
	/// assimilation, settlement bookkeeping, culture splits.
	class VAELEN_SIM_API PopulationSystem final : public ISystem
	{
	public:
		PopulationSystem(World& InWorld, WorldGen::WorldSetup InSetup, PopulationTypes InTypes,
						 PopulationRules InRules) noexcept
			: Owner(&InWorld), Setup(InSetup), Types(InTypes), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Population"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		WorldGen::WorldSetup Setup;
		PopulationTypes Types;
		PopulationRules Rules;
	};

	/// Monthly (LOD Statistic): migration from crowded regions to their least
	/// crowded neighbour. The region graph is derived from the immutable
	/// region layer and cached by its digest.
	class VAELEN_SIM_API MigrationSystem final : public ISystem
	{
	public:
		MigrationSystem(World& InWorld, WorldGen::WorldSetup InSetup, PopulationTypes InTypes,
						PopulationRules InRules) noexcept
			: Owner(&InWorld), Setup(InSetup), Types(InTypes), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Migration"; }
		SimLod GetLod() const noexcept override { return SimLod::Statistic; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		WorldGen::WorldSetup Setup;
		PopulationTypes Types;
		PopulationRules Rules;
		// Derived cache (not state): rebuilt when the region layer changes.
		Hash64 GraphDigest = 0;
		WorldGen::RegionGraph Graph;
	};

	struct PopulationStats
	{
		uint64 People = 0;
		uint64 Capacity = 0;
		uint32 Cultures = 0;
		uint32 SettledRegions = 0;
		uint32 Regions = 0;
		uint32 MaxCulturesInARegion = 0;
		uint32 LargestCulture = 0; ///< index
		uint64 LargestCulturePeople = 0;
	};
	VAELEN_SIM_API PopulationStats MeasurePopulation(const World& W, const PopulationTypes& Types);

	/// Region picture with the majority culture's glyph ('.' settled by none, '~' sea).
	VAELEN_SIM_API void ExportCultureAscii(const World& W, const WorldGen::WorldSetup& Setup,
										   const PopulationTypes& Types, uint32 Columns, std::string& Out);
} // namespace Vaelen::History
