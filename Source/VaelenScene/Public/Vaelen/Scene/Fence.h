// VAELEN - VaelenScene
// Phase 19 task 19.07: where the walker may stand, and when it has left (ADR-0155).
//
// The walker is the host's: a capsule in floating point, moved on the engine's
// frame. The PERSON is the world's, and the world knows a region and nothing
// smaller. If the capsule could stand in region B while the life says A, the
// picture would be a second source of truth. So the walker is FENCED to the
// walkable tiles of the region the life names, crosses only by the verb Move
// towards a region the life lists as Near, and is put back on a deterministic
// arrival point after every day turn. Only integers reach the door: the
// intent's target, the region looked at, the day turn.
//
// Everything here is a function of the Ground (Terrain.h) and the life view.
//
// STATUS: VALIDATED headless (Phase 19 task 19.07)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Scene/SceneApi.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Life.h"

#include <vector>

namespace Vaelen::Scene
{
	/// A tile the walker may stand on in Region: the region's own, land or a
	/// river's bank - not the sea, and not under a lake.
	VAELEN_SCENE_API bool IsWalkable(const Ground& G, uint32 Tile, uint32 Region);

	/// The region under a point, 0 off the map or on a tile of no region.
	VAELEN_SCENE_API uint32 RegionAt(const Ground& G, int64 Xcm, int64 Ycm);
	/// The point is on a walkable tile of Region.
	VAELEN_SCENE_API bool Inside(const Ground& G, uint32 Region, int64 Xcm, int64 Ycm);

	/// One unit edge of the fence: the side between a walkable tile of the
	/// region and a tile that is not (or the map's edge), in scene centimetres.
	/// The edges of a region close into loops: every corner they meet at is met
	/// an even number of times (Scene.Fence holds it).
	struct FenceEdge
	{
		int32 X0 = 0;
		int32 Y0 = 0;
		int32 X1 = 0;
		int32 Y1 = 0;
		uint32 Inside = 0;	///< the walkable tile
		uint32 Outside = 0; ///< the other tile, NoTile off the map
	};
	inline constexpr uint32 NoTile = 0xFFFFFFFFu;
	VAELEN_SCENE_API void BuildFence(const Ground& G, uint32 Region, std::vector<FenceEdge>& Out);

	namespace CrossingWhy
	{
		inline constexpr uint32 Crossing = 0; ///< a region the life lists as Near
		inline constexpr uint32 Home = 1;	  ///< ahead is still the life's own region
		inline constexpr uint32 OffMap = 2;	  ///< ahead is off the map
		inline constexpr uint32 Water = 3;	  ///< ahead is sea or lake: no region to walk into
		inline constexpr uint32 NotNear = 4;  ///< another region, not one a Move would be taken to
	} // namespace CrossingWhy

	struct Crossing
	{
		uint32 Region = 0; ///< the region a Move would name, 0 unless Why is Crossing
		uint32 Why = CrossingWhy::Home;
	};
	/// What pressing M means with the walker facing the point Ahead: a region
	/// only when it is in Life.Near (adjacent AND detailed, Doings.cpp) - any
	/// other answer is the page's to give, and nothing is sent to the world.
	VAELEN_SCENE_API Crossing CrossingOf(const Ground& G, const View::LifeView& Life, int64 AheadX, int64 AheadY);

	/// The walkable tile of To nearest the point (between centres, ties by the
	/// lower index). False when To has no walkable tile.
	VAELEN_SCENE_API bool Arrival(const Ground& G, uint32 To, int64 Xcm, int64 Ycm, uint32& Tile);

	/// After a day turn: a walker still on a walkable tile of the region the
	/// life now names stays where it is; otherwise it is put on the centre of
	/// Arrival(After, where it stood). True when it was put back.
	VAELEN_SCENE_API bool PlaceAfterDay(const Ground& G, uint32 After, int64& Xcm, int64& Ycm);
} // namespace Vaelen::Scene
