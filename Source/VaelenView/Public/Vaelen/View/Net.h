// VAELEN - VaelenView
// Phase 13 task 13.08a: what the world has BUILT, as a renderer needs it.
//
// 13.01 gave the view regions; 13.07a gave it the ground. Between the two, a
// renderer can draw where the land is and how many live on it, and still cannot
// draw a single road - because `RegionView::Roads` is a COUNT. It says a region
// is touched by three routes and not which three, so a map drawn from it has
// fifty-two towns on it and no lines between them, and the whole economy of the
// world is invisible.
//
// This is that, plus the colony - which is not a detail. VAELEN opens with the
// player owned, inside a mining colony, and a view that cannot say where the
// colony is cannot draw the one place the game begins.
//
// WHY THIS IS NOT IN `WorldView`, and the reason is a trap rather than taste.
// 13.02's `Delta` diffs a `WorldView` REGION BY REGION. A vector of routes
// added to that struct would be carried by the view, ignored by the diff, and
// silently absent from every screen rebuilt from a delta - correct on the first
// frame and stale forever after. A structure the diff does not know about must
// not live inside the thing the diff claims to describe.
//
// Same promise as 13.01 and 13.07a: flat numbers, no pointer, no handle, no way
// back. A `NetView` outlives the world it was taken from.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Net.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/ViewApi.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::View
{
	/// One route, as something drawing a line needs it. `From` is always the
	/// lower region index and `To` the higher, the way the kernel keeps them,
	/// so a renderer can key a line on the pair without ordering it first.
	struct RouteView
	{
		uint64 Carried = 0;	 ///< units moved over its whole life, both ways
		uint32 Index = 0;	 ///< 1-based, in order of opening
		uint32 From = 0;	 ///< region index, the lower
		uint32 To = 0;		 ///< region index, the higher
		uint32 Idle = 0;	 ///< years in a row carrying nothing
		uint32 Openings = 0; ///< times this road has been opened
		uint32 Open = 0;	 ///< 1 while open, 0 once closed
	};
	static_assert(sizeof(RouteView) == sizeof(uint64) + 6 * sizeof(uint32),
				  "RouteView must have no padding: MeasureNetView hashes it");

	/// One colony. Sixteen bytes, the same shape the kernel keeps it in.
	struct ColonyView
	{
		uint32 Region = 0;	 ///< the region it is
		uint32 Hands = 0;	 ///< people on the rock, recounted every tick
		uint32 Lifted = 0;	 ///< units of ore lifted over its life
		uint32 Reserved = 0; ///< keeps the count of 32-bit fields even
	};
	static_assert(sizeof(ColonyView) == 4 * sizeof(uint32),
				  "ColonyView must have no padding: MeasureNetView hashes it");

	/// What the world has built, for one frame. Routes in index order and
	/// colonies in region order, always: a renderer keeps its own array in step
	/// with this one, which it cannot do if the order rides on pool order.
	struct NetView
	{
		uint64 Tick = 0;
		uint32 Year = 0;
		uint32 Open = 0; ///< of the routes below, how many are open
		std::vector<RouteView> Routes;
		std::vector<ColonyView> Colonies;
	};

	/// Takes the network. Const world in, numbers out. Out is left empty when
	/// the sources carry neither trade nor a colony - a world with no economy
	/// has no roads, and says so rather than refusing.
	VAELEN_VIEW_API void TakeNetView(const World& W, const ViewSources& From, NetView& Out);

	/// The route between two regions, in either order, or nullptr for none.
	///
	/// A PAIR DOES NOT IDENTIFY A ROUTE. `RouteView::Index` does. The world can
	/// hold more than one route entity on the same pair - on AELVOR at 96, 96 of
	/// 254 of them - because 06.04 rebuilds a road it closed earlier in the same
	/// tick instead of reopening it (ADR-0120). So this returns the OPEN route
	/// when there is one, which is what anything drawing a live road wants, and
	/// the lowest-indexed otherwise. Anything that needs a specific route holds
	/// its Index.
	VAELEN_VIEW_API const RouteView* RouteBetween(const NetView& V, uint32 A, uint32 B);

	/// The route with this index, or nullptr. This is the unambiguous lookup.
	VAELEN_VIEW_API const RouteView* RouteOf(const NetView& V, uint32 Index);

	/// The colony on a region, or nullptr when that region is not one.
	VAELEN_VIEW_API const ColonyView* ColonyIn(const NetView& V, uint32 Region);

	struct NetStats
	{
		uint32 Routes = 0;	 ///< in the view, open and closed
		uint32 Open = 0;	 ///< of them, open
		uint32 Colonies = 0; //
		uint32 Hands = 0;	 ///< people on the rock across all of them
		uint32 Bytes = 0;	 ///< what the network weighs
		uint32 Reserved = 0; //
		Hash64 Digest = 0;	 ///< every route then every colony, in order
	};
	VAELEN_VIEW_API NetStats MeasureNetView(const NetView& V);
} // namespace Vaelen::View
