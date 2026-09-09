// VAELEN - Tests/Gameplay
// Phase 12.04: maps.
//
// The one document the world can check. An opinion has no truth to be measured
// against; a claim about ground does, because 02.06 built the region graph and
// AreAdjacent will answer. So this is where what is true and what somebody
// believes can be held side by side - and where a person who read a road off a
// page believes in it exactly as readily as one who walked it.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Living.h"
#include "Vaelen/Gameplay/Documents.h"
#include "Vaelen/Gameplay/Maps.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Colony/Holders.h"
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Colony;
using namespace Vaelen::Gameplay;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogMaps);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// The whole stack: a colony needs bondage AND feeding to be measured, and
	/// the two live in different modules.
	struct Run
	{
		explicit Run(uint64 Seed, uint32 HeldRegion = 0, LivingRules InLive = LivingRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Colony = ColonyTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			// NOT LodRules::Held. That rule is fixed when the system is built, so
			// it holds the region through the whole of Generate's pre-history -
			// and with the full economy running, three hundred years of that
			// empties the region: 1460 people in a world without the hold, none
			// in the world with it. The dated way is RequestDetail (04.06), which
			// protects a region from demotion (Lod.cpp:170) exactly as the rule
			// does, but from the tick it is asked rather than from the beginning
			// of time. Same lesson as ADR-0090: a colony is a fact with a date.
			(void)HeldRegion;
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage,
													BondageRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Ranks->ObserveBonds(Bondage.Bond);
			Orgs->ObserveBonds(Bondage.Bond);
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveMined(Colony.Mined);
			// 12.01: the people of a lively region act for themselves, through the
			// very Doings 10.05 hands to the player's own system.
			Live = LivingTypes::Declare(Instance);
			Acts = std::make_unique<Player::Doings>(Ages.Types(), Persons, Families, Needs, Economy_,
													Player::DoingRules{});
			Lives_ = std::make_unique<LivingSystem>(Instance, Ages.Types(), Persons, Live, InLive);
			Lives_->ObserveDoing(Acts.get());
			// 12.02: what people make of each other, and what they are told.
			Names = ReputeTypes::Declare(Instance);
			Papers = DocumentTypes::Declare(Instance);
			Charts = MapTypes::Declare(Instance);
			Talk = std::make_unique<ReputeSystem>(Instance, Persons, Names, ReputeRules{});
			Talk->RunAfter("Living");
			// 11.06: and who is on the rock, so the rest of the colony still farms.
			Harvest->ObserveBonds(Bondage.Bond);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Stocks->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Lives_.get());
			Instance.Systems().Add(Talk.get());
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		static WorldGenConfig Square(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			return Gen;
		}
		uint32 Busiest() const
		{
			uint32 Best = 0;
			uint32 People = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && (P->Total > People || (P->Total == People && R.Index < Best)))
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		uint32 Alive(uint32 Region) const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach([&](EntityHandle, const PersonInfo& P)
						 { N += P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) ? 1u : 0u; });
			return N;
		}
		/// Asks for a region to be simulated person by person from now on, and
		/// materialises it at once. RequestDetail is what keeps it detailed
		/// afterwards - the bridge will not demote a region that is wanted.
		bool Promote(uint32 Region)
		{
			RequestDetail(Instance, Lod, Region);
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0 ||
				   Alive(Region) > 0;
		}
		uint32 Grain(uint32 Region) const
		{
			EntityHandle RH;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region && RH.IsNull())
						{
							RH = H;
						}
					});
			const RegionStock* S = RH.IsNull() ? nullptr : Instance.Components().GetPool(Economy_.Region).TryGet(RH);
			return S == nullptr ? 0u : S->Amount[static_cast<uint32>(Good::Grain)];
		}
		uint32 Feed(uint32 Region, uint32 Units)
		{
			return AddStock(Instance, Ages.Types(), Families, Economy_, Region, 0u, Good::Grain,
							static_cast<int32>(Units), Instance.Now());
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		EconomyTypes Economy_;
		ProductionTypes Production;
		ColonyTypes Colony;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		LivingTypes Live;
		std::unique_ptr<Player::Doings> Acts;
		std::unique_ptr<LivingSystem> Lives_;
		ReputeTypes Names;
		DocumentTypes Papers;
		MapTypes Charts;
		std::unique_ptr<ReputeSystem> Talk;
	};
} // namespace

VAELEN_TEST(Maps, AMapOfWalkedGroundIsTrueAndAForgedOneIsNot)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear);

	// The world's own answer about its ground, which is what makes a map
	// checkable at all.
	const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Ages.Types().World.Regions);
	VT_REQUIRE(Graph.RegionCount() > 1);
	VT_REQUIRE(Where < Graph.Neighbours.size());
	VT_REQUIRE(!Graph.Neighbours[Where].empty());
	const uint32 Next_ = Graph.Neighbours[Where][0];

	// Somebody of the region, who walks next door and back.
	uint32 Walker = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (Walker == 0 && P.Region == Where && P.State == static_cast<uint8>(LifeState::Alive))
				{
					Walker = P.Index;
				}
			});
	VT_REQUIRE(Walker != 0);
	VT_CHECK(NoteGround(W.Instance, W.Persons, W.Charts, Walker, W.Instance.Now()));
	VT_REQUIRE(W.Promote(Next_)); // a person only walks to ground the world is simulating
	VT_CHECK(MovePerson(W.Instance, W.Ages.Types(), W.Persons, Walker, Next_, W.Instance.Now()));
	VT_CHECK(NoteGround(W.Instance, W.Persons, W.Charts, Walker, W.Instance.Now()));

	const PersonGround* G = GroundOf(W.Instance, W.Persons, W.Charts, Walker);
	VT_REQUIRE(G != nullptr);
	VT_CHECK_MSG(HasWalked(*G, Where) && HasWalked(*G, Next_), "they have stood on both");

	const uint32 Map = WriteMap(W.Instance, W.Persons, W.Charts, Walker, W.Instance.Now());
	VT_REQUIRE(Map != 0);
	const MapCheck Honest = CheckMap(W.Instance, Graph, W.Charts, Map);
	VAELEN_LOG_INFO(LogMaps, "a map of ground walked from %u to %u: %u claim(s), %u true, %u false", Where, Next_,
					Honest.Claims, Honest.True_, Honest.False_);
	VT_CHECK_MSG(Honest.Claims > 0, "the map says something");
	VT_CHECK_MSG(Honest.False_ == 0, "and everything it says is true, because it was walked");

	// Now a road nobody walked, put on the same map. Ground does not move, so a
	// map is wrong because somebody drew it wrong.
	uint32 Far = 0;
	for (uint32 R = 1; R <= Graph.RegionCount() && Far == 0; ++R)
	{
		if (R != Where && R != Next_ && !Graph.AreAdjacent(static_cast<uint16>(Where), static_cast<uint16>(R)))
		{
			Far = R;
		}
	}
	VT_REQUIRE(Far != 0);
	VT_CHECK(ForgeClaim(W.Instance, W.Charts, Map, Where, Far));
	const MapCheck Forged = CheckMap(W.Instance, Graph, W.Charts, Map);
	VAELEN_LOG_INFO(LogMaps, "with one forged road from %u to %u: %u claim(s), %u true, %u false", Where, Far,
					Forged.Claims, Forged.True_, Forged.False_);
	VT_CHECK_EQ(Forged.Claims, Honest.Claims + 1u);
	VT_CHECK_MSG(Forged.False_ == 1u, "and the world says exactly which one is a lie");
}

VAELEN_TEST(Maps, AReaderBelievesTheForgedRoadExactlyAsReadily)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear);
	const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Ages.Types().World.Regions);
	const uint32 Next_ = Graph.Neighbours[Where][0];

	uint32 Walker = 0;
	uint32 Reader = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (P.Region != Where || P.State != static_cast<uint8>(LifeState::Alive))
				{
					return;
				}
				if (Walker == 0)
				{
					Walker = P.Index;
				}
				else if (Reader == 0)
				{
					Reader = P.Index;
				}
			});
	VT_REQUIRE(Walker != 0 && Reader != 0);
	VT_CHECK(NoteGround(W.Instance, W.Persons, W.Charts, Walker, W.Instance.Now()));
	VT_REQUIRE(W.Promote(Next_));
	VT_CHECK(MovePerson(W.Instance, W.Ages.Types(), W.Persons, Walker, Next_, W.Instance.Now()));
	VT_CHECK(NoteGround(W.Instance, W.Persons, W.Charts, Walker, W.Instance.Now()));
	const uint32 Map = WriteMap(W.Instance, W.Persons, W.Charts, Walker, W.Instance.Now());
	VT_REQUIRE(Map != 0);

	uint32 Far = 0;
	for (uint32 R = 1; R <= Graph.RegionCount() && Far == 0; ++R)
	{
		if (R != Where && R != Next_ && !Graph.AreAdjacent(static_cast<uint16>(Where), static_cast<uint16>(R)))
		{
			Far = R;
		}
	}
	VT_REQUIRE(Far != 0 && ForgeClaim(W.Instance, W.Charts, Map, Where, Far));

	// A reader who has stood in one region their whole life.
	VT_CHECK(NoteGround(W.Instance, W.Persons, W.Charts, Reader, W.Instance.Now()));
	const PersonGround* Before = GroundOf(W.Instance, W.Persons, W.Charts, Reader);
	VT_REQUIRE(Before != nullptr);
	VT_CHECK_MSG(!CanName(*Before, Far), "they cannot name ground they have never heard of");
	VT_CHECK(ReadMap(W.Instance, W.Persons, W.Charts, Map, Reader, W.Instance.Now()));

	const PersonGround* After = GroundOf(W.Instance, W.Persons, W.Charts, Reader);
	VT_REQUIRE(After != nullptr);
	VT_CHECK_MSG(CanName(*After, Next_), "they can name the road that is there");
	VT_CHECK_MSG(CanName(*After, Far), "and the road that is not, exactly as readily");
	VT_CHECK_MSG(!HasWalked(*After, Next_) && !HasWalked(*After, Far),
				 "and they have walked neither, which is the only thing that tells them apart");
	const MapStats S = MeasureMaps(W.Instance, W.Persons, W.Charts);
	VAELEN_LOG_INFO(LogMaps, "%u map(s), %u claim(s) of which %u forged; %u people name ground, %u walked, %u read",
					S.Maps, S.Claims, S.Forged, S.KnowGround, S.Walked, S.Read);
	VT_CHECK_MSG(S.Forged == 1u, "one lie on one map");
}
