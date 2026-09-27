// VAELEN - VaelenScene
// Phase 19 task 19.08: towns, houses, roads and figures - what the scene
// invents, from what the views hold (ADR-0156).
//
// THE SIMULATION HOLDS NO HOUSE AND NO POSITION BELOW A REGION. Infrastructure's
// works are wired in no played world, the only buildings it knows are granaries,
// mills, smithies and walls (Buildings.h), and a person has a region and nothing
// smaller (Folk.h). Everything below is INVENTED, and said so here in capitals
// because an invented thing quietly becomes a fact otherwise
// (VaelenViewDrawer.h's rule for where a person stands):
//   - a SQUARE where a region has a settlement, near its centroid;
//   - one HOUSE per living family of a detailed region, a ceil(people / 5)
//     of them in a region the world keeps coarse, each plotted from a hash of
//     its family and trying again away from water, other regions, slopes over
//     20 deg and the houses of LOWER-indexed families - so a house never moves
//     when a later family appears or dies out;
//   - a ROAD along each open route, found tile by tile between the two
//     regions' centres (integer A*, ties on the tile index);
//   - a PIT where a colony works the rock;
//   - a FIGURE for every living person of a detailed region but the played
//     one, at a slot hashed from who they are and the DAY - once a day, never
//     on a frame.
// Each is a pure function of the views and the day: the same on every machine,
// stable under births and deaths, and nothing of it ever reaches the world.
//
// STATUS: VALIDATED headless (Phase 19 task 19.08)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Scene/SceneApi.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Folk.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Net.h"

#include <vector>

namespace Vaelen::Scene
{
	inline constexpr int32 HouseHalfCm = 400;  ///< a house is 8 m square
	inline constexpr int32 HouseSlopeCm = 291; ///< across its 8 m: tan 20 deg
	inline constexpr uint32 HouseTries = 64;   ///< plots tried before a house is given up
	inline constexpr int32 AimReachCm = 300;   ///< how near a figure must be to be spoken to

	struct Placed
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;
		uint32 Region = 0;
		uint32 Key = 0;	  ///< the family, the settlement, the person, the colony's region
		uint32 Flags = 0; ///< FigureFlag for a figure, 1 for a coarse region's house
	};

	namespace FigureFlag
	{
		inline constexpr uint32 Company = 1u << 0; ///< listed in the played life's Company
	} // namespace FigureFlag

	struct RoadPath
	{
		uint32 Route = 0;		   ///< RouteView::Index
		std::vector<uint32> Tiles; ///< from the lower region's centre to the higher's
	};

	struct SceneLayout
	{
		uint32 Day = 0;
		std::vector<Placed> Squares;
		std::vector<Placed> Houses;
		std::vector<Placed> Figures;
		std::vector<Placed> Pits;
		std::vector<RoadPath> Roads;
		uint32 Unplaced = 0; ///< houses no plot was found for in HouseTries
		uint32 Unrouted = 0; ///< open routes no land path joins
	};

	/// Lays the scene out. Day is the life's day of the year (or any day index
	/// the host keeps): figures move once per value of it and never otherwise.
	VAELEN_SCENE_API void BuildLayout(const Ground& G, const View::WorldView& World_, const View::NetView& Net,
									  const View::PeopleView& People, const View::LifeView& Life, uint32 Day,
									  SceneLayout& Out);

	/// The figure of the played life's company nearest the point, within
	/// AimReachCm and 30 deg either side of the direction (DirX, DirY); ties go
	/// to the lower person index. 0 when there is none: Tab is the fallback.
	VAELEN_SCENE_API uint32 AimAt(const SceneLayout& L, int64 Xcm, int64 Ycm, int64 DirX, int64 DirY);

	struct LayoutStats
	{
		uint32 Squares = 0;
		uint32 Houses = 0;
		uint32 Unplaced = 0;
		uint32 Figures = 0;
		uint32 Company = 0;
		uint32 Roads = 0;
		uint32 RoadTiles = 0;
		uint32 Unrouted = 0;
		uint32 Pits = 0;
		uint32 Reserved = 0;
		Hash64 Digest = 0;
	};
	VAELEN_SCENE_API LayoutStats MeasureLayout(const SceneLayout& L);

	/// `LogVaelenScene: AELVOR <size> seed <12 hex> layout day D: squares S, houses H
	/// (unplaced U), figures F (company C), roads R over T tiles (unrouted N), pits P;
	/// layout <16 hex>` - composed once, all or nothing.
	inline constexpr uint32 LayoutLineBytes = 320;
	VAELEN_SCENE_API uint32 LayoutLine(uint32 Size, uint64 Seed, uint32 Day, const LayoutStats& S, char* Out,
									   uint32 Bytes);
} // namespace Vaelen::Scene
