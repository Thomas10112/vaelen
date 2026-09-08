// VAELEN - VaelenMilitary
// Phase 08.02: marching - where a host goes, how long it takes to get there,
// and what it eats off the ground it crosses.
//
// STATUS: PROTOTYPE (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military
//
// A host raised at a seat is of no use at the seat. It marches, and it marches
// on the region graph the world was partitioned into (02.05), one hop a
// season: four hops a year, no faster, whatever the distance. Its aim is the
// nearest ground held by somebody its polity is at war with - nearest by hops,
// not by miles, because the graph is what an army can actually walk.
//
// Marching is not free and it is not paid for by the treasury. A host eats off
// the ground it stands on, and the ground it stands on is a region with a
// stock (06.01). On its own ground that is a cost its people bear; on enemy
// ground it is the point - an enemy host standing on a region loosens its
// ruler's grip on it, and 07.03 already knows what a loosened grip leads to.
// Nothing here takes a region: taking is 08.04's business. This only puts a
// host where taking becomes possible.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Military
{
	/// Component on an army entity: the order it is under. An army with no
	/// order in reach keeps Aim 0 and stands where it is.
	struct MarchOrder
	{
		uint32 Aim = 0;		///< region it is marching on, 0 = nothing in reach
		uint32 Hops = 0;	///< hops still between where it stands and the aim
		uint32 Walked = 0;	///< hops walked since it was raised
		uint32 Arrived = 0; ///< 1 while it stands on its aim
	};
	static_assert(sizeof(MarchOrder) == 16, "MarchOrder must stay padding free");

	/// Component on a region entity: what hosts have taken off it. Written by
	/// the march, read by anyone who wants to know why a region is hungry.
	struct RegionForage
	{
		uint32 Taken = 0; ///< grain taken off it last year
		uint32 Years = 0; ///< consecutive years a host has stood on it
	};
	static_assert(sizeof(RegionForage) == 8, "RegionForage must stay padding free");

	struct MarchTypes
	{
		ComponentType<MarchOrder> Order;
		ComponentType<RegionForage> Forage;
		static VAELEN_MILITARY_API MarchTypes Declare(World& W);
	};

	struct MarchRules
	{
		uint32 HopsPerYear = 4;			 ///< one hop a season
		uint32 AimWithin = 12;			 ///< hops; nothing further away is marched on
		uint32 ForagePerManPerYear = 2;	 ///< grain a host takes off the ground it stands on
		uint32 HostileHoldPerMille = 60; ///< hold an enemy host standing on a region costs its ruler each year
	};

	/// A host moved (Polity, the region it now stands in, the army, hops walked).
	inline constexpr EventType<Politics::PolityPayload> ArmyMarchedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyMarched");
	/// A host took grain off the ground (Polity, the region, the army, grain taken).
	inline constexpr EventType<Politics::PolityPayload> ArmyForagedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyForaged");
	/// A host reached what it was marching on (Polity, the region, the army, hops walked in all).
	inline constexpr EventType<Politics::PolityPayload> ArmyArrivedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyArrived");

	/// Yearly, after Armies: give every standing host an aim, walk it up to
	/// HopsPerYear hops towards that aim, feed it off the region it ends the
	/// year in, and loosen the grip of any ruler it is standing on.
	class VAELEN_MILITARY_API MarchSystem final : public ISystem
	{
	public:
		MarchSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::EconomyTypes InEconomy,
					Politics::PolityTypes InPolities, Politics::ReachTypes InReaches,
					Politics::DiplomacyTypes InRelations, ArmyTypes InArmies, MarchTypes InMarches,
					MarchRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Economy(InEconomy), Polities(InPolities), Reaches(InReaches),
			  Relations(InRelations), Armies(InArmies), Marches(InMarches), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Marching"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Armies"};
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
		Economy::EconomyTypes Economy;
		Politics::PolityTypes Polities;
		Politics::ReachTypes Reaches;
		Politics::DiplomacyTypes Relations;
		ArmyTypes Armies;
		MarchTypes Marches;
		MarchRules Rules;
		WorldGen::RegionGraph Graph; ///< cache, rebuilt when the world's regions change
		uint32 GraphRegions = 0;
	};

	/// The order an army is under (nullptr when it has never had one).
	VAELEN_MILITARY_API const MarchOrder* OrderOf(const World& W, const ArmyTypes& Armies, const MarchTypes& Marches,
												  uint32 Army);
	/// What a region has been eaten out of (nullptr when no host has stood on it).
	VAELEN_MILITARY_API const RegionForage* ForageOf(const World& W, const History::PreHistoryTypes& Types,
													 const MarchTypes& Marches, uint32 Region);

	struct MarchStats
	{
		uint32 Marching = 0; ///< standing hosts under an order
		uint32 Arrived = 0;	 ///< standing hosts on their aim
		uint32 Idle = 0;	 ///< standing hosts with nothing in reach to march on
		uint32 Abroad = 0;	 ///< standing hosts on ground their polity does not hold
		uint64 Walked = 0;	 ///< hops walked, over every host that ever marched
		uint32 Foraged = 0;	 ///< regions a host stood on last year
		uint64 Taken = 0;	 ///< grain taken off the land last year
		uint32 Marches = 0;	 ///< events, from the log
		uint32 Forages = 0;
		uint32 Arrivals = 0;
		uint32 Bad = 0;	   ///< an order left on a host that went home, a host abroad under no order,
						   ///< an aim that is not enemy ground, an arrival that is not where it stands
		Hash64 Digest = 0; ///< every order in army index order, then every forage in region order
	};
	VAELEN_MILITARY_API MarchStats MeasureMarches(const World& W, const History::PreHistoryTypes& Types,
												  const Politics::PolityTypes& Polities,
												  const Politics::DiplomacyTypes& Relations, const ArmyTypes& Armies,
												  const MarchTypes& Marches, const MarchRules& Rules);
} // namespace Vaelen::Military
