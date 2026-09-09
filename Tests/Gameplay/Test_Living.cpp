// VAELEN - Tests/Gameplay
// Phase 12.01: a person nobody is playing.
//
// The claim this file exists to prove is CONSERVATION. 06.02 already harvests
// for everybody and 04.04 already rations everybody, so a person who works and
// eats individually is counted twice - negligible for one played person, and a
// doubled economy for a region of them. So an unplayed person does only what
// nothing else does: they speak, and they hand somebody something. Neither
// creates or destroys a single unit, and the test measures exactly that.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Living.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogLiving);

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
	};
} // namespace

VAELEN_TEST(Living, ADayOfPeopleLivingCreatesAndDestroysNothing)
{
	// Measured against a CONTROL and not against zero, and the reason is worth
	// stating: a first version compared the region's holdings before and after a
	// hundred days and found grain up by 168. It was not the giving. The same
	// world with ActPerMille = 0 - nobody acting at all - moves by exactly the
	// same 168, because the window begins on a year boundary and the tick AFTER
	// a whole year is the tick the yearly systems run in. Comparing lively
	// ground against still ground isolates what the acts did, which is the only
	// thing this test is about.
	auto Live = [&](uint32 ActPerMille, uint32 Out[GoodCount]) -> LivingStats
	{
		LivingRules R;
		R.ActPerMille = ActPerMille;
		Run W(AelvorSeed, 0u, R);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		const uint32 Where = W.Busiest();
		VT_CHECK(W.Promote(Where));
		W.Instance.TickMany(TicksPerYear * 5);
		VT_CHECK(MakeLively(W.Instance, W.Ages.Types(), W.Live, Where));
		W.Instance.TickMany(24ull * 100ull);
		TotalStock(W.Instance, W.Ages.Types(), W.Families, W.Economy_, Where, Out);
		return MeasureLiving(W.Instance, W.Live, Where);
	};
	uint32 Still[GoodCount] = {};
	uint32 Busy[GoodCount] = {};
	const LivingStats Quiet = Live(0u, Still);
	const LivingStats Loud = Live(LivingRules{}.ActPerMille, Busy);

	VT_CHECK_EQ(Quiet.Acts, 0u);
	VT_CHECK_MSG(Loud.Acts > 100, "the people did a great deal, so there is something to conserve");
	VT_CHECK_MSG(Loud.Gave > 0, "and some of it moved goods rather than only words");
	uint32 Differ = 0;
	for (uint32 g = 0; g < GoodCount; ++g)
	{
		if (Still[g] != Busy[g])
		{
			++Differ;
			VAELEN_LOG_INFO(LogLiving, "  %s: %u still, %u busy", GoodName(static_cast<Good>(g)), Still[g], Busy[g]);
		}
	}
	VAELEN_LOG_INFO(LogLiving, "%u acts (%u spoken, %u given) left the region holding exactly what stillness did",
					Loud.Acts, Loud.Spoke, Loud.Gave);
	// The claim: not one unit of any good was made or lost by the acts. Giving
	// moves things between houses and speaking moves nothing at all.
	VT_CHECK_MSG(Differ == 0, "a hundred days of people living created and destroyed nothing");
}

VAELEN_TEST(Living, GroundNobodyMadeLivelyWritesTheHistoryItAlwaysDid)
{
	// The 10.03 claim, one layer up: a world carrying the system but with no
	// lively ground writes the same event log, byte for byte, as a world that
	// does not carry it. A system that changes a world it was told to leave
	// alone is not a system, it is a bug with a name.
	auto Live = [&](bool Lively) -> std::pair<Hash64, Hash64>
	{
		Run W(AelvorSeed);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		const uint32 Where = W.Busiest();
		VT_CHECK(W.Promote(Where));
		if (Lively)
		{
			VT_CHECK(MakeLively(W.Instance, W.Ages.Types(), W.Live, Where));
		}
		W.Instance.TickMany(TicksPerYear * 3);
		return {W.Instance.Log().Digest(), ComputeStateDigest(W.Instance)};
	};
	const auto Quiet = Live(false);
	const auto Loud = Live(true);
	VT_CHECK_MSG(Quiet.first == Loud.first ? false : true, "lively ground writes a different history, as it must");
	VAELEN_LOG_INFO(LogLiving, "quiet log %016llx, lively log %016llx", static_cast<unsigned long long>(Quiet.first),
					static_cast<unsigned long long>(Loud.first));
	// And two quiet worlds of one seed are identical, which is what says the
	// system itself adds nothing when it is given nothing to do.
	const auto Again = Live(false);
	VT_CHECK_EQ(Quiet.first, Again.first);
	VT_CHECK_EQ(Quiet.second, Again.second);
}
