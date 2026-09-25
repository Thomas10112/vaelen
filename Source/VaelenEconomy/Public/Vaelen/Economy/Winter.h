// VAELEN - VaelenEconomy
// Phase 18.06: the winter, yearly, as a consequence - fuel through the
// ledger, grain from the stores, chill on the people, deaths where nobody is
// detailed, and two events the chronicle can put into words.
//
// STATUS: PROTOTYPE (Phase 18) - unit/deterministic tests in Tests/Economy
//
// NOT A DISASTER (ADR-0152). A winter comes every year at the same rows,
// which is not what an omen is; a fifth DisasterKind would resize
// DisasterState and refuse every image of every world with the option off,
// and the omen loop would shift every draw. So the winter is its own yearly
// system, behind Options::Climate, that reads the climate of Sim/Climate.h
// (the ONE RegionYear the harvest and the view read too) and takes no draw:
// the year's variation is a hash, so the same year from one image is the
// same winter.
//
// WHAT A WINTER DOES, per region in index order, at the year's turn:
//   - the coarse people of a region nobody is simulating person by person
//     die by ColdDeathsPerMille of the severity, through the disasters' own
//     KillShare (a detailed region's deaths belong to the need system, which
//     judges the chill and names this winter);
//   - the WinterEvent is published for a winter of severity 1 or more;
//   - the fuel: ColdSum / DegreeDaysPerTimber timber per TimberPerPersons
//     people is taken from the common stock through the ledger (MoveStock,
//     StockTaken with the winter as cause, so Test_Ledger's Dark stays 0),
//     and what could not be taken is the exposure;
//   - the grain: a winter of severity 1 or more takes WinterGrainPerMille of
//     the common grain and of every house's, the winter as cause;
//   - the chill: every person of a detailed region (not the region living
//     by the day, 18.08) is chilled by Exposure x ColdSum / DegreeDaysPerChill,
//     where the exposure falls with the fuel covered, a settlement standing
//     in the region and a cloth stock that clothes its people;
//   - the WinterForeseen event for the coming year, when it is a winter of
//     severity 2 or more AND harder than the region's usual - the same rule
//     that makes a winter history in the chronicle (WinterIsHistory).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Warmth.h"
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Economy
{
	struct WinterRules
	{
		uint32 DegreeDaysPerTimber = 200; ///< one timber warms TimberPerPersons people through this much cold
		uint32 TimberPerPersons = 10;	  ///< keep equal to ProductionRules::TimberPerPersons, the yearly burn's
		uint32 ClothPerPersons =
			25; ///< a cloth stock of People / this clothes the region (ProductionRules::ClothWearPerPersons)
		uint32 ClothWarmthPerMille = 300;				///< the exposure a clothed region is spared
		uint32 ShelterPerMille = 400;					///< the exposure a settlement standing in the region is spared
		uint32 DegreeDaysPerChill = 20;					///< at full exposure, one chill per this many degree-days
		uint32 WinterGrainPerMille[3] = {50, 120, 250}; ///< of every stock's grain, by severity 1..3
		uint32 ColdDeathsPerMille[4] = {0, 2, 8, 20};	///< of a coarse region's people, by severity 0..3
		/// The region whose people are chilled by the day (18.08): the yearly
		/// pass leaves them alone, as the yearly food burn does. 0 is nowhere.
		uint32 DailyRegion = 0;
		WorldGen::ClimateRules Climate; ///< the lines and bands; the view's and the harvest's are the same rules
	};

	/// Yearly, behind Options::Climate: the winter just lain and the one coming.
	class VAELEN_ECONOMY_API WinterSystem final : public ISystem
	{
	public:
		WinterSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 Population::FamilyTypes InFamilies, EconomyTypes InEconomy, Population::WarmthTypes InWarmth,
					 WinterRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Warmth(InWarmth), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Winter"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Lod"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after another yearly system too (Stocks, so the houses are
		/// settled before the winter takes from them). The system must exist.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: a settlement standing in a region shelters its people.
		void ObserveSettlements(ComponentType<SettlementInfo> InSettlements) noexcept
		{
			Settlements = InSettlements;
			HasSettlements = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		EconomyTypes Economy;
		Population::WarmthTypes Warmth;
		WinterRules Rules;
		ComponentType<SettlementInfo> Settlements;
		bool HasSettlements = false;
	};

	/// What the winters did, from the log: the events by severity, the coarse
	/// dead, and every unit of timber and grain a winter took (StockTaken
	/// events whose cause is a Winter). Region 0 measures the world.
	struct WinterStats
	{
		uint32 Winters[4] = {}; ///< Winter events by severity (index 0 unused: none is published at 0)
		uint32 Foreseen = 0;
		uint32 ColdDeaths = 0; ///< coarse, from the payloads
		uint64 TimberTaken = 0;
		uint64 GrainTaken = 0;
		uint32 HousesTaken = 0; ///< house stocks a winter took grain from
	};
	VAELEN_ECONOMY_API WinterStats MeasureWinters(const World& W, uint32 Region);
} // namespace Vaelen::Economy
