// VAELEN - VaelenPresentation. Phase 13 task 13.07c.
//
// STATUS: PROTOTYPE - compiled and linked by UnrealBuildTool on UE 5.6.1 with
// MSVC 14.44 on 2026-09-10 (14 modules, 167 actions, Result: Succeeded), and
// NOT YET RUN. Nothing here has been dropped in a level or looked at, so every
// claim about what it DRAWS remains unmeasured. What compiling proves is only
// that it is the shape of a program.
//
// LOOK AT THE INCLUDES BELOW AND THEN LOOK FOR WHAT IS NOT THERE. No
// Vaelen/Sim/World.h. No Vaelen/Population/Persons.h. No Vaelen/Economy/
// anything. This file draws AELVOR and does not know the word World.
//
// Everything it has is three structs of numbers taken by 13.01, 13.07a and
// 13.08a. If somebody later needs "just one thing" out of the simulation to
// draw something here, the include they reach for will make this file stop
// compiling, and that is the design working rather than failing.
#include "VaelenViewDrawer.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"

namespace
{
	/// The eleven biomes of 02.04, in the order WorldGen::Biome declares them.
	/// Named here rather than pulled from the kernel on purpose: what a tundra
	/// LOOKS like is a decision of the presentation layer and of nothing else,
	/// and the day an artist wants a different green they should not have to
	/// touch a module the CI compiles four ways.
	constexpr FLinearColor BiomePaint[] = {
		FLinearColor(0.86f, 0.91f, 0.95f), // 0  Ice
		FLinearColor(0.62f, 0.65f, 0.58f), // 1  Tundra
		FLinearColor(0.16f, 0.32f, 0.24f), // 2  BorealForest
		FLinearColor(0.55f, 0.58f, 0.40f), // 3  ColdSteppe
		FLinearColor(0.20f, 0.45f, 0.22f), // 4  TemperateForest
		FLinearColor(0.45f, 0.60f, 0.28f), // 5  Grassland
		FLinearColor(0.58f, 0.55f, 0.32f), // 6  Scrubland
		FLinearColor(0.12f, 0.38f, 0.18f), // 7  TropicalForest
		FLinearColor(0.68f, 0.60f, 0.30f), // 8  Savanna
		FLinearColor(0.80f, 0.72f, 0.48f), // 9  Desert
		FLinearColor(0.70f, 0.70f, 0.72f), // 10 Alpine
	};
	constexpr int32 BiomeCount = static_cast<int32>(sizeof(BiomePaint) / sizeof(BiomePaint[0]));

	constexpr FLinearColor Sea(0.09f, 0.18f, 0.34f);
	constexpr FLinearColor River(0.20f, 0.42f, 0.68f);
	constexpr FLinearColor Lake(0.16f, 0.36f, 0.60f);
	constexpr FLinearColor Town(0.92f, 0.82f, 0.35f);
	constexpr FLinearColor Road(0.42f, 0.33f, 0.22f);

	/// Colour goes to the instance as three floats of per-instance custom data.
	/// A material that does not read PerInstanceCustomData will draw the whole
	/// world in one colour and be none the wiser, which is said plainly in
	/// VaelenViewDrawer.h rather than left for somebody to discover.
	void PaintInstance(UHierarchicalInstancedStaticMeshComponent* Into, int32 Index, const FLinearColor& C)
	{
		if (Into == nullptr || Index < 0)
		{
			return;
		}
		Into->SetCustomDataValue(Index, 0, C.R, false);
		Into->SetCustomDataValue(Index, 1, C.G, false);
		Into->SetCustomDataValue(Index, 2, C.B, false);
	}

	/// Three floats per instance, set once before anything is added.
	void WantColour(UHierarchicalInstancedStaticMeshComponent* Into)
	{
		if (Into != nullptr)
		{
			Into->NumCustomDataFloats = 3;
		}
	}

	/// The height, in centimetres, of the top of a tile's slab.
	float TopOf(const Vaelen::View::TileView& T, const FVaelenDrawSettings& How)
	{
		const bool bLand = (T.Ground & Vaelen::View::GroundFlag::Land) != 0u;
		return bLand ? static_cast<float>(T.Elevation) * How.ReliefScale : 0.0f;
	}
} // namespace

FLinearColor VaelenViewDrawer::ColourOfBiome(uint8 Biome, uint8 Ground)
{
	using namespace Vaelen::View;
	if ((Ground & GroundFlag::Land) == 0u)
	{
		return Sea;
	}
	if ((Ground & GroundFlag::River) != 0u)
	{
		return River;
	}
	if ((Ground & GroundFlag::Lake) != 0u)
	{
		return Lake;
	}
	const int32 Index = static_cast<int32>(Biome);
	return Index >= 0 && Index < BiomeCount ? BiomePaint[Index] : Sea;
}

FVector VaelenViewDrawer::PlaceOfTile(const Vaelen::View::MapView& Map, uint32 Tile, const FVaelenDrawSettings& How)
{
	if (Map.Width == 0)
	{
		return FVector::ZeroVector;
	}
	const uint32 X = Tile % Map.Width;
	const uint32 Y = Tile / Map.Width;
	// Centred on the actor, so a 256 map does not begin a kilometre away.
	const float HalfW = static_cast<float>(Map.Width) * 0.5f;
	const float HalfH = static_cast<float>(Map.Height) * 0.5f;
	return FVector((static_cast<float>(X) - HalfW) * How.TileSize, (static_cast<float>(Y) - HalfH) * How.TileSize,
				   0.0f);
}

int32 VaelenViewDrawer::DrawGround(const Vaelen::View::MapView& Map, const FVaelenDrawSettings& How,
								   UHierarchicalInstancedStaticMeshComponent* Into, int32& OutLandTiles)
{
	OutLandTiles = 0;
	if (Into == nullptr || Map.Width == 0 || Map.Height == 0)
	{
		return 0;
	}
	WantColour(Into);
	const int32 Count = static_cast<int32>(Map.Tiles.size());
	TArray<FTransform> Slabs;
	Slabs.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const Vaelen::View::TileView& T = Map.Tiles[static_cast<size_t>(i)];
		const bool bLand = (T.Ground & Vaelen::View::GroundFlag::Land) != 0u;
		OutLandTiles += bLand ? 1 : 0;
		FVector At = PlaceOfTile(Map, static_cast<uint32>(i), How);
		At.Z = TopOf(T, How);
		// A unit cube scaled to the slab: the mesh the caller set is assumed to
		// be 100 units a side, which is Unreal's own cube.
		const FVector Scale(How.TileSize / 100.0f, How.TileSize / 100.0f, How.SlabHeight / 100.0f);
		Slabs.Add(FTransform(FRotator::ZeroRotator, At, Scale));
	}
	const TArray<int32> Placed = Into->AddInstances(Slabs, true);
	for (int32 i = 0; i < Placed.Num() && i < Count; ++i)
	{
		const Vaelen::View::TileView& T = Map.Tiles[static_cast<size_t>(i)];
		PaintInstance(Into, Placed[i], ColourOfBiome(T.Biome, T.Ground));
	}
	Into->MarkRenderStateDirty();
	return Count;
}

int32 VaelenViewDrawer::DrawTowns(const Vaelen::View::WorldView& Frame, const Vaelen::View::MapView& Map,
								  const FVaelenDrawSettings& How, UHierarchicalInstancedStaticMeshComponent* Into)
{
	if (Into == nullptr || Map.Width == 0)
	{
		return 0;
	}
	TArray<FTransform> Marks;
	for (const Vaelen::View::RegionView& R : Frame.Regions)
	{
		if (R.Settlement == 0)
		{
			continue;
		}
		const uint32 Tile = R.CentroidTile;
		if (Tile >= Map.Tiles.size())
		{
			continue;
		}
		FVector At = PlaceOfTile(Map, Tile, How);
		At.Z = TopOf(Map.Tiles[Tile], How) + How.TownHeight * 0.5f;
		const FVector Scale(How.TileSize / 100.0f, How.TileSize / 100.0f, How.TownHeight / 100.0f);
		Marks.Add(FTransform(FRotator::ZeroRotator, At, Scale));
	}
	WantColour(Into);
	const TArray<int32> Placed = Into->AddInstances(Marks, true);
	for (int32 i = 0; i < Placed.Num(); ++i)
	{
		PaintInstance(Into, Placed[i], Town);
	}
	Into->MarkRenderStateDirty();
	return Marks.Num();
}

int32 VaelenViewDrawer::DrawRoads(const Vaelen::View::NetView& Net, const Vaelen::View::WorldView& Frame,
								  const Vaelen::View::MapView& Map, const FVaelenDrawSettings& How,
								  UHierarchicalInstancedStaticMeshComponent* Into, int32& OutSkipped)
{
	OutSkipped = 0;
	if (Into == nullptr || Map.Width == 0)
	{
		return 0;
	}
	TArray<FTransform> Bars;
	for (const Vaelen::View::RouteView& Rt : Net.Routes)
	{
		if (Rt.Open == 0)
		{
			continue;
		}
		const Vaelen::View::RegionView* A = Vaelen::View::RegionIn(Frame, Rt.From);
		const Vaelen::View::RegionView* B = Vaelen::View::RegionIn(Frame, Rt.To);
		if (A == nullptr || B == nullptr || A->CentroidTile >= Map.Tiles.size() || B->CentroidTile >= Map.Tiles.size())
		{
			// A road between regions this frame does not have. Not drawn, and
			// counted so the caller can say so - a road quietly drawn to the
			// origin is exactly the confident wrong picture this layer exists
			// to prevent.
			++OutSkipped;
			continue;
		}
		FVector From = PlaceOfTile(Map, A->CentroidTile, How);
		FVector To = PlaceOfTile(Map, B->CentroidTile, How);
		From.Z = TopOf(Map.Tiles[A->CentroidTile], How) + How.SlabHeight;
		To.Z = TopOf(Map.Tiles[B->CentroidTile], How) + How.SlabHeight;
		const FVector Along = To - From;
		const float Length = static_cast<float>(Along.Size());
		if (Length <= KINDA_SMALL_NUMBER)
		{
			++OutSkipped;
			continue;
		}
		// Traffic spans four orders of magnitude, so width goes by its cube
		// root: a road that carried a hundred thousand units is wider than one
		// that carried a hundred, but not a thousand times wider.
		const double Busy =
			How.RoadBusy > 0 ? static_cast<double>(Rt.Carried) / static_cast<double>(How.RoadBusy) : 0.0;
		const float Wide = How.RoadWidth * (1.0f + static_cast<float>(FMath::Pow(FMath::Max(0.0, Busy), 1.0 / 3.0)));
		const FVector Middle = (From + To) * 0.5;
		const FRotator Facing = Along.Rotation();
		const FVector Scale(Length / 100.0f, Wide / 100.0f, 0.12f);
		Bars.Add(FTransform(Facing, Middle, Scale));
	}
	WantColour(Into);
	const TArray<int32> Placed = Into->AddInstances(Bars, true);
	for (int32 i = 0; i < Placed.Num(); ++i)
	{
		PaintInstance(Into, Placed[i], Road);
	}
	Into->MarkRenderStateDirty();
	return Bars.Num();
}
