// VAELEN - VaelenView
// Phase 14.02: the TAKE side of the view - the one header of this module that
// may name the World, and the one a UI may never include.
//
// STATUS: PROTOTYPE (Phase 14) - probe TU Tests/View/Probe_ViewLeaf.cpp, CTest View.Leaf, Atlas.Frozen128
//
// 13.01's promise is that a view holds no pointer, handle or reference to the
// World it came from, so a renderer that has one cannot reach the simulation
// even by mistake. The STRUCTS kept that promise. The HEADERS did not: Frame.h
// included eight kernel headers so that ViewSources and TakeView(const World&)
// could be declared beside WorldView, and the transitive closure of Frame.h
// was 63 headers, reaching Player/Commands.h - Submit(World&) - through
// Gameplay/Fame.h and Gameplay/Repute.h. Every other view header included
// Frame.h. So every consumer of a view could NAME the simulation's write entry
// point by include, through the one module the layering rule calls safe.
//
// This header is where that reach now lives, and nowhere else. Frame.h, Land.h,
// Net.h, Folk.h, Delta.h and Eye.h keep only their structs and their measures,
// and include only Core and ViewApi. ViewSources and every Take*(const World&,
// const ViewSources&, ...) are here. A file that takes a view includes this; a
// file that only reads one never needs to, and Tests/View/Probe_ViewLeaf.cpp
// proves it cannot by accident: it includes every view header but this one,
// with only Core, View and Player/Intent on its include path.
//
// Nothing moved but declarations. No struct, no field, no layout, and the
// frozen digests are asserted unmoved - VAELEN_VIEWGATE_FROZEN_VIEW in
// Test_ViewGate, and the ADR-0135 pair by the new CTest Atlas.Frozen128,
// which is the first time that pair has been held by anything but a document.
// ADR-0137.
#pragma once

#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/View/Delta.h"
#include "Vaelen/View/Eye.h"
#include "Vaelen/View/Folk.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Net.h"
#include "Vaelen/View/ViewApi.h"

namespace Vaelen
{
	class World;
}

namespace Vaelen::View
{
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

	/// Takes the ground. Const world in, numbers out - the same signature and
	/// the same promise as TakeView, and the reason both live in this module.
	/// Out is left empty when the world's map has not been generated.
	VAELEN_VIEW_API void TakeMapView(const World& W, const ViewSources& From, MapView& Out);

	/// Takes the network. Const world in, numbers out. Out is left empty when
	/// the sources carry neither trade nor a colony - a world with no economy
	/// has no roads, and says so rather than refusing.
	VAELEN_VIEW_API void TakeNetView(const World& W, const ViewSources& From, NetView& Out);

	/// Takes the people. Const world in, numbers out - the same signature and
	/// the same promise as TakeView and TakeNetView. Out is left empty when no
	/// region is detailed, which is a world nobody is looking at closely and not
	/// an error.
	VAELEN_VIEW_API void TakePeopleView(const World& W, const ViewSources& From, PeopleView& Out);

	/// Takes a frame for somebody looking from `At`. The regions come out in
	/// index order as always, and each carries its own `Grain_` so a renderer
	/// can switch on it.
	///
	/// The cache belongs to the caller, the way 10.05's Doings holds its own:
	/// building the region graph is a walk of the whole map and a frame is taken
	/// sixty times a second, so the choice is made where somebody can see it
	/// rather than hidden in a static.
	VAELEN_VIEW_API void TakeViewFor(const World& W, const ViewSources& From, const Eye& At,
									 WorldGen::RegionGraphCache& Ways, WorldView& Out);

	/// How far one region is from another across borders, or `Unreached` when no
	/// chain of borders joins them. Public because "how far is that" is a
	/// question a renderer asks about things other than drawing.
	VAELEN_VIEW_API uint32 BordersBetween(const World& W, const History::PreHistoryTypes& Types,
										  WorldGen::RegionGraphCache& Ways, uint32 From, uint32 To);
} // namespace Vaelen::View
