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
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Warmth.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/Delta.h"
#include "Vaelen/View/Eye.h"
#include "Vaelen/View/Folk.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Life.h"
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
		/// 14.04: the played life. Six type sets a screen reads through; a
		/// world without them (every Atlas world) takes an empty LifeView.
		bool HasLife = false;
		Player::HourTypes Hour;
		Player::OrderTypes Order;
		Player::RegardTypes Regard;
		Player::StartTypes Start;
		Population::NeedTypes Needs;
		Population::FamilyTypes Families;
		/// 14.05: the goods and the society, so that a line of the chronicle
		/// about a stock or an organization is worded by its own layer, as
		/// Run::Aelvor::Life() words it. Without them the person layer speaks
		/// for every layer, which is a plainer sentence and not an error.
		bool HasGoods = false;
		Economy::MarketTypes Markets;
		Society::OrganizationTypes Organizations;
		/// 18.02: whether the world has a climate to read. False from every
		/// source until 18.10 makes it the run's own flag; a take with it false
		/// is byte for byte the take of the world before Phase 18. 18.04 reads
		/// it: with it true the frame carries the season and every region's
		/// climate word, the life the day's degrees where the played person
		/// stands, and TakeClimateView fills a tile leaf; the rules are the
		/// host's, as OrderRules are.
		bool HasClimate = false;
		WorldGen::ClimateRules Climate;
		/// 18.05: the warmth a climate world declared (Population::WarmthTypes,
		/// after Polity and before Colony), read by TakeLifeView for the
		/// played person's chill. Separate from HasClimate because Aelvor
		/// declares the type from 18.05 on while its HasClimate waits for
		/// 18.10, and a take must never read a pool that was not declared.
		bool HasWarmth = false;
		Population::WarmthTypes Warmth;
	};

	/// Takes the frame. Const world in, numbers out: the signature is the
	/// promise, and it is the reason this function is worth a module.
	VAELEN_VIEW_API void TakeView(const World& W, const ViewSources& From, WorldView& Out);

	/// Takes the ground. Const world in, numbers out - the same signature and
	/// the same promise as TakeView, and the reason both live in this module.
	/// Out is left empty when the world's map has not been generated.
	VAELEN_VIEW_API void TakeMapView(const World& W, const ViewSources& From, MapView& Out);

	/// 18.04: the climate of every tile today. Empty, with Season 0, when the
	/// sources carry no climate. Once a day, like the map: a quarter of a
	/// megabyte at 256 is not a per-frame structure.
	VAELEN_VIEW_API void TakeClimateView(const World& W, const ViewSources& From, ClimateView& Out);

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

	/// Takes the played life (14.04). Const world in, numbers and names out.
	/// The cache is the caller's, as for TakeViewFor: the graph is a walk of
	/// the whole map and a screen is taken every frame, so the choice is made
	/// where somebody can see it. Out has Person = 0 and every name empty when
	/// the sources carry no life or nobody is played.
	VAELEN_VIEW_API void TakeLifeView(const World& W, const ViewSources& From, WorldGen::RegionGraphCache& Ways,
									  LifeView& Out);

	/// Takes the last lines of the played life (14.05), INCREMENTALLY: only
	/// the log events appended since the last take of this same view are read
	/// and described, and the view is started over when the played person is
	/// not the one it was taken for. Returns the events read this time. Out
	/// has Person = 0 and nothing written when the sources carry no life or
	/// nobody is played. A fresh view taken once equals one grown take by
	/// take, byte for byte.
	VAELEN_VIEW_API uint32 TakeChronicleView(const World& W, const ViewSources& From, ChronicleView& Out);

	/// How far one region is from another across borders, or `Unreached` when no
	/// chain of borders joins them. Public because "how far is that" is a
	/// question a renderer asks about things other than drawing.
	VAELEN_VIEW_API uint32 BordersBetween(const World& W, const History::PreHistoryTypes& Types,
										  WorldGen::RegionGraphCache& Ways, uint32 From, uint32 To);
} // namespace Vaelen::View
