// VAELEN - Tests/Colony
// Phase 11.04: who holds a colony. 05.04 binds people one at a time and a
// holder takes no more than BondageRules::MaxHeldPerHolder. This asks whether
// that survives a region where nearly everybody is bound at once - a question
// Phase 05 never had to answer, because it never had a colony.
//
// STATUS: PROTOTYPE (Phase 11)

#include "Vaelen/Colony/Holders.h"
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
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

#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Colony;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogHolders);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, uint32 HeldRegion = 0, BondageRules InRules = BondageRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Colony = ColonyTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			// 11.01: the colony is a region the world keeps detailed.
			LodRules Grain;
			Grain.Held = HeldRegion;
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, Grain);
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage, InRules);
			Ranks->ObserveBonds(Bondage.Bond);
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->ObserveBonds(Bondage.Bond);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Bonds.get());
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
		bool Promote(uint32 Region)
		{
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0 ||
				   Alive(Region) > 0;
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
		/// Who holds the bound of a region: how many are held by a person, how
		/// many by the region itself (BondState::Holder == 0), how many elites
		/// there are to hold anybody, and how full the fullest of them is.
		struct Holding
		{
			uint32 Bound = 0;
			uint32 ByPerson = 0;
			uint32 ByRegion = 0;
			uint32 Elites = 0;
			uint32 Fullest = 0;
		};
		Holding Who(uint32 Region) const
		{
			Holding Out;
			std::vector<std::pair<uint32, uint32>> Held;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region != Region || P.State != static_cast<uint8>(LifeState::Alive))
						{
							return;
						}
						const PersonStanding* S = Instance.Components().GetPool(Standing.Standing).TryGet(H);
						const BondState* B = Instance.Components().GetPool(Bondage.Bond).TryGet(H);
						if (S != nullptr && S->Tier_ == static_cast<uint8>(Tier::Elite) && B == nullptr)
						{
							++Out.Elites;
						}
						if (B == nullptr)
						{
							return;
						}
						++Out.Bound;
						if (B->Holder == 0)
						{
							++Out.ByRegion;
							return;
						}
						++Out.ByPerson;
						bool Seen = false;
						for (auto& [Who_, Count] : Held)
						{
							if (Who_ == B->Holder)
							{
								++Count;
								Seen = true;
							}
						}
						if (!Seen)
						{
							Held.push_back({B->Holder, 1u});
						}
					});
			for (const auto& [Who_, Count] : Held)
			{
				(void)Who_;
				Out.Fullest = Count > Out.Fullest ? Count : Out.Fullest;
			}
			return Out;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		ColonyTypes Colony;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<BondageSystem> Bonds;
	};
} // namespace

VAELEN_TEST(Holders, WhoHoldsAColonyWhenTheEliteRunsOut)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Busiest();
	VT_REQUIRE(Where != 0);

	Run W(AelvorSeed, Where);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 30);

	const Run::Holding H = W.Who(Where);
	const uint32 People = W.Alive(Where);
	const uint32 Capacity = H.Elites * BondageRules{}.MaxHeldPerHolder;
	VAELEN_LOG_INFO(LogHolders,
					"region %u after thirty years: %u alive, %u bound, %u held by a person, %u by the region, "
					"%u elites (capacity %u), fullest holder %u",
					Where, People, H.Bound, H.ByPerson, H.ByRegion, H.Elites, Capacity, H.Fullest);
	VT_CHECK_MSG(People > 0, "the region is peopled");
	VT_CHECK_MSG(H.Fullest <= BondageRules{}.MaxHeldPerHolder, "no holder ever takes more than the rule allows");
	VT_CHECK(H.ByPerson + H.ByRegion == H.Bound);
}

VAELEN_TEST(Holders, AColonyIsFoundedBoundBecauseItCannotGrowThatWay)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Busiest();
	VT_REQUIRE(Where != 0);

	Run W(AelvorSeed, Where);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 30);

	// Thirty years of 05.04 left the region a long way from a colony.
	const HoldingStats Grown = MeasureHolding(W.Instance, W.Persons, W.Bondage, W.Standing, BondageRules{}, Where);
	VT_CHECK_MSG(Grown.Bound * 4 < Grown.People, "bondage on its own rates stays a small part of a region");

	// Founding it makes it one, in a tick.
	VT_CHECK(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	const uint32 Bound =
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where, W.Instance.Now());
	const HoldingStats Made = MeasureHolding(W.Instance, W.Persons, W.Bondage, W.Standing, BondageRules{}, Where);
	VT_CHECK_MSG(Bound > 0, "the founding bound somebody");
	VT_CHECK_MSG(Made.Bound > Grown.Bound * 4, "and a great many more than thirty years of debt ever did");
	// Held by the colony, not by masters with twelve each - which is the only way
	// it can be held at all: the region's elite could not carry this between them.
	VT_CHECK_MSG(Made.ByRegion >= Bound, "the colony holds them");
	VT_CHECK_MSG(Made.Bound > Made.Capacity, "and there were never enough elites to hold this many");
	// The elite are spared, so somebody is left to be an overseer at all.
	VT_CHECK_MSG(Made.Elites > 0, "the elite are not bound with the rest");
	VAELEN_LOG_INFO(LogHolders,
					"region %u: thirty years of debt bound %u of %u; the founding bound %u more, %u by the colony "
					"itself, against a person-holding capacity of only %u",
					Where, Grown.Bound, Grown.People, Bound, Made.ByRegion, Made.Capacity);
}

VAELEN_TEST(Holders, TwoWorldsOfOneSeedAreBoundTheSameWay)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Busiest();
	VT_REQUIRE(Where != 0);

	auto Found = [&](std::vector<uint32>& Who, std::vector<uint32>& Kind) -> HoldingStats
	{
		Run W(AelvorSeed, Where);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		VT_CHECK(W.Promote(Where));
		W.Instance.TickMany(TicksPerYear * 10);
		VT_CHECK(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where, W.Instance.Now());
		for (const Event& E : W.Instance.Log().All())
		{
			if (E.Is(BondEnteredEvent) && E.Get<BondPayload>().Region == Where)
			{
				Who.push_back(E.Get<BondPayload>().Person);
				Kind.push_back(E.Get<BondPayload>().Kind);
			}
		}
		return MeasureHolding(W.Instance, W.Persons, W.Bondage, W.Standing, BondageRules{}, Where);
	};
	std::vector<uint32> WhoA, WhoB, KindA, KindB;
	const HoldingStats A = Found(WhoA, KindA);
	const HoldingStats B = Found(WhoB, KindB);

	VT_CHECK_MSG(!WhoA.empty(), "somebody was bound, so there is something to compare");
	VT_CHECK_EQ(A.Bound, B.Bound);
	VT_CHECK_EQ(A.ByRegion, B.ByRegion);
	VT_CHECK_EQ(A.Enslaved, B.Enslaved);
	VT_CHECK_EQ(WhoA.size(), WhoB.size());
	usize Differ = 0;
	for (usize i = 0; i < WhoA.size() && i < WhoB.size(); ++i)
	{
		Differ += WhoA[i] != WhoB[i] || KindA[i] != KindB[i] ? 1u : 0u;
	}
	VT_CHECK_MSG(Differ == 0, "the same people were bound in the same order to the same condition");
}

VAELEN_TEST(Holders, TheEdgesOfHolding)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Busiest();
	VT_REQUIRE(Where != 0);

	Run W(AelvorSeed, Where);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 5);

	// Ground that is not a colony binds nobody, whatever it is asked.
	VT_CHECK_EQ(
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where, W.Instance.Now()),
		0u);
	VT_CHECK_EQ(
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, 0u, W.Instance.Now()), 0u);

	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	const uint32 First =
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where, W.Instance.Now());
	VT_CHECK_MSG(First > 0, "the founding bound the colony");
	// Asking twice binds nobody twice: 05.04 owns whoever it already holds.
	VT_CHECK_EQ(
		BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where, W.Instance.Now()),
		0u);
	// And a bond is a bond: 05.04 goes on freeing and hardening them as it does
	// anybody else's, so the colony is not a special case once it is made.
	const HoldingStats Made = MeasureHolding(W.Instance, W.Persons, W.Bondage, W.Standing, BondageRules{}, Where);
	W.Instance.TickMany(TicksPerYear * 5);
	const HoldingStats Later = MeasureHolding(W.Instance, W.Persons, W.Bondage, W.Standing, BondageRules{}, Where);
	VT_CHECK_MSG(Later.Bound < Made.Bound, "five years of manumission and flight take some of them back out");
	VAELEN_LOG_INFO(LogHolders, "founded %u bound; five years later %u of them are still bound", Made.Bound,
					Later.Bound);
}

VAELEN_TEST(Holders, WhereEverybodysRankIsTheSameWork)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Busiest();
	VT_REQUIRE(Where != 0);

	Run W(AelvorSeed, Where);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 10);

	// The spread of standing in the region, as the count in each tier and the
	// span of scores. A place where everybody does the same work should be
	// flatter than a place that farms, holds office and inherits.
	auto Spread = [&]()
	{
		struct S
		{
			uint32 Tiers[3] = {};
			uint32 Low = 0xffffffffu;
			uint32 High = 0;
			uint32 Counted = 0;
		} Out;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Region != Where || P.State != static_cast<uint8>(LifeState::Alive))
					{
						return;
					}
					const PersonStanding* St = W.Instance.Components().GetPool(W.Standing.Standing).TryGet(H);
					if (St == nullptr || St->Tier_ > 2)
					{
						return;
					}
					++Out.Tiers[St->Tier_];
					++Out.Counted;
					Out.Low = St->Score < Out.Low ? St->Score : Out.Low;
					Out.High = St->Score > Out.High ? St->Score : Out.High;
				});
		return Out;
	};
	const auto Before = Spread();
	const uint32 AliveBefore = W.Alive(Where);
	VT_REQUIRE(Before.Counted > 0);

	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	VT_REQUIRE(BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where,
						  W.Instance.Now()) > 0);
	// Standing is written yearly; the founding has to be lived through before it
	// shows in anybody's rank.
	W.Instance.TickMany(TicksPerYear * 2);
	const auto After = Spread();

	VAELEN_LOG_INFO(LogHolders, "alive before %u, after %u; bound after %u", AliveBefore, W.Alive(Where),
					MeasureHolding(W.Instance, W.Persons, W.Bondage, W.Standing, BondageRules{}, Where).Bound);
	VAELEN_LOG_INFO(LogHolders, "before: %u common, %u notable, %u elite, scores %u-%u", Before.Tiers[0],
					Before.Tiers[1], Before.Tiers[2], Before.Low, Before.High);
	VAELEN_LOG_INFO(LogHolders, "after:  %u common, %u notable, %u elite, scores %u-%u", After.Tiers[0], After.Tiers[1],
					After.Tiers[2], After.Low, After.High);
	// Nobody died: the region holds the same people it held before.
	VT_CHECK_EQ(W.Alive(Where), AliveBefore);
	// What happened is not that standing FLATTENED. 05.02 takes the rank off
	// anybody bound (Standing.cpp:97) and leaves them out of the ranking
	// (Standing.cpp:158), so in a colony standing very nearly ceases to exist:
	// nine hundred and thirty-five people were ranked here, and eighty-two are.
	// "Everybody's rank is the same work" is literally true in this model - the
	// bound have no rank at all, and the work is all there is.
	VT_CHECK_MSG(After.Counted * 4 < Before.Counted, "almost nobody in a colony has a standing at all");
	VT_CHECK_MSG(After.Counted > 0, "the free few still do");
	// And among those few nothing flattened: the shape of the ranking is what it
	// was. An earlier version of this test checked only that fewer people stood
	// above the common run, which was true for the wrong reason - the bound had
	// left the ranking, and the proportions among the rest had not moved at all.
	const uint32 BeforeAbove = Before.Counted == 0 ? 0u : (Before.Tiers[1] + Before.Tiers[2]) * 1000u / Before.Counted;
	const uint32 AfterAbove = After.Counted == 0 ? 0u : (After.Tiers[1] + After.Tiers[2]) * 1000u / After.Counted;
	const uint32 Moved = BeforeAbove > AfterAbove ? BeforeAbove - AfterAbove : AfterAbove - BeforeAbove;
	VT_CHECK_MSG(Moved < 50, "the proportion standing above the common run is unchanged among the free");
	VAELEN_LOG_INFO(LogHolders, "above the common run: %u per mille of the ranked before, %u after", BeforeAbove,
					AfterAbove);
}

VAELEN_TEST(Holders, TheOverseersAreAnInstitutionAndNotAListOfOwners)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Busiest();
	VT_REQUIRE(Where != 0);

	Run W(AelvorSeed, Where);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 10);

	// Ground that is not a colony raises none.
	VT_CHECK_EQ(RaiseOverseers(W.Instance, W.Ages.Types(), W.Organizations, W.Colony, Where, 24u, W.Instance.Now()),
				0u);

	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	VT_REQUIRE(BindColony(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, W.Standing, W.Colony, Where,
						  W.Instance.Now()) > 0);
	const uint32 Overseers =
		RaiseOverseers(W.Instance, W.Ages.Types(), W.Organizations, W.Colony, Where, 24u, W.Instance.Now());
	VT_CHECK_MSG(Overseers != 0, "the colony has overseers");
	// Raised once and once only.
	VT_CHECK_EQ(RaiseOverseers(W.Instance, W.Ages.Types(), W.Organizations, W.Colony, Where, 24u, W.Instance.Now()),
				0u);

	// The yearly system seats them like anybody else, from the region's unbound
	// adults: nobody is SET to hold a colony they are held by.
	auto Bench = [&]()
	{
		std::pair<uint32, uint32> Out{0u, 0u}; // seated, of them bound
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo&)
				{
					const Membership* M = W.Instance.Components().GetPool(W.Organizations.Member).TryGet(H);
					if (M == nullptr || M->Organization != Overseers)
					{
						return;
					}
					++Out.first;
					Out.second += W.Instance.Components().GetPool(W.Bondage.Bond).TryGet(H) != nullptr ? 1u : 0u;
				});
		return Out;
	};
	W.Instance.TickMany(TicksPerYear);
	const auto Fresh = Bench();
	VT_CHECK_MSG(Fresh.first > 0, "somebody sits among the overseers");
	VT_CHECK_MSG(Fresh.second == 0, "and none of them was chosen while the colony held them");

	// What happens AFTER is 05.04's business, and it is worth saying out loud
	// rather than asserting away: an overseer can fall into debt like anybody
	// else, and 05.01 does not take a seat back when they do. Whether an
	// institution should unseat a member who is bound is a Phase 05 question,
	// not a colony's, so this measures the drift instead of pretending it away.
	W.Instance.TickMany(TicksPerYear * 3);
	const auto Later = Bench();
	VT_CHECK_MSG(Later.second * 4 < Later.first, "the bench stays overwhelmingly free even so");
	VAELEN_LOG_INFO(LogHolders,
					"overseers %u: %u seated with %u held at the first seating, %u with %u held three "
					"years on",
					Overseers, Fresh.first, Fresh.second, Later.first, Later.second);
}
