// VAELEN - VaelenEconomy
// Phase 06.05: wealth and inheritance - what a house owns at its market's
// prices, the weight of that wealth in its members' standing, and the house
// its goods pass to when it dies out.
//
// STATUS: VALIDATED (Phase 06) - unit/integration/deterministic tests in Tests/Economy
//
// Every year, in every detailed region, a house's goods are valued at the
// region's prices (06.03) and the houses are ranked among themselves; the rank
// is written as HouseWealth, a Society type the standing system observes
// (05.02), so that a rich house lifts its members without standing ever
// knowing what a price is. The same pass names each house's heir by its
// culture's descent custom (05.03): the eldest living child of the head who
// carries the line - sons where descent is patrilineal, daughters where it is
// matrilineal - and who has a house of their own in the region. The stock
// system honours that name when a house dies out (ObserveHeirs): its goods
// pass to the heir instead of returning to the common stock, and a house
// whose line has no heir loses everything to the commons.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Standing.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Economy
{
	struct WealthTypes
	{
		ComponentType<Society::HouseWealth> Wealth; ///< the standing system observes it
		ComponentType<HouseHeir> Heir;				///< the stock system observes it
		static WealthTypes Declare(World& W);
	};

	struct WealthRules
	{
		uint32 HeirFromAge = 12;	 ///< a child younger than this does not inherit
		uint32 RichPerMille = 100;	 ///< top share of a region's houses called rich (for the stats and the chronicle)
		uint32 ChangePerMille = 250; ///< a wealth rank moved by this much is an event
	};

	struct WealthPayload
	{
		uint32 Family = 0;
		uint32 Region = 0;
		uint32 Other = 0; ///< the heir named, or the person whose death named it
		uint32 Value = 0; ///< the house's goods at its market's prices, or its new rank
	};
	/// A house's rank among its region's houses moved (Value = the new rank).
	inline constexpr EventType<WealthPayload> FortuneChangedEvent = MakeEventType<WealthPayload>("FortuneChanged");
	/// A house named a new heir (Other = the heir's house, Value = the heir person).
	inline constexpr EventType<WealthPayload> HeirNamedEvent = MakeEventType<WealthPayload>("HeirNamed");

	/// Yearly, after Markets: value and rank the houses, name their heirs.
	class VAELEN_ECONOMY_API WealthSystem final : public ISystem
	{
	public:
		WealthSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 Population::FamilyTypes InFamilies, EconomyTypes InEconomy, MarketTypes InMarkets,
					 Society::NormTypes InNorms, WealthTypes InWealth, WealthRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Markets(InMarkets), Norms(InNorms), Wealth(InWealth), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Wealth"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Markets"};
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
		Population::FamilyTypes Families;
		EconomyTypes Economy;
		MarketTypes Markets;
		Society::NormTypes Norms;
		WealthTypes Wealth;
		WealthRules Rules;
	};

	/// The wealth of a house (nullptr while its region is coarse, or unknown).
	VAELEN_ECONOMY_API const Society::HouseWealth* WealthOf(const World& W, const Population::FamilyTypes& Families,
															const WealthTypes& Wealth, uint32 Family);
	/// The heir named on a house (nullptr when none).
	VAELEN_ECONOMY_API const HouseHeir* HeirOf(const World& W, const Population::FamilyTypes& Families,
											   const WealthTypes& Wealth, uint32 Family);
	/// The houses of a region by wealth, richest first (then by index).
	VAELEN_ECONOMY_API void RichestOf(const World& W, const Population::FamilyTypes& Families,
									  const WealthTypes& Wealth, uint32 Region, std::vector<uint32>& Out);

	struct WealthStats
	{
		uint32 Valued = 0;	 ///< houses carrying a wealth, region-filtered
		uint32 WithHeir = 0; ///< houses whose heir is named
		uint32 Rich = 0;	 ///< houses in the top share
		uint32 Richest = 0;	 ///< the highest value
		uint64 Value = 0;	 ///< the sum of the houses' values
		uint32 Changes = 0;	 ///< fortune events, from the log
		uint32 HeirsNamed = 0;
		uint32 Inheritances = 0; ///< StockInherited events
		uint32 Stale = 0;  ///< wealth or heir on an extinct house or a coarse region, or an heir naming a dead house
		Hash64 Digest = 0; ///< every wealth then every heir in family order
	};
	VAELEN_ECONOMY_API WealthStats MeasureWealth(const World& W, const Population::FamilyTypes& Families,
												 const WealthTypes& Wealth, uint32 Region);
} // namespace Vaelen::Economy
