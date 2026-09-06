// VAELEN - VaelenSim
// Phase 03.05: disasters and omens.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// Disasters are tied to the world: drought where the land is dry, flood where
// rivers run, eruption under high mountains, plague where people crowd. Every
// year each region at risk may receive an omen (an event about the region,
// with the risk that produced it); the next year an omen may become a
// disaster whose cause is the omen. Every disaster therefore has a place and
// a cause. A disaster kills a share of the region's people (per culture, in
// proportion), shakes the faith held there, may request a religion founding
// where no faith is held and may request a new era when severe. Hazards are a
// derived cache over the world layers; every piece of state (counters,
// pending omens, disaster records) lives in components.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	enum class DisasterKind : uint32
	{
		Drought = 0,
		Flood,
		Eruption,
		Plague,
		Count
	};
	VAELEN_SIM_API const char* DisasterName(DisasterKind Kind) noexcept;

	/// Component of a disaster record entity.
	struct DisasterInfo
	{
		uint32 Index = 0;	 ///< 1-based, in order of striking
		uint32 Kind = 0;	 ///< DisasterKind
		uint32 Region = 0;	 ///< region index (never 0)
		uint32 Severity = 0; ///< 1..3
		uint64 Struck = 0;	 ///< tick
		uint64 Omen = 0;	 ///< event id of the omen that caused it (never 0)
		uint32 Deaths = 0;
		uint32 PeopleBefore = 0;
	};
	static_assert(sizeof(DisasterInfo) == 40, "DisasterInfo must stay padding free");

	/// Static and dynamic risk of a region, 0..1000 per kind (derived, not state).
	struct RegionHazard
	{
		uint32 Tiles = 0;
		uint32 RiverTiles = 0;
		uint32 MountainTiles = 0;									///< tiles at least MountainElevation above the sea
		uint32 MoisturePerMille = 0;								///< mean moisture of the region's tiles
		uint32 Risk[static_cast<uint32>(DisasterKind::Count)] = {}; ///< plague risk is filled per year
	};

	struct PendingOmen
	{
		uint32 Region = 0;
		uint32 Kind = 0;
		uint32 Risk = 0;
		uint32 Reserved = 0;
		uint64 Event = 0; ///< omen event id
	};

	/// Singleton component on the disaster entity created by InitializeDisasters.
	struct DisasterState
	{
		static constexpr uint32 MaxPending = 32;
		uint32 Count = 0;		 ///< disasters struck
		uint32 Omens = 0;		 ///< omens published
		uint32 Dropped = 0;		 ///< omens lost because the queue was full
		uint32 PendingCount = 0; ///< omens waiting for next year
		uint32 PerKind[static_cast<uint32>(DisasterKind::Count)] = {};
		PendingOmen Pending[MaxPending];
	};
	static_assert(sizeof(DisasterState) == 32 + 24 * DisasterState::MaxPending, "DisasterState must stay padding free");

	struct DisasterTypes
	{
		ComponentType<DisasterInfo> Disaster;
		ComponentType<DisasterState> State;
		static DisasterTypes Declare(World& W);
	};

	/// Creates the disaster entity once (fresh worlds only). Null when it exists.
	VAELEN_SIM_API EntityHandle InitializeDisasters(World& W, const DisasterTypes& Types);

	struct DisasterRules
	{
		/// Yearly chance per mille of an omen in a region at full risk, per kind.
		uint32 OmenPerMille[static_cast<uint32>(DisasterKind::Count)] = {30, 25, 8, 40};
		/// Chance per mille that an omen strikes the next year. Severity: 2 with
		/// chance Risk / 2 per mille, then 3 with chance Risk / 4 per mille.
		uint32 StrikePerMille = 500;
		/// Deaths per mille of the region's people by kind and severity 1..3.
		uint32 DeathsPerMille[static_cast<uint32>(DisasterKind::Count)][3] = {
			{30, 80, 150}, {20, 50, 100}, {100, 250, 500}, {50, 150, 300}};
		uint32 DroughtMoisture = 350;		///< per mille of moisture below which drought risk rises
		uint32 FloodRiverShare = 120;		///< per mille of river tiles for full flood risk
		uint32 MountainElevation = 1200;	///< metres above the sea from which a tile counts as mountain
		uint32 EruptionMountainShare = 150; ///< per mille of mountain tiles for full eruption risk
		uint32 PlagueDensity = 40;			///< people per tile for full plague risk
		uint32 FaithShakenPerMille = 200;	///< believers of the majority faith lost where it struck
		uint32 FoundingSeverity = 2;		///< a disaster this severe founds a faith where none is held
		uint32 EraSeverity = 3;
		uint32 EraDeaths =
			200; ///< and at least this many deaths				///< a disaster this severe requests a new era
	};

	struct OmenPayload
	{
		uint32 Region = 0;
		uint32 Kind = 0;
		uint32 Risk = 0;
		uint32 Reserved = 0;
	};
	struct DisasterPayload
	{
		uint32 Region = 0;
		uint32 Kind = 0;
		uint32 Severity = 0;
		uint32 Deaths = 0;
	};
	inline constexpr EventType<OmenPayload> OmenEvent = MakeEventType<OmenPayload>("Omen");
	inline constexpr EventType<DisasterPayload> DisasterStruckEvent = MakeEventType<DisasterPayload>("DisasterStruck");

	/// Static hazards of every region (index 0 unused), from the world layers.
	VAELEN_SIM_API std::vector<RegionHazard> ComputeHazards(const World& W, const WorldGen::WorldSetup& Setup,
															const DisasterRules& Rules);
	/// Plague risk of a region from its people per tile.
	VAELEN_SIM_API uint32 PlagueRisk(uint32 People, uint32 Tiles, const DisasterRules& Rules) noexcept;

	/// Yearly: strikes the omens of last year (chance, severity, deaths per
	/// culture, faith shaken, founding and era requests), then draws this
	/// year's omens region by region and kind by kind.
	class VAELEN_SIM_API DisasterSystem final : public ISystem
	{
	public:
		DisasterSystem(World& InWorld, WorldGen::WorldSetup InSetup, PopulationTypes InPopulation,
					   DisasterTypes InTypes, DisasterRules InRules) noexcept
			: Owner(&InWorld), Setup(InSetup), Population(InPopulation), Types(InTypes), Rules(InRules)
		{
		}
		/// Optional: shake the faith of struck regions and request foundings.
		void ShakeFaith(ReligionTypes InReligion, ReligionSystem* InReligions) noexcept
		{
			Religion = InReligion;
			Religions = InReligions;
			HasReligion = true;
		}
		/// Optional: severe disasters open eras.
		void OpenEras(EraSystem* InEras) noexcept { Eras = InEras; }
		const char* GetName() const noexcept override { return "Disasters"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		WorldGen::WorldSetup Setup;
		PopulationTypes Population;
		DisasterTypes Types;
		DisasterRules Rules;
		ReligionTypes Religion;
		ReligionSystem* Religions = nullptr;
		bool HasReligion = false;
		EraSystem* Eras = nullptr;
		Hash64 HazardDigest = 0;		   ///< derived cache, not state
		std::vector<RegionHazard> Hazards; ///< derived cache, not state
	};

	struct DisasterStats
	{
		uint32 Total = 0;
		uint32 PerKind[static_cast<uint32>(DisasterKind::Count)] = {};
		uint32 PerSeverity[3] = {};
		uint64 Deaths = 0;
		uint32 Omens = 0;
		uint32 Dropped = 0;
		uint32 Pending = 0;
		uint32 RegionsStruck = 0; ///< distinct regions
	};
	VAELEN_SIM_API DisasterStats MeasureDisasters(const World& W, const DisasterTypes& Types);
} // namespace Vaelen::History
