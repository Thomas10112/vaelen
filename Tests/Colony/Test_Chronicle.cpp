// VAELEN - Tests/Colony
// Phase 11.07: the colony in the chronicle.
//
// The colony's acts have to be tellable, in the same hand as every layer below,
// and the same seed has to tell them the same way word for word. Founding a
// colony published nothing at all until this task; a thing the world does with
// no event behind it cannot be remembered, and that was the first thing to fix.
//
// STATUS: PROTOTYPE (Phase 11)

#include "Vaelen/Colony/ColonyHistory.h"
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
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Colony;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogChronicle);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// The whole stack: a colony needs bondage AND feeding to be measured, and
	/// the two live in different modules.
	struct Run
	{
		explicit Run(uint64 Seed, uint32 HeldRegion = 0) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
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
			Rock = std::make_unique<MiningSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Colony,
												  MiningRules{});
			Rock->ObserveTraits(Traits.Traits);
			Ranks->ObserveBonds(Bondage.Bond);
			Orgs->ObserveBonds(Bondage.Bond);
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveMined(Colony.Mined);
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
			Instance.Systems().Add(Rock.get());
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
		/// The peopled region with the most ore under it. The busiest region of
		/// AELVOR 128 has none at all, which 10.02 already knew: the ground a
		/// colony would stand on and the ground the most people live on are not
		/// the same ground.
		uint32 Orerichest() const
		{
			std::vector<uint32> Ore;
			Instance.Components()
				.GetPool(Ages.Types().World.DepositTypes_.Deposit)
				.ForEach(
					[&](EntityHandle, const DepositInfo& D)
					{
						const bool IsOre = D.Kind == static_cast<uint32>(ResourceKind::IronOre) ||
										   D.Kind == static_cast<uint32>(ResourceKind::CopperOre);
						if (!IsOre || D.Region == 0)
						{
							return;
						}
						if (D.Region >= Ore.size())
						{
							Ore.resize(usize{D.Region} + 1u, 0u);
						}
						Ore[D.Region] += D.Richness;
					});
			uint32 Best = 0;
			uint32 Most = 0;
			for (uint32 R = 1; R < Ore.size(); ++R)
			{
				bool Peopled = false;
				Instance.Components()
					.GetPool(Ages.Types().World.RegionTypes_.Region)
					.ForEach(
						[&](EntityHandle H, const RegionInfo& Info)
						{
							if (Info.Index != R)
							{
								return;
							}
							const RegionPopulation* P =
								Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
							Peopled = P != nullptr && P->Total > 0;
						});
				if (Peopled && Ore[R] > Most)
				{
					Most = Ore[R];
					Best = R;
				}
			}
			return Best;
		}
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
		std::unique_ptr<MiningSystem> Rock;
	};
} // namespace

VAELEN_TEST(Chronicle, EveryActOfAColonyHasASentence)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Orerichest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(W.Promote(Where));
	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	const uint32 Bound =
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where, W.Instance.Now());
	VT_REQUIRE(Bound > 0);
	W.Feed(Where, 8000000u);
	W.Instance.TickMany(TicksPerYear * 20);

	const ColonyContext Context{W.Colony, W.Persons, W.Bondage};
	uint32 Founded = 0, Lifts = 0, Spent = 0, Silent = 0;
	std::string First, LastLift, LastSpent;
	for (const Event& E : W.Instance.Log().All())
	{
		std::string Line;
		if (!DescribeColonyEvent(W.Instance, W.Ages.Types(), Context, E, Line))
		{
			continue; // an event of some layer below, which has its own sentence
		}
		VT_CHECK_MSG(!Line.empty(), "a colony event that is described is described in words");
		Silent += Line.empty() ? 1u : 0u;
		if (E.Is(ColonyFoundedEvent))
		{
			++Founded;
			First = Line;
		}
		else if (E.Is(OreLiftedEvent))
		{
			++Lifts;
			LastLift = Line;
		}
		else if (E.Is(SeamWorkedOutEvent))
		{
			++Spent;
			LastSpent = Line;
		}
	}
	VT_CHECK_EQ(Founded, 1u);
	VT_CHECK_MSG(Lifts > 0, "the colony lifted ore and every lift has a line");
	VT_CHECK_EQ(Silent, 0u);
	std::string Binding;
	DescribeBinding(W.Instance, W.Ages.Types(), Where, Bound, Binding);
	VAELEN_LOG_INFO(LogChronicle, "%s", First.c_str());
	VAELEN_LOG_INFO(LogChronicle, "%s", Binding.c_str());
	VAELEN_LOG_INFO(LogChronicle, "%s", LastLift.c_str());
	if (!LastSpent.empty())
	{
		VAELEN_LOG_INFO(LogChronicle, "%s", LastSpent.c_str());
	}
	VAELEN_LOG_INFO(LogChronicle, "%u foundings, %u lifts, %u seams spent, all with a sentence", Founded, Lifts, Spent);
}
