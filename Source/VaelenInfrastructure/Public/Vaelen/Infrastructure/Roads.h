// VAELEN - VaelenInfrastructure
// Phase 09.04: roads - a route of 06.04 made into something, kept, and let go.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure
//
// A route of 06.04 is not a road. It is a fact about prices: grain was dearer
// there than here, often enough and long enough that somebody carried it. It
// opens because a gap opened and closes because nothing crossed it, and nobody
// ever built it.
//
// 09.04 lets the regions at its two ends MAKE something of it. A road is cut
// out of their common stocks and the hands their fields can spare, only where
// enough already crosses to be worth the timber - a road is never a wager on
// trade that does not exist yet. Once made it carries more of the trade that
// already wanted to happen, which is the whole of what a road does here: it
// creates nothing.
//
// And it has to be kept. Every year the two ends pay a little timber, or the
// road wears; unkept long enough it loses a grade, and at grade zero it is a
// track again - which is what a route was before anybody touched it. Nothing is
// destroyed and no entity dies: the road falls back to what the economy always
// had, and can be cut again by regions that have the timber.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Sim/Event.h"
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
	/// Component of a route entity (ids of kind Route), beside the RouteInfo of
	/// 06.04 rather than instead of it: trade knows the route exists and why,
	/// and this knows what has been made of it.
	struct RoadInfo
	{
		uint32 Route = 0; ///< the route index of 06.04
		uint32 From = 0;
		uint32 To = 0;
		uint32 Grade = 0;  ///< 0 = a track again, 1..MostGrade = a made road
		uint32 Repair = 0; ///< per mille of the grade still sound
		uint32 Idle = 0;   ///< years in a row nobody has kept it
		uint32 Timber = 0; ///< what it has taken, cutting and keeping, over its life
		uint32 Hands = 0;  ///< man-years
		uint64 Cut = 0;	   ///< tick it was first made
		uint64 Seen = 0;   ///< units the route had carried when it was last looked at
		Hash64 Identity = 0;
	};
	static_assert(sizeof(RoadInfo) == 56, "RoadInfo must stay padding free");

	struct RoadTypes
	{
		ComponentType<RoadInfo> Road;
		ComponentType<Economy::RouteEase> Ease;
		static VAELEN_INFRASTRUCTURE_API RoadTypes Declare(World& W);
	};

	struct RoadRules
	{
		uint32 TrafficPerGrade = 150; ///< units a year across it before a grade is worth cutting
		uint32 MostGrade = 3;		  ///< the best road this world makes
		uint32 TimberPerGrade = 60;	  ///< out of the two ends, half each, to cut a grade
		uint32 HandsPerGrade = 30;	  ///< man-years, which the ends must have to spare
		uint32 EasePerGrade = 200;	  ///< per mille more that crosses a sound road of one grade
		uint32 UpkeepPerGrade = 6;	  ///< timber a year, out of the two ends, to keep it
		uint32 MendPerYear = 250;	  ///< per mille of repair regained in a year it is kept
		uint32 WearPerYear = 80;	  ///< per mille lost in a year it is not
		uint32 IdleBeforeLosing = 3;  ///< years unkept, with nothing left to wear, before a grade goes
		uint32 PeopleToWork = 200;	  ///< an end under this many living gives no hands
		uint32 SparePerMille = 40;	  ///< of an end's people who can leave the fields for a road
	};

	struct RoadPayload
	{
		uint32 Route = 0;
		uint32 From = 0;
		uint32 To = 0;
		uint32 Grade = 0; ///< the grade it now has
	};
	/// A route was made into a road, or an existing road was raised a grade.
	inline constexpr EventType<RoadPayload> RoadCutEvent = MakeEventType<RoadPayload>("RoadCut");
	/// A road nobody kept lost a grade; at grade 0 it is a track again.
	inline constexpr EventType<RoadPayload> RoadLostEvent = MakeEventType<RoadPayload>("RoadLost");

	/// Yearly, after Places: what the ends of every route can afford to make of
	/// it, what they keep, and what they let go.
	class VAELEN_INFRASTRUCTURE_API RoadSystem final : public ISystem
	{
	public:
		RoadSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::FamilyTypes InFamilies,
				   Economy::EconomyTypes InEconomy, Economy::TradeTypes InTrade, RoadTypes InRoads,
				   RoadRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Families(InFamilies), Economy(InEconomy), Trade(InTrade), Roads(InRoads),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Roads"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Places"};
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
		Population::FamilyTypes Families;
		Economy::EconomyTypes Economy;
		Economy::TradeTypes Trade;
		RoadTypes Roads;
		RoadRules Rules;
	};

	/// What has been made of a route (nullptr while it is still only a route).
	VAELEN_INFRASTRUCTURE_API const RoadInfo* RoadOn(const World& W, const RoadTypes& Roads, uint32 Route);
	/// What has been made of the route between two regions (nullptr when there is none).
	VAELEN_INFRASTRUCTURE_API const RoadInfo* RoadBetween(const World& W, const Economy::TradeTypes& Trade,
														  const RoadTypes& Roads, uint32 A, uint32 B);
	/// What a road is worth to whatever crosses it, per mille: the grade by the
	/// rules, cut by how sound it still is. 0 for a track and for no road at all.
	VAELEN_INFRASTRUCTURE_API uint32 RoadWorth(const RoadInfo& Road, const RoadRules& Rules) noexcept;

	struct RoadStats
	{
		uint32 Roads = 0;  ///< routes something has been made of, tracks included
		uint32 Made = 0;   ///< of those, still above a track
		uint32 Tracks = 0; ///< fallen all the way back
		uint32 Grades = 0; ///< summed grade of every road standing
		uint32 Best = 0;   ///< the best road in the world
		uint32 Timber = 0; ///< what all of it has taken
		uint32 Hands = 0;
		uint32 Cuttings = 0; ///< events, from the log
		uint32 Losses = 0;	 ///< events
		uint32 Bad = 0;		 ///< incoherent: see MeasureRoads
		Hash64 Digest = 0;	 ///< every road, in route order
	};

	/// Counts the roads and checks what one must keep: a route under it, the two
	/// ends its route names, a grade within its cap, a repair no better than
	/// sound, one road to a route, and an ease that is exactly what the grade
	/// and the repair are worth.
	VAELEN_INFRASTRUCTURE_API RoadStats MeasureRoads(const World& W, const Economy::TradeTypes& Trade,
													 const RoadTypes& Roads, const RoadRules& Rules);
} // namespace Vaelen::Infrastructure
