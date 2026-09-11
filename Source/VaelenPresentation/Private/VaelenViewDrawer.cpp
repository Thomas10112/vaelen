// VAELEN - VaelenPresentation. Phase 13 tasks 13.07c and 13.08b.
//
// STATUS: UNVERIFIED - the 13.07c part of this file was compiled by
// UnrealBuildTool on UE 5.6.1 with MSVC 14.44, dropped in a level and LOOKED AT
// on 2026-09-10: AELVOR stood there, twelve biomes, rivers, 44 towns, 90 roads,
// and the engine reported the same figures as the headless kernel.
//
// The 13.08b part - the people - has been compiled by NOTHING. The headless CI
// cannot build this module and there is no engine on the machine that wrote it,
// so what is claimed below about drawing a person is a claim about source text.
// 13.07c pushed a rename that did not compile and cost a round trip; this is the
// same exposure, named in advance rather than after.
//
// LOOK AT THE INCLUDES BELOW AND THEN LOOK FOR WHAT IS NOT THERE. No
// Vaelen/Sim/World.h. No Vaelen/Population/Persons.h. No Vaelen/Economy/
// anything. This file draws AELVOR and does not know the word World.
//
// Everything it has is four structs of numbers taken by 13.01, 13.07a, 13.08a
// and 13.08b. If somebody later needs "just one thing" out of the simulation to
// draw something here, the include they reach for will make this file stop
// compiling, and that is the design working rather than failing.
#include "VaelenViewDrawer.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"

namespace
{
	/// The twelve biomes of 02.04, in the order WorldGen::Biome declares them,
	/// indexed by the enum's own value.
	///
	/// This table used to hold eleven and start at Ice, while the enum starts at
	/// Ocean = 0 - so every land tile was painted as the biome one step colder
	/// than it is, and Alpine (11) ran off the end and came out sea blue. The
	/// comment above it said "the eleven biomes, in the order WorldGen::Biome
	/// declares them", which was wrong twice: there are twelve, and that was not
	/// the order. A comment asserting a correspondence is worth nothing unless
	/// something checks it, hence the static_assert below.
	///
	/// Named here rather than pulled from the kernel on purpose: what a tundra
	/// LOOKS like is a decision of the presentation layer and of nothing else,
	/// and the day an artist wants a different green they should not have to
	/// touch a module the CI compiles four ways.
	constexpr FLinearColor BiomePaint[] = {
		FLinearColor(0.09f, 0.18f, 0.34f), // 0  Ocean - land tiles never reach it
		FLinearColor(0.86f, 0.91f, 0.95f), // 1  Ice
		FLinearColor(0.62f, 0.65f, 0.58f), // 2  Tundra
		FLinearColor(0.16f, 0.32f, 0.24f), // 3  BorealForest
		FLinearColor(0.55f, 0.58f, 0.40f), // 4  ColdSteppe
		FLinearColor(0.20f, 0.45f, 0.22f), // 5  TemperateForest
		FLinearColor(0.45f, 0.60f, 0.28f), // 6  Grassland
		FLinearColor(0.58f, 0.55f, 0.32f), // 7  Scrubland
		FLinearColor(0.12f, 0.38f, 0.18f), // 8  TropicalForest
		FLinearColor(0.68f, 0.60f, 0.30f), // 9  Savanna
		FLinearColor(0.80f, 0.72f, 0.48f), // 10 Desert
		FLinearColor(0.70f, 0.70f, 0.72f), // 11 Alpine
	};
	constexpr int32 BiomeCount = static_cast<int32>(sizeof(BiomePaint) / sizeof(BiomePaint[0]));
	// Checked against the view, not against the kernel: this file is not
	// allowed to name WorldGen, and Build.cs says the compiler is what
	// enforces that. View::BiomeKinds is asserted against the enum in
	// Land.cpp, which is the one place that may see both.
	static_assert(BiomeCount == static_cast<int32>(Vaelen::View::BiomeKinds),
				  "BiomePaint must hold one colour per biome, indexed by the enum's own value");

	constexpr FLinearColor Sea(0.09f, 0.18f, 0.34f);
	constexpr FLinearColor River(0.20f, 0.42f, 0.68f);
	constexpr FLinearColor Lake(0.16f, 0.36f, 0.60f);
	constexpr FLinearColor Town(0.92f, 0.82f, 0.35f);
	constexpr FLinearColor Road(0.42f, 0.33f, 0.22f);

	/// The three ages of a person. Chosen to read against the ground rather
	/// than to be pretty: every biome in the table above is a green, a tan or a
	/// grey, so people are drawn in the one family of hues the land never uses.
	constexpr FLinearColor Child(0.98f, 0.70f, 0.78f);
	constexpr FLinearColor Grown(0.86f, 0.20f, 0.24f);
	constexpr FLinearColor Elder(0.98f, 0.96f, 0.98f);

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

	/// Centimetres of height per elevation unit, so that the highest land in
	/// THIS view stands ReliefFraction of the map's width above the sea.
	float ReliefPerUnit(const Vaelen::View::MapView& Map, const FVaelenDrawSettings& How)
	{
		int32 Highest = 1;
		for (const Vaelen::View::TileView& T : Map.Tiles)
		{
			if ((T.Ground & Vaelen::View::GroundFlag::Land) != 0u && T.Elevation > Highest)
			{
				Highest = T.Elevation;
			}
		}
		const float Across = static_cast<float>(Map.Width) * How.TileSize;
		return Across * How.ReliefFraction / static_cast<float>(Highest);
	}

	/// The height, in centimetres, of the top of a tile's slab. The sea is flat.
	float TopOf(const Vaelen::View::TileView& T, float PerUnit)
	{
		const bool bLand = (T.Ground & Vaelen::View::GroundFlag::Land) != 0u;
		return bLand ? static_cast<float>(T.Elevation) * PerUnit : 0.0f;
	}

	/// The surface a thing standing on a tile stands on. The slab is a cube
	/// centred on TopOf, so its top face is half a slab higher - which is a
	/// sentence worth writing down, because getting it wrong buries a figure to
	/// the waist and looks like a shorter figure rather than like a mistake.
	float GroundLevel(const Vaelen::View::TileView& T, float PerUnit, const FVaelenDrawSettings& How)
	{
		return TopOf(T, PerUnit) + How.SlabHeight * 0.5f;
	}

	/// One draw from a person's own identity, stable across machines because
	/// it is the kernel's hash and not the engine's RNG. Nth says which draw:
	/// the tile, then the offset east, then the offset north, all from the same
	/// person and all different.
	Vaelen::uint64 DrawOf(Vaelen::Hash64 Identity, Vaelen::uint64 Nth)
	{
		return static_cast<Vaelen::uint64>(
			Vaelen::HashCombine(Vaelen::HashUInt64(static_cast<Vaelen::uint64>(Identity)), Vaelen::HashUInt64(Nth)));
	}

	/// A draw turned into a fraction of a tile, from -Spread to +Spread.
	float Scatter(Vaelen::uint64 Draw, float Spread)
	{
		// 12 bits is finer than a pixel at any camera height this map is looked
		// at from, and keeps the arithmetic in float exactly.
		const float Unit = static_cast<float>(Draw & 0xFFFu) / 4095.0f;
		return (Unit - 0.5f) * 2.0f * Spread;
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
								   UHierarchicalInstancedStaticMeshComponent* Into, int32& OutLandTiles,
								   int32& OutDistinctColours)
{
	OutLandTiles = 0;
	OutDistinctColours = 0;
	if (Into == nullptr || Map.Width == 0 || Map.Height == 0)
	{
		return 0;
	}
	WantColour(Into);
	const float PerUnit = ReliefPerUnit(Map, How);
	const int32 Count = static_cast<int32>(Map.Tiles.size());
	TArray<FTransform> Slabs;
	Slabs.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const Vaelen::View::TileView& T = Map.Tiles[static_cast<size_t>(i)];
		const bool bLand = (T.Ground & Vaelen::View::GroundFlag::Land) != 0u;
		OutLandTiles += bLand ? 1 : 0;
		FVector At = PlaceOfTile(Map, static_cast<uint32>(i), How);
		At.Z = TopOf(T, PerUnit);
		// A unit cube scaled to the slab: the mesh the caller set is assumed to
		// be 100 units a side, which is Unreal's own cube.
		const FVector Scale(How.TileSize / 100.0f, How.TileSize / 100.0f, How.SlabHeight / 100.0f);
		Slabs.Add(FTransform(FRotator::ZeroRotator, At, Scale));
	}
	const TArray<int32> Placed = Into->AddInstances(Slabs, true);
	TArray<FLinearColor> Used;
	for (int32 i = 0; i < Placed.Num() && i < Count; ++i)
	{
		const Vaelen::View::TileView& T = Map.Tiles[static_cast<size_t>(i)];
		const FLinearColor C = ColourOfBiome(T.Biome, T.Ground);
		PaintInstance(Into, Placed[i], C);
		Used.AddUnique(C);
	}
	OutDistinctColours = Used.Num();
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
	const float PerUnit = ReliefPerUnit(Map, How);
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
		At.Z = TopOf(Map.Tiles[Tile], PerUnit) + How.TownHeight * 0.5f;
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
	const float PerUnit = ReliefPerUnit(Map, How);
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
		From.Z = TopOf(Map.Tiles[A->CentroidTile], PerUnit) + How.SlabHeight;
		To.Z = TopOf(Map.Tiles[B->CentroidTile], PerUnit) + How.SlabHeight;
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

FLinearColor VaelenViewDrawer::ColourOfPerson(const Vaelen::View::PersonView& Who, const FVaelenDrawSettings& How)
{
	if (Who.Years < How.ChildYears)
	{
		return Child;
	}
	return Who.Years >= How.ElderYears ? Elder : Grown;
}

int32 VaelenViewDrawer::DrawFolk(const Vaelen::View::PeopleView& Folk, const Vaelen::View::MapView& Map,
								 const FVaelenDrawSettings& How, UHierarchicalInstancedStaticMeshComponent* Into,
								 int32& OutSkipped, int32& OutTiles)
{
	using namespace Vaelen::View;

	OutSkipped = 0;
	OutTiles = 0;
	if (Into == nullptr || Map.Width == 0 || Map.Tiles.empty())
	{
		return 0;
	}

	// The ground each region owns, in ONE pass over the map. A person names a
	// region and the map names a region per tile, so this is the join the whole
	// function turns on, and doing it per person would be a scan of the world
	// for every figure drawn.
	//
	// Land only. A region touching the coast owns sea tiles too and nobody
	// stands on those.
	TMap<uint32, TArray<int32>> Held;
	const int32 TileCount = static_cast<int32>(Map.Tiles.size());
	for (int32 i = 0; i < TileCount; ++i)
	{
		const TileView& T = Map.Tiles[static_cast<size_t>(i)];
		if (T.Region != 0 && (T.Ground & GroundFlag::Land) != 0u)
		{
			Held.FindOrAdd(static_cast<uint32>(T.Region)).Add(i);
		}
	}

	const float PerUnit = ReliefPerUnit(Map, How);
	const float Spread = 0.4f; // of a tile, from its centre
	TArray<FTransform> Figures;
	TArray<FLinearColor> Paint;
	TSet<int32> Stood;
	Figures.Reserve(static_cast<int32>(Folk.Living));
	Paint.Reserve(static_cast<int32>(Folk.Living));

	for (const PersonView& P : Folk.People)
	{
		// The dead are in the view, kept for history, and are not drawn. So is
		// anyone Gone - they left the detailed grain and are nowhere at all,
		// which is exactly why IsAlive exists and "not dead" would not do.
		if (!IsAlive(P))
		{
			continue;
		}
		const TArray<int32>* Ground_ = Held.Find(P.Region);
		if (Ground_ == nullptr || Ground_->Num() == 0)
		{
			// Alive, in a region this map gives no land to. Counted rather than
			// dropped at the origin, and counted rather than dropped silently:
			// it would mean the people and the ground disagree about what
			// regions exist, and that is a finding about two views, not a
			// rounding error.
			++OutSkipped;
			continue;
		}
		const int32 Tile = (*Ground_)[static_cast<int32>(DrawOf(P.Identity, 1) % static_cast<uint64>(Ground_->Num()))];
		FVector At = PlaceOfTile(Map, static_cast<uint32>(Tile), How);
		At.X += Scatter(DrawOf(P.Identity, 2), Spread) * How.TileSize;
		At.Y += Scatter(DrawOf(P.Identity, 3), Spread) * How.TileSize;
		At.Z = GroundLevel(Map.Tiles[static_cast<size_t>(Tile)], PerUnit, How) + How.FolkHeight * 0.5f;
		const FVector Scale(How.FolkSize / 100.0f, How.FolkSize / 100.0f, How.FolkHeight / 100.0f);
		Figures.Add(FTransform(FRotator::ZeroRotator, At, Scale));
		Paint.Add(ColourOfPerson(P, How));
		Stood.Add(Tile);
	}

	WantColour(Into);
	const TArray<int32> Placed = Into->AddInstances(Figures, true);
	for (int32 i = 0; i < Placed.Num() && i < Paint.Num(); ++i)
	{
		PaintInstance(Into, Placed[i], Paint[i]);
	}
	Into->MarkRenderStateDirty();
	OutTiles = Stood.Num();
	return Figures.Num();
}
