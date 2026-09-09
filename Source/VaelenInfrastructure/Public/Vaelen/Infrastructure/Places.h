// VAELEN - VaelenInfrastructure
// Phase 09.03: settlements as places - a thing standing on a tile of the map,
// with a size that grows with the people around it and the goods through it.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure
//
// 06.04 already founds settlements, and they are not places. A settlement of
// 06.04 is a fact about trade: goods changed hands here often enough and long
// enough that somebody stayed. It has a region and a traffic and no position,
// no size and no streets - which is right for what it was for, and not enough
// to build in.
//
// 09.03 gives it a body. A place stands on ONE TILE of its region, chosen once
// and never moved: on water where the region has water, at its heart where it
// has none, because towns grow where the boats and the mills are. It is the
// first thing on this map smaller than a region, which is what every later
// phase needs - 09.04 to run a road to it, 13 to draw it, and a player to stand
// in it.
//
// It has a size, from the people who live in it rather than on the land around
// it and from the goods carried through it, capped, so that a town of eight is
// the biggest thing this world makes. And it holds the works of 09.01: a
// granary stands IN the town where the region has one, and in the countryside
// where it has not.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Infrastructure/Buildings.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
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
	/// Component of a settlement entity (ids of kind Settlement), beside the
	/// SettlementInfo of 06.04 rather than instead of it: trade knows why the
	/// place is there, and this knows where and how big.
	struct PlaceInfo
	{
		uint32 Settlement = 0; ///< the settlement index of 06.04
		uint32 Region = 0;
		uint32 Tile = 0;   ///< the tile it stands on, chosen once and never moved
		uint32 Size = 0;   ///< 1..MostSize while it stands, 0 once it is abandoned
		uint32 People = 0; ///< living in it rather than on the land around it
		uint32 Works = 0;  ///< buildings of 09.01 standing in it
		uint32 Grown = 0;  ///< the largest size it ever reached, which a ruin keeps
		uint32 Reserved = 0;
		uint64 Settled = 0;	 ///< tick it became a place
		Hash64 Identity = 0; ///< from the world seed, for names
	};
	static_assert(sizeof(PlaceInfo) == 48, "PlaceInfo must stay padding free");

	/// Component of a building entity: where in its region it actually stands.
	/// A building without one stands in the countryside, which is also what a
	/// region with no town gets.
	struct BuildingPlace
	{
		uint32 Settlement = 0; ///< 0 = in the countryside
		uint32 Tile = 0;
	};
	static_assert(sizeof(BuildingPlace) == 8, "BuildingPlace must stay padding free");

	struct PlaceTypes
	{
		ComponentType<PlaceInfo> Place;
		ComponentType<BuildingPlace> At;
		static VAELEN_INFRASTRUCTURE_API PlaceTypes Declare(World& W);
	};

	struct PlaceRules
	{
		uint32 TownSharePerMille = 150; ///< of a region's people who live in its town
		uint32 PeoplePerSize = 50;		///< a size for this many townsfolk
		uint32 TrafficPerSize = 100;	///< and a size for this much carried through it
		uint32 MostSize = 8;			///< the biggest thing this world makes
		uint32 WorksInTown = 1;			///< a region's works stand in its town where it has one
	};

	struct PlacePayload
	{
		uint32 Settlement = 0;
		uint32 Region = 0;
		uint32 Tile = 0;
		uint32 Size = 0;
	};
	/// A settlement of 06.04 became a place on the map (Size = 1).
	inline constexpr EventType<PlacePayload> PlaceSettledEvent = MakeEventType<PlacePayload>("PlaceSettled");
	/// A place grew, or shrank (Size = what it is now).
	inline constexpr EventType<PlacePayload> PlaceGrewEvent = MakeEventType<PlacePayload>("PlaceGrew");
	/// A place emptied, because the settlement under it was abandoned.
	inline constexpr EventType<PlacePayload> PlaceEmptiedEvent = MakeEventType<PlacePayload>("PlaceEmptied");

	/// Yearly, after Works: the body of every settlement, and where every
	/// building of the region actually stands.
	class VAELEN_INFRASTRUCTURE_API PlaceSystem final : public ISystem
	{
	public:
		PlaceSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::TradeTypes InTrade,
					InfrastructureTypes InBuildings, PlaceTypes InPlaces, PlaceRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Trade(InTrade), Buildings(InBuildings), Places(InPlaces), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Places"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Works"};
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
		InfrastructureTypes Buildings;
		PlaceTypes Places;
		PlaceRules Rules;
	};

	/// The place of a settlement (nullptr before it has a body, or for an unknown one).
	VAELEN_INFRASTRUCTURE_API const PlaceInfo* PlaceOf(const World& W, const PlaceTypes& Places, uint32 Settlement);
	/// The place standing in a region, if it has one.
	VAELEN_INFRASTRUCTURE_API const PlaceInfo* PlaceIn(const World& W, const PlaceTypes& Places, uint32 Region);
	/// Where a building stands (nullptr before anything placed it).
	VAELEN_INFRASTRUCTURE_API const BuildingPlace* PlaceOfBuilding(const World& W, const InfrastructureTypes& Buildings,
																   const PlaceTypes& Places, uint32 Building);

	struct PlaceStats
	{
		uint32 Places = 0;	  ///< settlements with a body, standing or emptied
		uint32 Standing = 0;  ///< of those, still lived in
		uint32 Emptied = 0;	  ///< abandoned under their own feet
		uint32 Townsfolk = 0; ///< people living in towns rather than on the land
		uint32 Works = 0;	  ///< buildings standing inside a town
		uint32 Loose = 0;	  ///< buildings standing in the countryside
		uint32 Largest = 0;	  ///< the biggest place in the world
		uint32 Bad = 0;		  ///< incoherent: see MeasurePlaces
		Hash64 Digest = 0;	  ///< every place, in settlement order
	};

	/// Counts the places and checks what a place must keep: a settlement that
	/// exists, a tile inside its own region, one place to a tile, a size within
	/// its cap and zero exactly when the settlement is abandoned, a Grown that
	/// never falls, and a count of works that matches the buildings placed there.
	VAELEN_INFRASTRUCTURE_API PlaceStats MeasurePlaces(const World& W, const History::PreHistoryTypes& Types,
													   const Economy::TradeTypes& Trade,
													   const InfrastructureTypes& Buildings, const PlaceTypes& Places,
													   const PlaceRules& Rules);
} // namespace Vaelen::Infrastructure
