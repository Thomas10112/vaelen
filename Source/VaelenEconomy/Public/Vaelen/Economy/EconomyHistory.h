// VAELEN - VaelenEconomy
// Phase 06.07: the economy in the chronicle - roads opened and closed, towns
// risen and abandoned, prices that reached their bounds, harvests a region
// could not eat, fortunes made and lost, inheritances.
//
// STATUS: VALIDATED (Phase 06) - integration/text/deterministic tests in Tests/Economy
//
// The economy's events reach the chronicle the way the society's did (05.07):
// a listener turns what matters into RecordInfo documents, bounded per region
// and year, and DescribeEconomyEvent gives each one a line in the words of the
// world. What matters is deliberately narrow - a price is recorded only when
// it reaches its floor or its ceiling, since a market that merely moves is not
// history - so that a century of a world adds tens of records rather than
// thousands. The why of a dear loaf runs from the price to the harvest to the
// drought, three layers down, through the causes each system already wrote.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Society/SocietyHistory.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Economy
{
	struct EconomyChronicleRules
	{
		uint32 RecordRoutes = 1;		///< RouteOpened, RouteClosed
		uint32 RecordSettlements = 1;	///< SettlementFounded, SettlementAbandoned
		uint32 RecordExtremePrices = 1; ///< PriceChanged at the floor or the ceiling
		uint32 RecordShortfalls = 1;	///< Shortfall
		uint32 RecordFortunes = 1;		///< FortuneChanged past the swing below
		uint32 RecordInheritances = 1;	///< StockInherited
		uint32 FortuneSwing = 128;		///< a rank must move at least this much to be history
		uint32 RoadTraffic = 100;		///< units a road must have carried before its closing is history
		uint32 ShortfallFloor = 20;		///< units a region must be short of before it is history
		uint32 MaxRecordsPerYear = 16;	///< per region
	};

	/// Singleton component: the listener's tallies.
	struct EconomyChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 Region = 0;
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(EconomyChronicleState) == 24, "EconomyChronicleState must stay padding free");

	struct EconomyChronicleTypes
	{
		ComponentType<EconomyChronicleState> State;
		static VAELEN_ECONOMY_API EconomyChronicleTypes Declare(World& W);
	};

	/// Everything the economy text needs to name things.
	struct EconomyContext
	{
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		TradeTypes Trade;
		MarketTypes Markets;
		MarketRules Prices;
		/// Optional: with it, an event of the society layer gets its society line
		/// rather than the plainer person one. The describer of the topmost layer
		/// speaks for every layer under it, or the chronicle of a whole world
		/// would lose the words of its middle.
		const Society::SocietyContext* Society = nullptr;
	};

	/// Listener: the economy events that matter become chronicle records.
	class VAELEN_ECONOMY_API EconomyChronicle final : public IEventListener
	{
	public:
		EconomyChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, EconomyContext InContext,
						 EconomyChronicleTypes InState, EconomyChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Context(InContext), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "EconomyChronicle"; }
		void OnEvent(const Event& E) override;
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Region) const;
		World* Owner;
		History::PreHistoryTypes Types;
		EconomyContext Context;
		EconomyChronicleTypes State;
		EconomyChronicleRules Rules;
	};

	/// "the road from Edavaken to Ekum", or "road 7" when it is gone.
	VAELEN_ECONOMY_API void NameRoute(const World& W, const History::PreHistoryTypes& Types,
									  const EconomyContext& Context, uint32 Route, std::string& Out);
	/// "the town of Edavaken", or "town 3".
	VAELEN_ECONOMY_API void NameSettlement(const World& W, const History::PreHistoryTypes& Types,
										   const EconomyContext& Context, uint32 Settlement, std::string& Out);
	/// One line for any event: the economy events get their own sentence, every
	/// other event goes through the person text.
	VAELEN_ECONOMY_API void DescribeEconomyEvent(const World& W, const History::PreHistoryTypes& Types,
												 const EconomyContext& Context, const Event& E, std::string& Out,
												 const Population::PersonIndex* Index = nullptr);
	/// The whole chronicle as text, in tick order, one line each.
	VAELEN_ECONOMY_API uint32 ExportChronicleWithEconomy(const World& W, const History::PreHistoryTypes& Types,
														 const EconomyContext& Context, std::string& Out,
														 uint32 MaxLines = 0);
	/// The why of an event id as text: the event, then "because ..." lines to the root.
	VAELEN_ECONOMY_API uint32 ExportWhyWithEconomy(const World& W, const History::PreHistoryTypes& Types,
												   const EconomyContext& Context, PersistentId Id, std::string& Out);

	struct EconomyChronicleStats
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Described = 0; ///< records whose event has an economy line
		uint32 WithRegion = 0;
		uint32 EraConsistent = 0;
		uint32 ByType[6] = {}; ///< road, town, price, shortfall, fortune, inheritance
	};
	VAELEN_ECONOMY_API EconomyChronicleStats CheckEconomyChronicle(const World& W,
																   const History::PreHistoryTypes& Types,
																   const EconomyContext& Context,
																   const EconomyChronicleTypes& State);
} // namespace Vaelen::Economy
