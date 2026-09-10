// VAELEN - Tests/View
// Phase 13.04: the view across a snapshot and a reload.
//
// A frame taken across a save must not tear. 03.06 can put a world on disk and
// bring it back, and 13.01 says a view is a pure function of world state - put
// together, those two claims mean a view taken before a save and a view taken
// after the reload have to be the SAME view, and a delta made across the save
// has to still apply.
//
// If that ever failed it would mean the view reads something a snapshot does not
// carry, which is the one way a renderer could show a loaded game that never
// existed.
//
// STATUS: PROTOTYPE (Phase 13)

#include "Vaelen/View/Delta.h"
#include "Vaelen/View/Eye.h"
#include "Vaelen/View/Frame.h"

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::View;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogKeep);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Norms_ = NormTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms_, NormRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms_, Standing, Bondage,
													BondageRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
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
		ViewSources Sources() const
		{
			ViewSources S;
			S.Types = Ages.Types();
			S.Persons = Persons;
			S.HasBondage = true;
			S.Bondage = Bondage;
			S.HasTrade = true;
			S.Trade = Trade;
			return S;
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
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		NormTypes Norms_;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		BondageTypes Bondage;
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
	};
} // namespace

VAELEN_TEST(Keeping, AViewSurvivesASaveAndAReload)
{
	// The claim: a view is a pure function of world state, and a snapshot
	// carries all the state. So the frame before the save and the frame after
	// the reload must be one frame.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Instance.TickMany(TicksPerYear * 3);

	WorldView Before;
	TakeView(W.Instance, W.Sources(), Before);
	const ViewStats Was = MeasureView(Before);

	std::vector<uint8> Bytes;
	SaveSnapshot(W.Instance, Bytes);
	VT_REQUIRE(!Bytes.empty());

	Run Fresh(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(Fresh.Instance, Bytes.data(), Bytes.size()) == SnapshotResult::Ok);
	WorldView After;
	TakeView(Fresh.Instance, Fresh.Sources(), After);
	const ViewStats Now = MeasureView(After);

	VAELEN_LOG_INFO(LogKeep, "%zu bytes of snapshot: %u regions and %u people before, %u and %u after", Bytes.size(),
					Was.Regions, Before.People, Now.Regions, After.People);
	VT_CHECK_MSG(Now.Digest == Was.Digest, "the frame after the reload is the frame before the save");
	VT_CHECK_EQ(After.Tick, Before.Tick);
	VT_CHECK_EQ(After.Year, Before.Year);
	VT_CHECK_EQ(After.People, Before.People);
	VT_CHECK_EQ(After.Width, Before.Width);
	VT_CHECK_EQ(After.Regions.size(), Before.Regions.size());
}

VAELEN_TEST(Keeping, ADeltaMadeAcrossASaveStillApplies)
{
	// The harder half, and the one a renderer actually lives on. Take a frame,
	// save, run the world on, take a frame from the RELOADED world, and the
	// difference between the two must still rebuild the newer one exactly. A
	// renderer holding yesterday's frame does not care that the game was saved
	// in between, and it must not have to.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Instance.TickMany(TicksPerYear * 3);

	WorldView Held;
	TakeView(W.Instance, W.Sources(), Held);

	std::vector<uint8> Bytes;
	SaveSnapshot(W.Instance, Bytes);
	Run Fresh(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(Fresh.Instance, Bytes.data(), Bytes.size()) == SnapshotResult::Ok);
	Fresh.Instance.TickMany(TicksPerYear * 4);

	WorldView Later;
	TakeView(Fresh.Instance, Fresh.Sources(), Later);
	const Hash64 Truth = MeasureView(Later).Digest;

	ViewDelta D;
	Diff(Held, Later, D);
	const DeltaStats S = MeasureDelta(D, Later);
	Apply(Held, D);

	VAELEN_LOG_INFO(LogKeep, "across a save and four years: %u regions changed, %u bytes, whole=%u", S.Changed, S.Bytes,
					D.Whole);
	VT_CHECK_MSG(MeasureView(Held).Digest == Truth,
				 "the frame held from before the save, brought up to date, is right");
	VT_CHECK_MSG(D.Whole == 0u, "and it was a difference, not a replacement - the save did not tear the ground");
}

VAELEN_TEST(Keeping, ASaveAndAReloadDoNotMoveTheEyeEither)
{
	// The same claim for 13.03: a framed view is a pure function of the world
	// AND the eye, so neither half may notice a save.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Instance.TickMany(TicksPerYear);
	const uint32 Middle = W.Busiest();
	VT_REQUIRE(Middle != 0);

	WorldGen::RegionGraphCache Ways;
	Eye At;
	At.Region = Middle;
	At.Reach = 2;
	WorldView Before;
	TakeViewFor(W.Instance, W.Sources(), At, Ways, Before);

	std::vector<uint8> Bytes;
	SaveSnapshot(W.Instance, Bytes);
	Run Fresh(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(Fresh.Instance, Bytes.data(), Bytes.size()) == SnapshotResult::Ok);
	WorldGen::RegionGraphCache Again;
	WorldView After;
	TakeViewFor(Fresh.Instance, Fresh.Sources(), At, Again, After);

	VAELEN_LOG_INFO(LogKeep, "eye on region %u across a save: %zu regions before, %zu after", Middle,
					Before.Regions.size(), After.Regions.size());
	VT_CHECK_MSG(MeasureEye(After, 0).Digest == MeasureEye(Before, 0).Digest,
				 "the same eye on the reloaded world frames the same ground");
	VT_CHECK_EQ(After.People, Before.People);
}

VAELEN_TEST(Keeping, TakingFramesAcrossASaveWritesNothingAnywhere)
{
	// And the whole point of 13.01 again, this time with a save in the middle:
	// neither the world that was saved nor the world that was loaded may have
	// been touched by being looked at.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Instance.TickMany(TicksPerYear * 2);

	std::vector<uint8> Bytes;
	SaveSnapshot(W.Instance, Bytes);
	Run Fresh(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(Fresh.Instance, Bytes.data(), Bytes.size()) == SnapshotResult::Ok);

	const Hash64 SourceBefore = ComputeStateDigest(W.Instance);
	const Hash64 LoadedBefore = ComputeStateDigest(Fresh.Instance);
	WorldView V;
	for (uint32 Frame = 0; Frame < 30; ++Frame)
	{
		TakeView(W.Instance, W.Sources(), V);
		TakeView(Fresh.Instance, Fresh.Sources(), V);
	}
	// And a second save of the world that has been looked at thirty times must
	// be the same bytes as the first.
	std::vector<uint8> AgainBytes;
	SaveSnapshot(W.Instance, AgainBytes);

	VAELEN_LOG_INFO(LogKeep, "thirty frames from each world: %zu bytes of snapshot before, %zu after", Bytes.size(),
					AgainBytes.size());
	VT_CHECK_MSG(ComputeStateDigest(W.Instance) == SourceBefore, "the saved world is untouched");
	VT_CHECK_MSG(ComputeStateDigest(Fresh.Instance) == LoadedBefore, "and so is the loaded one");
	VT_CHECK_MSG(AgainBytes == Bytes, "and it saves to the same bytes it did before anybody looked at it");
}
