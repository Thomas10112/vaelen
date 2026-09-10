// VAELEN - VaelenView
// Phase 13 task 13.01: a read-only view of the world for a frame.
//
// The layering rule of this project has said since Phase 00 that PRESENTATION
// reads WORLD STATE and does not touch it. For thirteen phases that has been a
// promise kept by everybody remembering it. This is where it becomes
// structural.
//
// A `WorldView` is a flat block of numbers. It holds no pointer, no handle, no
// component type and no reference to the World it came from, so a renderer that
// has one cannot reach the simulation even by mistake - there is nothing in its
// hand to reach with. That is the whole design, and everything below follows
// from it: the view is taken by value, it can be copied, kept, compared to the
// one before it, and handed to another thread without the world caring.
//
// What it is NOT. It is not a snapshot (03.06 owns those, and they are the
// whole world including the things nobody can see). It is not a query - 03.07
// answers questions. It is one frame's worth of what can be drawn, and it is
// deliberately small enough to be taken sixty times a second.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Frame.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/View/ViewApi.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::View
{
	/// One region as a renderer needs it. Everything is a number and nothing is
	/// a handle: this struct crossing into a rendering thread carries no way
	/// back into the world.
	struct RegionView
	{
		int64 Elevation = 0;	 ///< Fix64 raw, as 02.02 keeps it - first, so nothing pads behind it
		uint32 Index = 0;		 ///< the region's own index, 1-based
		uint32 CentroidTile = 0; ///< where to draw it
		uint32 Tiles = 0;		 ///< how much ground it covers
		uint32 Biome = 0;		 ///< what it looks like
		uint32 People = 0;		 ///< how many live there
		uint32 Bound = 0;		 ///< of them, how many are not free
		uint32 Settlement = 0;	 ///< settlement index, 0 for none
		uint32 Roads = 0;		 ///< open routes touching it
		uint32 Names = 0;		 ///< names the place carries (12.05)
		uint32 Detailed = 0;	 ///< 1 when the world simulates it person by person
		uint32 Grain_ = 0;		 ///< View::Grain - how finely to draw it, filled by 13.03 and 0 (Near) otherwise
		uint32 Reserved = 0;	 ///< keeps the count of 32-bit fields even, which is what removes the padding
	};
	// NOT sizeof == 56 on its own, which is what this said first and was wrong.
	// The eleven 32-bit fields came to 52 bytes and the compiler rounded the
	// struct to 56 for its 8-byte alignment, leaving four bytes nobody wrote.
	// MeasureView hashes these structs and Diff memcmps them, so those four
	// bytes decided whether two identical regions compared equal - and they
	// happened to be zero under gcc and not under clang-release, which is how a
	// suite that was green in one preset failed in another. The assertion below
	// compares the size against the fields, so it cannot pass a padded struct.
	static_assert(sizeof(RegionView) == sizeof(int64) + 12 * sizeof(uint32),
				  "RegionView must have no padding: MeasureView hashes it and Diff memcmps it");

	/// The world as a renderer needs it, for one frame.
	struct WorldView
	{
		uint64 Tick = 0;				 ///< the frame this was taken at
		uint32 Year = 0;				 //
		uint32 Width = 0;				 ///< of the map, so a renderer can place a tile
		uint32 Height = 0;				 //
		uint32 People = 0;				 ///< alive in the whole world
		uint32 Played = 0;				 ///< person index of whoever is being played, 0 for nobody
		uint32 Reserved = 0;			 //
		std::vector<RegionView> Regions; ///< in index order, always
	};

	/// What the view needs to be told to look at. Every field beyond the first
	/// two is optional: a world without an economy still has a map and people,
	/// and the view of it says 0 roads rather than refusing to be taken.
	struct ViewSources
	{
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;

		bool HasBondage = false;
		Society::BondageTypes Bondage;
		bool HasTrade = false;
		Economy::TradeTypes Trade;
		bool HasFame = false;
		Gameplay::FameTypes Fame;
		bool HasPlayer = false;
		Player::PlayerTypes Played;
		bool HasColony = false;
		Colony::ColonyTypes Colony_;
	};

	/// Takes the frame. Const world in, numbers out: the signature is the
	/// promise, and it is the reason this function is worth a module.
	VAELEN_VIEW_API void TakeView(const World& W, const ViewSources& From, WorldView& Out);

	/// The view of one region, or nullptr when the view does not have it.
	VAELEN_VIEW_API const RegionView* RegionIn(const WorldView& V, uint32 Region);

	struct ViewStats
	{
		uint32 Regions = 0;	 ///< in the view
		uint32 Peopled = 0;	 ///< of them, ones with anybody living there
		uint32 Detailed = 0; ///< of them, ones simulated person by person
		uint32 Named = 0;	 ///< of them, ones carrying a name
		uint32 Bytes = 0;	 ///< what the whole view weighs, which is what a frame costs
		Hash64 Digest = 0;	 ///< every region in index order
	};
	VAELEN_VIEW_API ViewStats MeasureView(const WorldView& V);
} // namespace Vaelen::View
