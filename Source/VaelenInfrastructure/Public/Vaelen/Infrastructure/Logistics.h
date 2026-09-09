// VAELEN - VaelenInfrastructure
// Phase 09.06: logistics - what a road is worth to an army and to a polity.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure
//
// 09.04 made roads and gave them one job: more of the trade that already wanted
// to happen gets across. That is the economy's side of a road, and it is not
// what a road was ever mainly for. An army marches on it. A tax collector,
// a courier and a garrison go up it.
//
// So this task turns the roads touching a region into ONE number on that
// region - Politics::RegionWays::EasePerMille - and hands it to the two systems
// that already walk the ground hop by hop:
//
//   08.02 marching -> a host gets further along it in a year, and eats less
//                     beside it, because what it needs comes up the way rather
//                     than off the field it is standing in
//   07.03 reach    -> the word carries further before the hold falls away, and
//                     the upkeep of carrying it that far costs less
//
// Neither system learned what a road is. Both read a component they were told
// to observe, and a region with no road on it reads a factor of one, to the
// unit - which is why every frozen digest of Phases 07 and 08 came through this
// task unchanged.
//
// A region is served by its BEST road, not by the sum of them. Four tracks
// meeting at a village do not make a highway, and the alternative - adding them
// up - would make the busiest crossroads unconquerable by arithmetic rather
// than by anything anybody did.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Infrastructure
{
	struct LogisticsTypes
	{
		ComponentType<Politics::RegionWays> Ways;
		static VAELEN_INFRASTRUCTURE_API LogisticsTypes Declare(World& W);
	};

	struct LogisticsRules
	{
		uint32 SharePerMille = 1000; ///< of what the best road touching a region is worth to trade
		uint32 MostEase = 900;		 ///< and no ground is ever easier than this
	};

	/// Yearly, after Roads: the one number the marching of 08.02 and the reach
	/// of 07.03 read. Writes nothing else and publishes nothing - the roads are
	/// the event, this is their arithmetic.
	class VAELEN_INFRASTRUCTURE_API LogisticsSystem final : public ISystem
	{
	public:
		LogisticsSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::TradeTypes InTrade,
						RoadTypes InRoads, LogisticsTypes InWays, RoadRules InRoadRules,
						LogisticsRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Trade(InTrade), Roads(InRoads), Ways(InWays), RoadRules_(InRoadRules),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Logistics"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Roads"};
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
		Economy::TradeTypes Trade;
		RoadTypes Roads;
		LogisticsTypes Ways;
		RoadRules RoadRules_;
		LogisticsRules Rules;
	};

	/// How well a region is served by made roads, per mille (0 for bare ground).
	VAELEN_INFRASTRUCTURE_API uint32 EaseOf(const World& W, const History::PreHistoryTypes& Types,
											const LogisticsTypes& Ways, uint32 Region);

	struct LogisticsStats
	{
		uint32 Served = 0; ///< regions a made road reaches
		uint32 Best = 0;   ///< the easiest ground in the world, per mille
		uint32 Total = 0;  ///< summed ease, for a sense of scale
		uint32 Bad = 0;	   ///< incoherent: past the cap, or not what the roads say
		Hash64 Digest = 0; ///< every written number, in region order
	};

	/// Counts the served ground and checks that every number written is the best
	/// road touching that region, by the rules, within its cap.
	VAELEN_INFRASTRUCTURE_API LogisticsStats MeasureLogistics(const World& W, const History::PreHistoryTypes& Types,
															  const Economy::TradeTypes& Trade, const RoadTypes& Roads,
															  const LogisticsTypes& Ways, const RoadRules& RoadRules_,
															  const LogisticsRules& Rules);
} // namespace Vaelen::Infrastructure
