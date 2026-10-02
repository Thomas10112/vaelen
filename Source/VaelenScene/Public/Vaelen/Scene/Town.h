// VAELEN - VaelenScene
// Phase 23 task 23.05: a house has a roof that faces its street and walls of
// its family's culture - what the scene adds to the layout, in integers.
//
// THE LAYOUT IS NOT TOUCHED. Its digest is pinned in the gates and the
// replays (6e23ade67f848db7 on the walk of 15.10, 6917612c1344027f on the
// sixty-year world) and a byte added to Placed would move every one of
// them; the roof and the wall are read FROM the layout and the people, by
// a function beside it, digested on their own. Both are invented, as the
// houses are (Layout.h's rule):
//   - the RIDGE of a house runs along the nearest road tile within
//     RoofReachTiles of its own (the road's direction there: the way to its
//     next tile), ties on the lower tile index; a house with no road near
//     it takes an axis hashed from its key - so a street is lined with
//     gables and a lone farm faces whichever way it was built;
//   - the CULTURE of a house is its family's: the culture of the family's
//     lowest-indexed living member in the people view; 0 for a coarse
//     region's house (Flags 1, nobody's family) or a family nobody living
//     is of. The engine tints the walls by it.
//
// STATUS: VALIDATED headless (Phase 23 task 23.05)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Scene/Layout.h"
#include "Vaelen/Scene/SceneApi.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Folk.h"

#include <vector>

namespace Vaelen::Scene
{
	inline constexpr uint32 RoofReachTiles = 2; ///< a road this near sets the ridge

	/// One house's look, 4 bytes, no padding: MeasureTown hashes it.
	struct HouseLook
	{
		uint8 RidgeAlongY = 0; ///< 1 when the ridge runs along Y, 0 along X
		uint8 ByRoad = 0;	   ///< 1 when a road set it, 0 when the hash did
		uint16 Culture = 0;	   ///< the family's, 0 for none
	};
	static_assert(sizeof(HouseLook) == 4, "HouseLook must have no padding: MeasureTown hashes it");

	struct TownLook
	{
		uint32 Day = 0;
		std::vector<HouseLook> Houses; ///< one per L.Houses, in its order
	};

	/// The ridge of one house, 0 along X or 1 along Y, and whether a road set
	/// it. Pure in the ground, the layout's roads and the house.
	VAELEN_SCENE_API uint32 RidgeOf(const Ground& G, const SceneLayout& L, const Placed& House, bool& bByRoad);
	/// The culture of one house, or 0.
	VAELEN_SCENE_API uint32 CultureOfHouse(const View::PeopleView& People, const Placed& House);
	/// Both, for every house of the layout.
	VAELEN_SCENE_API void LookOfTown(const Ground& G, const SceneLayout& L, const View::PeopleView& People,
									 TownLook& Out);

	struct TownStats
	{
		uint32 Houses = 0;
		uint32 AlongY = 0;	 ///< ridges along Y
		uint32 ByRoad = 0;	 ///< ridges a road set
		uint32 Cultured = 0; ///< houses of a culture
		uint32 Cultures = 0; ///< distinct cultures among them
		uint32 Reserved = 0;
		Hash64 Digest = 0;
	};
	VAELEN_SCENE_API TownStats MeasureTown(const TownLook& T);

	/// `LogVaelenScene: AELVOR <size> seed <12 hex> town day D: houses H, ridges
	/// along y Y (by a road R), of a culture C (K cultures); town <16 hex>`.
	inline constexpr uint32 TownLineBytes = 256;
	VAELEN_SCENE_API uint32 TownLine(uint32 Size, uint64 Seed, uint32 Day, const TownStats& S, char* Out, uint32 Bytes);
} // namespace Vaelen::Scene
