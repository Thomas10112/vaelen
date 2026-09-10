// VAELEN - Tests/Colony
// Phase 11.03: the work of a colony - hands on the seams of 02.07, the ore
// credited through the stocks of 06.01 with a cause, the seam running out, and
// a region that eats what it does not grow.
//
// STATUS: PROTOTYPE (Phase 11)

#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
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
using namespace Vaelen::WorldGen;

// Recorded on gcc 13 / Linux x86_64 on 2026-09-09 (11.07): AELVOR 128 at year
// 300, the ore-richest peopled region founded as a colony and lived three years
// at the day, with lives, families, traits, needs, lod, stocks and production.
//
// Re-frozen from 0xa7b00a23e7072fbb, and the two reasons are worth naming.
// 11.06 stopped a mined region reaping nothing, so its people are fed
// differently; and 11.07 replaced LodRules::Held with RequestDetail, which holds
// the region from the tick it is asked rather than through the whole of
// Generate's pre-history. Both change the world this measures, and neither is a
// determinism failure: the same seed still gives the same colony twice over,
// which is what the checks above this one prove.
#define VAELEN_MINING_FROZEN_128 0xe737ebcd1c65e709ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogMining);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, MiningRules InMine = MiningRules{}, ProductionRules InGrow = ProductionRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Economy = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Colony = ColonyTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			// 11.01: the colony is a region the world KEEPS detailed. Without the
			// hold the bridge demotes it on the first crowded year and the hands
			// vanish with it - which is what this test saw before the hold was set.
			// The hold is asked for at a tick by Promote (RequestDetail), not fixed
			// as a rule here: LodRules::Held would apply through the whole of
			// Generate's pre-history, which 11.05 found empties the region.
			LodRules Grain;
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, Grain);
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy, Production,
														 InGrow);
			Rock = std::make_unique<MiningSystem>(Instance, Ages.Types(), Persons, Families, Economy, Colony, InMine);
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Rock->ObserveTraits(Traits.Traits);
			// 06.02 stops reaping a region the moment it is marked mined - not
			// before, which is what makes founding a colony an event in the world
			// rather than a fact of the run.
			Harvest->ObserveMined(Colony.Mined);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
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
		EntityHandle RegionHandle(uint32 Region) const
		{
			EntityHandle Out;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region && Out.IsNull())
						{
							Out = H;
						}
					});
			return Out;
		}
		/// The peopled region with the most ore under it: the one a colony would
		/// be put on, and the same one in every run of the same world.
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
				const RegionPopulation* P = Counts(R);
				if (P == nullptr || P->Total == 0)
				{
					continue;
				}
				if (Ore[R] > Most)
				{
					Most = Ore[R];
					Best = R;
				}
			}
			return Best;
		}
		const RegionPopulation* Counts(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
		}
		uint32 Ore(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			const RegionStock* S = H.IsNull() ? nullptr : Instance.Components().GetPool(Economy.Region).TryGet(H);
			return S == nullptr ? 0u : S->Amount[static_cast<uint32>(Good::Ore)];
		}
		uint32 Grain(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			const RegionStock* S = H.IsNull() ? nullptr : Instance.Components().GetPool(Economy.Region).TryGet(H);
			return S == nullptr ? 0u : S->Amount[static_cast<uint32>(Good::Grain)];
		}
		/// People of a region, alive, at this moment.
		uint32 Alive(uint32 Region) const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach([&](EntityHandle, const PersonInfo& P)
						 { N += P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) ? 1u : 0u; });
			return N;
		}
		/// Makes sure a region is simulated person by person. The hold of 11.01
		/// promotes the colony's region by itself, so a region that already has
		/// people is already where this wants it - asking twice is not a failure.
		bool Promote(uint32 Region)
		{
			RequestDetail(Instance, Lod, Region);
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0 ||
				   Alive(Region) > 0;
		}
		/// Puts grain in a region's common stock, the way a road of 11.06 will.
		/// A colony that reaps nothing needs it, and a mining test run on a
		/// starving colony measures the famine and not the mine.
		uint32 Feed(uint32 Region, uint32 Units)
		{
			return AddStock(Instance, Ages.Types(), Families, Economy, Region, 0u, Good::Grain,
							static_cast<int32>(Units), Instance.Now());
		}
		/// Units of ore ADDED to a region's common stock, read off the log. The
		/// stock itself is no ledger of the mine: 06.02 spends ore on tools, so a
		/// region's holding falls while the mine still gives.
		/// Ore credited to a region's common stock BY THE MINE, told from ore
		/// credited by anything else by the cause each event carries.
		///
		/// It used to sum every StockAdded of ore in the region, on the stated
		/// grounds that "every unit lifted entered the world through AddStock
		/// and through nothing else". That was true only because 06.02's yearly
		/// extraction wrote the common stock directly and told nobody - so this
		/// number was right by the silence of the other source rather than by
		/// its own construction. ADR-0111 gave 06.02 a voice and the sum went
		/// from 957 to 14332 without a single unit of ore moving differently.
		///
		/// The cause is what separates them, and it is why 11.03 insisted on it:
		/// MiningSystem publishes OreLifted and hands that event's id to
		/// AddStock, so the mine's credits are the ones caused by a lift.
		uint64 OreAdded(uint32 Region) const
		{
			std::vector<uint64> Lifts;
			for (const Event& E : Instance.Log().All())
			{
				if (E.Is(Colony::OreLiftedEvent))
				{
					Lifts.push_back(E.Id.Value);
				}
			}
			std::sort(Lifts.begin(), Lifts.end());
			uint64 Sum = 0;
			for (const Event& E : Instance.Log().All())
			{
				if (!E.Is(StockAddedEvent))
				{
					continue;
				}
				const StockPayload& P = E.Get<StockPayload>();
				if (P.Region == Region && P.House == 0 && P.Good == static_cast<uint32>(Good::Ore) &&
					std::binary_search(Lifts.begin(), Lifts.end(), E.Cause.Value))
				{
					Sum += P.Amount;
				}
			}
			return Sum;
		}
		MiningStats Stats(uint32 Region) const { return MeasureMining(Instance, Ages.Types(), Colony, Region); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		EconomyTypes Economy;
		ProductionTypes Production;
		ColonyTypes Colony;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MiningSystem> Rock;
	};
} // namespace

VAELEN_TEST(Mining, AColonyLiftsOreAndTheStockHasExactlyWhatWasLifted)
{
	// A world, then the peopled region with the most ore under it made detailed
	// and founded as a colony.
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Orerichest();
	VT_CHECK_MSG(Where != 0, "AELVOR 128 has a peopled region with ore under it");

	MiningRules Mine;
	Run W(AelvorSeed, Mine);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	VT_CHECK(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	// A second founding on the same ground is refused.
	VT_CHECK(!FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));

	// A colony is supplied; 11.06 gives it the road, this gives it the grain, so
	// that what follows measures the mine and not a famine. Without it the
	// region reaps nothing, loses nine tenths of its people in two years, and
	// the mining numbers become a reading of a collapse.
	W.Feed(Where, 400000u);
	const uint32 Before = W.Ore(Where);
	const uint32 Seam = SeamLeft(W.Instance, W.Ages.Types(), W.Colony, Where);
	VT_CHECK_MSG(Seam > 0, "the colony sits on ore that has not been touched");

	W.Instance.TickMany(TicksPerYear * 2);

	const MiningStats S = W.Stats(Where);
	VT_CHECK_MSG(S.Colonies == 1, "one colony");
	VT_CHECK_MSG(S.Hands > 0, "somebody is on the rock");
	VT_CHECK_MSG(S.Taken > 0, "and the rock gave");
	VT_CHECK_MSG(S.Lifts > 0, "every lift is in the log");
	// Every unit lifted entered the world through AddStock with the lift as its
	// cause, so the log holds exactly what the seams gave. The stock itself is
	// not the measure: 06.02 goes on spending ore on tools, and an earlier
	// version of this test read that spending as ore that never arrived.
	//
	// Nor is "every StockAdded of ore here" the measure, which is what this line
	// used to compare against and what ADR-0111 exposed: 06.02's own yearly
	// extraction credits ore too, and used to do it in silence. See OreAdded.
	VT_CHECK_EQ(W.OreAdded(Where), uint64{S.Taken});
	// Not "richer in ore than before": since 11.06 the colony farms as well as
	// mines, so it has more people, makes more tools, and spends ore faster than
	// two years of a young colony lifts it. What is exact is the log, above.
	(void)Before;
	// And what was taken came out of the seam, unit for unit.
	VT_CHECK_EQ(SeamLeft(W.Instance, W.Ages.Types(), W.Colony, Where), Seam - S.Taken);
	VAELEN_LOG_INFO(LogMining, "colony in region %u: %u hands, %u units lifted of %u in the seam", Where, S.Hands,
					S.Taken, Seam);
}

VAELEN_TEST(Mining, ASeamGivesWhatItHeldAndNotOneUnitMore)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Orerichest();
	VT_REQUIRE(Where != 0);

	// Hands that lift hard, so the seam runs out inside the test rather than
	// inside a century nobody will sit through.
	MiningRules Mine;
	Mine.PerHandPerYear = 4000;
	Run W(AelvorSeed, Mine);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Where));
	VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
	W.Feed(Where, 400000u);
	const uint32 Held = SeamLeft(W.Instance, W.Ages.Types(), W.Colony, Where);

	W.Instance.TickMany(TicksPerYear * 20);

	const MiningStats S = W.Stats(Where);
	VT_CHECK_MSG(S.Taken == Held, "the seams gave everything they held and no more");
	VT_CHECK_MSG(S.WorkedOut == S.Seams, "every seam is worked out");
	VT_CHECK_EQ(SeamLeft(W.Instance, W.Ages.Types(), W.Colony, Where), 0u);
	// Worked out means worked out: another decade lifts nothing. The claim is
	// about the LIFTS, not about the stock - 06.02 goes on spending ore on tools
	// whether the seam gives or not, and an earlier version of this test read
	// that spending as a failure of the mine.
	const uint32 Lifts = S.Lifts;
	W.Instance.TickMany(TicksPerYear * 10);
	VT_CHECK_EQ(W.Stats(Where).Taken, Held);
	VT_CHECK_MSG(W.Stats(Where).Lifts == Lifts, "not one more lift out of a seam that has nothing left");
	VAELEN_LOG_INFO(LogMining, "%u seams worked out, %u units in all, then ten years of nothing", S.Seams, Held);
}

VAELEN_TEST(Mining, TwoWorldsOfOneSeedLiftTheSameOreOnTheSameDays)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Orerichest();
	VT_REQUIRE(Where != 0);

	auto Live = [&](std::vector<uint64>& Days, std::vector<uint32>& Units) -> MiningStats
	{
		MiningRules Mine;
		Run W(AelvorSeed, Mine);
		VT_CHECK(W.Ages.Generate(Run::Square(128), 300));
		VT_CHECK(W.Promote(Where));
		VT_CHECK(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
		W.Feed(Where, 400000u);
		W.Instance.TickMany(TicksPerYear * 3);
		for (const Event& E : W.Instance.Log().All())
		{
			if (E.Is(OreLiftedEvent))
			{
				Days.push_back(E.Tick);
				Units.push_back(E.Get<StockPayload>().Amount);
			}
		}
		return W.Stats(Where);
	};
	std::vector<uint64> DaysA, DaysB;
	std::vector<uint32> UnitsA, UnitsB;
	const MiningStats A = Live(DaysA, UnitsA);
	const MiningStats B = Live(DaysB, UnitsB);

	VT_CHECK_MSG(!DaysA.empty(), "the colony lifted something to compare");
	VT_CHECK_EQ(A.Digest, B.Digest);
	VT_CHECK_EQ(A.Taken, B.Taken);
	VT_CHECK_EQ(DaysA.size(), DaysB.size());
	VT_CHECK_EQ(UnitsA.size(), UnitsB.size());
	usize Differ = 0;
	for (usize i = 0; i < DaysA.size() && i < DaysB.size(); ++i)
	{
		Differ += DaysA[i] != DaysB[i] || UnitsA[i] != UnitsB[i] ? 1u : 0u;
	}
	VT_CHECK_MSG(Differ == 0, "every lift fell on the same tick for the same units in both worlds");
	// And the same seed gives the same colony across builds, not merely across
	// two runs of one build.
	VT_CHECK_EQ(A.Digest, Hash64{VAELEN_MINING_FROZEN_128});
	VAELEN_LOG_INFO(LogMining, "%zu lifts over three years, identical in two worlds, digest %016llx", DaysA.size(),
					static_cast<unsigned long long>(A.Digest));
}

VAELEN_TEST(Mining, AColonyEatsWhatItDoesNotGrow)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Orerichest();
	VT_REQUIRE(Where != 0);

	// The same world twice: once left to farm, once founded as a colony. Only
	// the founding differs, so what separates them is what a colony costs.
	MiningRules Mine;

	Run Farm(AelvorSeed, Mine);
	VT_REQUIRE(Farm.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Farm.Promote(Where));
	Farm.Feed(Where, 40000u);
	const uint32 FarmBefore = Farm.Grain(Where);

	Run Pit(AelvorSeed, Mine);
	VT_REQUIRE(Pit.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Pit.Promote(Where));
	VT_REQUIRE(FoundColony(Pit.Instance, Pit.Ages.Types(), Pit.Colony, Where));
	Pit.Feed(Where, 40000u);
	const uint32 PitBefore = Pit.Grain(Where);
	VT_CHECK_EQ(FarmBefore, PitBefore); // the two worlds start level

	Farm.Instance.TickMany(TicksPerYear * 5);
	Pit.Instance.TickMany(TicksPerYear * 5);

	// Since 11.06 the colony farms with whoever it has not bound, so the gap in
	// grain has closed almost to nothing - which is the correction, not a
	// failure. What still separates the two grounds is the ore: only the colony
	// lifts any.
	VT_CHECK_MSG(Pit.Grain(Where) < PitBefore, "the colony's grain falls: it eats what it does not grow");
	VT_CHECK_MSG(Pit.Grain(Where) > 0, "and the colony still has grain, because it still farms");
	// The ore is the other side of the bargain: the pit has it, the farm does
	// not lift any beyond what 06.02's yearly extraction gives everybody.
	VT_CHECK_MSG(Pit.Stats(Where).Taken > 0, "the colony lifted ore");
	VT_CHECK_EQ(Farm.Stats(Where).Taken, 0u);
	VAELEN_LOG_INFO(LogMining, "five years on the same ground: farm grain %u, colony grain %u (from %u), ore lifted %u",
					Farm.Grain(Where), Pit.Grain(Where), PitBefore, Pit.Stats(Where).Taken);
}

VAELEN_TEST(Mining, TheEdgesOfAColony)
{
	Run Probe(AelvorSeed);
	VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = Probe.Orerichest();
	VT_REQUIRE(Where != 0);

	// A colony nobody founded lifts nothing, however the rules are set.
	{
		MiningRules Mine;
		Run W(AelvorSeed, Mine);
		VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
		VT_REQUIRE(W.Promote(Where));
		W.Instance.TickMany(TicksPerYear);
		VT_CHECK_EQ(W.Stats(Where).Taken, 0u);
		VT_CHECK_EQ(W.Stats(Where).Colonies, 0u);
	}
	// A founding on ground that is not there is refused, and changes nothing.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
		VT_CHECK(!FoundColony(W.Instance, W.Ages.Types(), W.Colony, 0u));
		VT_CHECK(!FoundColony(W.Instance, W.Ages.Types(), W.Colony, 999999u));
	}
	// Hands that lift nothing at all lift nothing: the rule is honoured, not
	// rounded up to one.
	{
		MiningRules Mine;
		Mine.PerHandPerYear = 0;
		Run W(AelvorSeed, Mine);
		VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
		VT_REQUIRE(W.Promote(Where));
		VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
		W.Feed(Where, 40000u);
		W.Instance.TickMany(TicksPerYear);
		const MiningStats S = W.Stats(Where);
		VT_CHECK_MSG(S.Hands > 0, "there are hands");
		VT_CHECK_MSG(S.Taken == 0, "and they lift nothing, because the rule says a year of them is nothing");
	}
	// Nobody old enough: hands are counted from an age, and an age nobody has
	// reached leaves the rock alone.
	{
		MiningRules Mine;
		Mine.WorkFromAge = 250;
		Run W(AelvorSeed, Mine);
		VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
		VT_REQUIRE(W.Promote(Where));
		VT_REQUIRE(FoundColony(W.Instance, W.Ages.Types(), W.Colony, Where));
		W.Feed(Where, 40000u);
		W.Instance.TickMany(TicksPerYear);
		const MiningStats S = W.Stats(Where);
		VT_CHECK_EQ(S.Hands, 0u);
		VT_CHECK_EQ(S.Taken, 0u);
	}
}
