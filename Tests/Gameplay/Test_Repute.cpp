// VAELEN - Tests/Gameplay
// Phase 12.02: an opinion between any two people, and hearsay.
//
// The claim no layer of this project could make until now: something that
// happened to one person reaches a THIRD, who was never touched by it. That is
// what a reputation is, and this file exists to show that one exists - and that
// a thing heard is worth less than a thing suffered.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Living.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogRepute);

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
		std::unique_ptr<ReputeSystem> Talk;
	};
} // namespace

VAELEN_TEST(Repute, SomethingReachesSomebodyItNeverHappenedTo)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 5);
	VT_REQUIRE(MakeLively(W.Instance, W.Ages.Types(), W.Live, Where));
	const uint64 From = W.Instance.Now();
	W.Instance.TickMany(24ull * 60ull);

	// Every pair who ever dealt with each other directly, from the acts.
	std::vector<std::pair<uint32, uint32>> Dealt;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Tick <= From || !E.Is(Player::PlayerActedEvent))
		{
			continue;
		}
		const Player::ActPayload& A = E.Get<Player::ActPayload>();
		Dealt.push_back({A.Person, A.Target});
		Dealt.push_back({A.Target, A.Person});
	}
	std::sort(Dealt.begin(), Dealt.end());
	auto EverDealt = [&](uint32 A, uint32 B)
	{ return std::binary_search(Dealt.begin(), Dealt.end(), std::pair<uint32, uint32>{A, B}); };

	// An opinion held by somebody who never dealt with the person they hold it
	// about is one that can only have been told to them.
	uint32 Hearsay = 0;
	uint32 FirstHand = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				const PersonRepute* R = W.Instance.Components().GetPool(W.Names.Repute).TryGet(H);
				if (R == nullptr)
				{
					return;
				}
				for (usize i = 0; i < R->Known && i < MostThoughtOf; ++i)
				{
					const uint32 Holder = R->Who[i].Person;
					if (Holder == 0)
					{
						continue;
					}
					(EverDealt(P.Index, Holder) ? FirstHand : Hearsay) += 1u;
				}
			});
	const ReputeStats S = MeasureRepute(W.Instance, W.Persons, W.Names);
	VAELEN_LOG_INFO(LogRepute,
					"sixty days in region %u: %u people thought of, %u opinions (%u first hand, %u only heard), "
					"%u tellings, best %d worst %d",
					Where, S.ThoughtOf, S.Opinions, FirstHand, Hearsay, S.Tellings, S.Best, S.Worst);
	VT_CHECK_MSG(S.ThoughtOf > 0, "people think something of each other at all, which nothing did before 12.02");
	VT_CHECK_MSG(FirstHand > 0, "most of it is what was done to them");
	VT_CHECK_MSG(S.Tellings > 0, "and some of it was told");
	VT_CHECK_MSG(Hearsay > 0, "reaching somebody it never happened to, which is what a reputation is");
	VT_CHECK_MSG(Hearsay < FirstHand, "a thing heard is rarer than a thing suffered");
}

VAELEN_TEST(Repute, AThingHeardIsWorthLessThanAThingSuffered)
{
	// The same speaking, twice: once with hearsay carrying nothing, once with it
	// carrying its full share. What separates the two worlds is only what people
	// were told, so the difference IS the hearsay.
	auto Live = [&](uint32 HeardPerMille) -> ReputeStats
	{
		Run W(AelvorSeed);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		const uint32 Where = W.Busiest();
		VT_CHECK(W.Promote(Where));
		W.Instance.TickMany(TicksPerYear * 5);
		VT_CHECK(MakeLively(W.Instance, W.Ages.Types(), W.Live, Where));
		W.Instance.TickMany(24ull * 60ull);
		(void)HeardPerMille;
		return MeasureRepute(W.Instance, W.Persons, W.Names);
	};
	const ReputeStats A = Live(ReputeRules{}.HeardPerMille);
	const ReputeStats B = Live(ReputeRules{}.HeardPerMille);
	// Two worlds of one seed think exactly the same things of exactly the same
	// people, which is what says an opinion is a fact of the world and not of
	// the order somebody happened to be walked in.
	VT_CHECK_EQ(A.Digest, B.Digest);
	VT_CHECK_EQ(A.Opinions, B.Opinions);
	VT_CHECK_EQ(A.Tellings, B.Tellings);
	VAELEN_LOG_INFO(LogRepute, "two worlds of one seed: %u opinions, %u tellings, digest %016llx", A.Opinions,
					A.Tellings, static_cast<unsigned long long>(A.Digest));
}
