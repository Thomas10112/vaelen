// VAELEN - VaelenPresentation. Phase 13 task 13.07c.
//
// The world drawn from the view, and from nothing else.
//
// STATUS: PROTOTYPE - compiled and linked by UnrealBuildTool on UE 5.6.1 with
// MSVC 14.44 on 2026-09-10 (14 modules, 167 actions, Result: Succeeded), and
// NOT YET RUN. Nothing here has been dropped in a level or looked at, so every
// claim about what it DRAWS remains unmeasured. What compiling proves is only
// that it is the shape of a program.
//
// The claim about what this file CANNOT REACH is checked by the compiler, and
// that is the point of it - see below.
//
// READ THE SIGNATURES. Not one function here takes a World, a TickContext, an
// EntityHandle, a ComponentType, or anything else that could lead back into the
// simulation. They take three flat structs of numbers - MapView, WorldView,
// NetView - and a settings block, and they fill instanced meshes.
//
// VaelenViewDrawer.cpp includes Vaelen/View/*.h and the engine, and NOTHING
// from VaelenSim, VaelenPopulation, VaelenEconomy or VaelenPolitics. That is
// not a promise in a comment; it is a compile error waiting for anyone who
// tries. 13.01 said a view "holds no pointer, no handle, and no reference to
// the World it came from, so a renderer that has one cannot reach the
// simulation even by mistake - there is nothing in its hand to reach with".
// This is the file that has nothing in its hand.
//
// The headless half of this claim is already tested: Tests/View/Test_Land.cpp
// has a suite that takes a view, destroys the world, and reads the view
// afterwards. AVaelenViewActor does the same thing inside Unreal, and it does
// it in that order deliberately - the world is gone before a single instance is
// placed. See VaelenViewActor.h.
#pragma once

#include "CoreMinimal.h"

#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Net.h"

class UHierarchicalInstancedStaticMeshComponent;

/// How big to draw it. Nothing here is about the simulation; it is all about
/// centimetres, which is the correct division of labour.
struct FVaelenDrawSettings
{
	/// Side of one tile's slab, in centimetres.
	float TileSize = 100.0f;
	/// How tall the highest land stands, as a fraction of the map's width.
	///
	/// NOT centimetres per elevation unit, which is what this was and why the
	/// land was never once visible: TileView::Elevation is a Fix64 raw shifted
	/// down 16, so AELVOR's land runs to about 2000 of them and its median sits
	/// near 640. At the old 700 cm per unit a middling field stood 4.5 km above
	/// a map 128 m across and the mountains reached 14 km. Every land tile was
	/// launched clean out of frame, and what looked like a continent in the
	/// viewport was the HOLE in the sea where the land should have been.
	///
	/// A fraction cannot do that. The drawer reads the view's own highest land
	/// and scales to it, so a flat world and an alpine one both come out
	/// legible, and a world whose elevation unit means something else entirely
	/// still does.
	float ReliefFraction = 0.08f;
	/// Thickness of a slab. The sea is drawn flat.
	float SlabHeight = 40.0f;
	/// Height of a settlement marker above the ground it stands on.
	float TownHeight = 400.0f;
	/// Width of a road, before its traffic widens it.
	float RoadWidth = 30.0f;
	/// A road that has carried this much is drawn at double width. Traffic
	/// spans four orders of magnitude, so the width goes by the cube root of
	/// it - the same choice Tools/Viewer/Atlas.html made, for the same reason.
	uint64 RoadBusy = 20000;
};

/// What one call to Draw put on the ground, so a caller can log it and a
/// reader can tell an empty world from a failed one - the two look identical
/// in a viewport.
struct FVaelenDrawTally
{
	int32 Tiles = 0;		///< slabs placed
	int32 Land = 0;			///< of them, above water
	int32 Towns = 0;		///< settlement markers
	int32 Roads = 0;		///< open routes drawn
	int32 SkippedRoads = 0; ///< routes whose regions the frame does not have
};

namespace VaelenViewDrawer
{
	/// The ground: one instance per tile, raised by its elevation.
	///
	/// Takes a MapView and a component. There is no third parameter that could
	/// be a world, and that absence is the whole design.
	///
	/// OutDistinctColours reports how many different colours were actually
	/// written to the instances. It exists because a flat-looking map has two
	/// very different causes - the drawer painted one colour, or it painted
	/// many and the material showed one - and from a screenshot they are the
	/// same picture. This number tells them apart without another round trip.
	VAELENPRESENTATION_API int32 DrawGround(const Vaelen::View::MapView& Map, const FVaelenDrawSettings& How,
											UHierarchicalInstancedStaticMeshComponent* Into, int32& OutLandTiles,
											int32& OutDistinctColours);

	/// The towns: one marker per region that has a settlement, standing on the
	/// tile the region calls its centroid.
	VAELENPRESENTATION_API int32 DrawTowns(const Vaelen::View::WorldView& Frame, const Vaelen::View::MapView& Map,
										   const FVaelenDrawSettings& How,
										   UHierarchicalInstancedStaticMeshComponent* Into);

	/// The roads: one box per OPEN route, stretched between the centroids of
	/// the two regions it joins, widened by what it has carried.
	///
	/// Needs the frame as well as the network, because a route names its
	/// regions by index and only the frame knows where a region sits. A route
	/// whose regions the frame does not have is counted in OutSkipped and not
	/// drawn - the alternative is drawing a road to the origin, which is a
	/// confident wrong picture, and this whole layer exists to prevent those.
	VAELENPRESENTATION_API int32 DrawRoads(const Vaelen::View::NetView& Net, const Vaelen::View::WorldView& Frame,
										   const Vaelen::View::MapView& Map, const FVaelenDrawSettings& How,
										   UHierarchicalInstancedStaticMeshComponent* Into, int32& OutSkipped);

	/// The colour a biome is drawn in. Biome is a number in the view; what it
	/// LOOKS like is a decision of this layer and of no other, which is why the
	/// palette lives in the .cpp here and not in VaelenSim.
	///
	/// COLOUR NEEDS A MATERIAL THIS MODULE DOES NOT SHIP. Each instance gets
	/// its colour as three floats of per-instance custom data (0 = R, 1 = G,
	/// 2 = B). A material that does not read PerInstanceCustomData will draw
	/// AELVOR in one flat colour and give no hint why. Said here rather than
	/// left to be discovered: the first person to build this should expect a
	/// grey world until they point the components at a material that reads it.
	VAELENPRESENTATION_API FLinearColor ColourOfBiome(uint8 Biome, uint8 Ground);

	/// Where a tile's centre sits, in centimetres, given its index in the map.
	VAELENPRESENTATION_API FVector PlaceOfTile(const Vaelen::View::MapView& Map, uint32 Tile,
											   const FVaelenDrawSettings& How);
} // namespace VaelenViewDrawer
