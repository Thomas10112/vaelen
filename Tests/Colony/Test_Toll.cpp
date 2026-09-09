// VAELEN - Tests/Colony
// Phase 11.05: what a colony does to the people in it.
//
// This file is named for the QUESTION and not for the answer. Phase 10 read
// that a bound person dies young, and the roadmap asks whether that is the
// world being harsh or the model being wrong.
//
// Reading the code before measuring anything says there is no mechanism at all:
// LifeRules has no bondage term and mortality is by age band alone; death by
// health has one path, and health falls only from hunger and plague; and being
// fed last is a question of family (HouseAt) rather than of the bond. So the
// expected answer is "the ground, not the bond" - and this test is written so
// that it could say the opposite.
//
// STATUS: PROTOTYPE (Phase 11)

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
	VAELEN_DEFINE_LOG_CATEGORY(LogToll);

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
			Ranks->ObserveBonds(Bondage.Bond);
			Orgs->ObserveBonds(Bondage.Bond);
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveMined(Colony.Mined);
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
	};
} // namespace

VAELEN_TEST(Toll, DoesTheBondItselfShortenALife)
{
	// The control has to come from an ORDINARY region, not from a colony. In a
	// colony there is no free group of working age to compare against: 11.04's
	// founding binds every adult but the elite, so the free are children and
	// notables. An ordinary region of AELVOR settles near a twelfth of its
	// people bound by 05.04's own rates, which gives both groups on one ground.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(W.Promote(Where));
	// Long enough for 05.04 to bind a fair number, before the cohort is taken.
	W.Instance.TickMany(TicksPerYear * 40);

	const uint64 From = W.Instance.Now();
	std::vector<uint32> Bound, Free;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				if (P.Region != Where || P.State != static_cast<uint8>(LifeState::Alive))
				{
					return;
				}
				// One cohort and one age band, so that what is compared is the
				// bond and not who happened to be young. Twenty to forty is an
				// age everybody in both groups has reached and none ages out of.
				const uint32 Age = AgeYears(P, From);
				if (Age < 20 || Age > 40)
				{
					return;
				}
				(W.Instance.Components().GetPool(W.Bondage.Bond).TryGet(H) != nullptr ? Bound : Free)
					.push_back(P.Index);
			});
	std::sort(Bound.begin(), Bound.end());
	std::sort(Free.begin(), Free.end());
	VAELEN_LOG_INFO(LogToll, "region %u at year %u: %zu bound and %zu free aged 20-40", Where,
					static_cast<uint32>(From / TicksPerYear), Bound.size(), Free.size());
	VT_REQUIRE(Bound.size() > 20 && Free.size() > 20);

	W.Instance.TickMany(TicksPerYear * 70);

	uint64 BoundAges = 0, FreeAges = 0;
	uint32 BoundDeaths = 0, FreeDeaths = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Tick <= From || !E.Is(PersonDiedEvent))
		{
			continue;
		}
		const PersonPayload& P = E.Get<PersonPayload>();
		if (std::binary_search(Bound.begin(), Bound.end(), P.Person))
		{
			BoundAges += P.AgeYears;
			++BoundDeaths;
		}
		else if (std::binary_search(Free.begin(), Free.end(), P.Person))
		{
			FreeAges += P.AgeYears;
			++FreeDeaths;
		}
	}
	VT_CHECK_MSG(BoundDeaths > 15 && FreeDeaths > 15, "enough of both lived out their lives to compare");
	const uint32 BoundMean = static_cast<uint32>(BoundAges / (BoundDeaths == 0 ? 1u : BoundDeaths));
	const uint32 FreeMean = static_cast<uint32>(FreeAges / (FreeDeaths == 0 ? 1u : FreeDeaths));
	VAELEN_LOG_INFO(LogToll, "seventy years on: %u of the bound died at a mean age of %u, %u of the free at %u",
					BoundDeaths, BoundMean, FreeDeaths, FreeMean);
	// The claim, and it is the answer to the Phase 10 question: the bond does
	// not shorten a life, because nothing in the model makes it. LifeRules has
	// no bondage term, death by health comes only from hunger and plague, and
	// being fed last is a question of family rather than of the bond.
	//
	// If this ever fails, there IS a mechanism nobody documented and finding it
	// is the task. Do not widen the tolerance to make it pass.
	const uint32 Gap = BoundMean > FreeMean ? BoundMean - FreeMean : FreeMean - BoundMean;
	VT_CHECK_MSG(Gap <= 5, "the bond itself does not shorten a life");
}

VAELEN_TEST(Toll, WhatAColonyCostsThePeopleInIt)
{
	// The same ground twice, from one seed: left to farm, and founded as a
	// colony. Only the founding differs, so what separates them is what a
	// colony does to the people in it - which is the other half of the roadmap
	// line, and the half that turns out to matter.
	auto Live = [&](bool AsColony, uint32& Left, uint32 ByCause[4]) -> uint32
	{
		Run W(AelvorSeed);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		const uint32 Where = W.Busiest();
		VT_CHECK(W.Promote(Where));
		if (AsColony)
		{
			VT_CHECK(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
			VT_CHECK(BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where,
								W.Instance.Now()) > 0);
		}
		// Fed far past what it can eat either way, so this is not a test about
		// running out of grain.
		W.Feed(Where, 8000000u);
		const uint64 From = W.Instance.Now();
		const uint32 Was = W.Alive(Where);
		for (uint32 Decade = 1; Decade <= 6; ++Decade)
		{
			W.Instance.TickMany(TicksPerYear * 10);
			VAELEN_LOG_INFO(LogToll, "  %s year %u: %u alive, %u grain in the common stock",
							AsColony ? "mine " : "farm ", Decade * 10u, W.Alive(Where), W.Grain(Where));
		}
		for (const Event& E : W.Instance.Log().All())
		{
			if (E.Tick <= From || !E.Is(PersonDiedEvent))
			{
				continue;
			}
			const PersonPayload& P = E.Get<PersonPayload>();
			// PersonPayload::Other carries the DeathCause on a death (0 natural,
			// 1 famine, 2 starvation, 3 plague), not a separate Cause field.
			if (P.Region == Where)
			{
				ByCause[P.Other < 4u ? P.Other : 0u] += 1u;
			}
		}
		Left = W.Alive(Where);
		return Was;
	};
	uint32 FarmLeft = 0, PitLeft = 0;
	uint32 FarmCause[4] = {}, PitCause[4] = {};
	const uint32 FarmWas = Live(false, FarmLeft, FarmCause);
	const uint32 PitWas = Live(true, PitLeft, PitCause);

	VAELEN_LOG_INFO(LogToll, "sixty years farming: %u people became %u (natural %u, famine %u, starved %u, plague %u)",
					FarmWas, FarmLeft, FarmCause[0], FarmCause[1], FarmCause[2], FarmCause[3]);
	VAELEN_LOG_INFO(LogToll, "sixty years mining:  %u people became %u (natural %u, famine %u, starved %u, plague %u)",
					PitWas, PitLeft, PitCause[0], PitCause[1], PitCause[2], PitCause[3]);
	VT_CHECK_EQ(FarmWas, PitWas); // the two worlds start level
	// The two worlds drain the same endowment at the same rate. The farm lives
	// because it REAPS when the pile runs out; the colony reaps nothing, so it
	// is fine while the store covers the year's need and collapses inside a
	// decade of the year it stops. An endowment only moves the date.
	VT_CHECK_MSG(PitLeft * 10 < FarmLeft, "a colony without a road empties itself");
	VT_CHECK_MSG(FarmLeft >= FarmWas, "and the same ground left to farm feeds itself indefinitely");
	// And it is starvation that does it - not famine, which needs a drought,
	// and not plague. Naming the cause is the point of the test: the colony's
	// people die with a full granary behind them until the year it is not.
	VT_CHECK_MSG(PitCause[2] > 0, "the colony's dead starved");
	VT_CHECK_EQ(FarmCause[2], 0u);
	VT_CHECK_MSG(PitCause[3] == 0 && FarmCause[3] == 0, "no plague in either, so sickness is not the difference");
}
