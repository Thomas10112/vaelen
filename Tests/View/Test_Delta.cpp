// VAELEN - Tests/View
// Phase 13.02: what changed since the last frame.
//
// A delta nobody can replay is a delta nobody should believe. So the claim this
// file is built around is not "the difference is small" - though it is - but
// that applying the difference to the older view gives the newer one back BYTE
// FOR BYTE, checked by digest.
//
// STATUS: PROTOTYPE (Phase 13)

#include "Vaelen/View/Delta.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogDelta);

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

VAELEN_TEST(Delta, ApplyingTheDifferenceGivesTheNewerFrameBack)
{
	// The claim the task rests on. Take a frame, run the world, take another,
	// take the difference, apply it to the FIRST - and the first must now be the
	// second, byte for byte.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView Before;
	TakeView(W.Instance, W.Sources(), Before);
	const Hash64 WasDigest = MeasureView(Before).Digest;

	W.Instance.TickMany(TicksPerYear * 5);
	WorldView After;
	TakeView(W.Instance, W.Sources(), After);
	const ViewStats Truth = MeasureView(After);
	VT_REQUIRE(Truth.Digest != WasDigest);

	ViewDelta D;
	Diff(Before, After, D);
	const DeltaStats S = MeasureDelta(D, After);
	Apply(Before, D);
	const ViewStats Rebuilt = MeasureView(Before);

	VAELEN_LOG_INFO(LogDelta,
					"five years moved %u of %u regions: %u bytes against %u for the whole frame (%u per mille)",
					S.Changed, Truth.Regions, S.Bytes, S.Whole, S.PerMille);
	VT_CHECK_MSG(Rebuilt.Digest == Truth.Digest, "the older frame with the difference applied IS the newer frame");
	VT_CHECK_EQ(Before.Tick, After.Tick);
	VT_CHECK_EQ(Before.Year, After.Year);
	VT_CHECK_EQ(Before.People, After.People);
	VT_CHECK_EQ(Before.Regions.size(), After.Regions.size());
	VT_CHECK_MSG(D.Whole == 0, "and it was a difference, not a replacement");
}

VAELEN_TEST(Delta, ADayCostsFarLessThanAFrame)
{
	// Why the task exists. A world moves a little between two frames, and a
	// renderer should be told about the little rather than re-reading the world.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView Last;
	TakeView(W.Instance, W.Sources(), Last);

	uint32 Frames = 0;
	uint64 DeltaBytes = 0;
	uint64 WholeBytes = 0;
	uint32 Quiet = 0;
	for (uint32 Day = 0; Day < 60; ++Day)
	{
		W.Instance.TickMany(24);
		WorldView Now;
		TakeView(W.Instance, W.Sources(), Now);
		ViewDelta D;
		Diff(Last, Now, D);
		const DeltaStats S = MeasureDelta(D, Now);
		++Frames;
		DeltaBytes += S.Bytes;
		WholeBytes += S.Whole;
		Quiet += S.Changed == 0 ? 1u : 0u;
		// And every one of them must still rebuild the frame exactly.
		Apply(Last, D);
		VT_CHECK(MeasureView(Last).Digest == MeasureView(Now).Digest);
	}
	const uint64 Share = WholeBytes == 0 ? 0u : DeltaBytes * 1000u / WholeBytes;
	VAELEN_LOG_INFO(
		LogDelta,
		"sixty days: %llu bytes of difference against %llu of frames (%llu per mille), %u days when nothing moved",
		static_cast<unsigned long long>(DeltaBytes), static_cast<unsigned long long>(WholeBytes),
		static_cast<unsigned long long>(Share), Quiet);
	VT_CHECK_EQ(Frames, 60u);
	VT_CHECK_MSG(DeltaBytes < WholeBytes, "a day's difference is smaller than a day's frame");
	VT_CHECK_MSG(Quiet > 0, "and on some days the world moved nothing a renderer can see");
}

VAELEN_TEST(Delta, TheFirstFrameOfASessionIsTheWholeThing)
{
	// A renderer that has drawn nothing yet needs everything, and saying so is
	// better than a delta that quietly assumes an empty screen already matches.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView Now;
	TakeView(W.Instance, W.Sources(), Now);

	WorldView Nothing;
	ViewDelta D;
	Diff(Nothing, Now, D);
	VT_CHECK_MSG(D.Whole == 1u, "with nothing on screen, the difference is the whole view");
	VT_CHECK_EQ(D.Changed.size(), Now.Regions.size());
	Apply(Nothing, D);
	VAELEN_LOG_INFO(LogDelta, "from an empty screen: %zu regions carried, whole=%u", D.Changed.size(), D.Whole);
	VT_CHECK_MSG(MeasureView(Nothing).Digest == MeasureView(Now).Digest, "and applying it draws the world");
}

VAELEN_TEST(Delta, GroundThatIsGoneIsSaidToBeGone)
{
	// The case that cannot happen yet and will: a view with fewer regions than
	// the one before it. Built by hand rather than by waiting for a world to
	// lose ground, because the branch exists and an untested branch is a guess.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	WorldView Full;
	TakeView(W.Instance, W.Sources(), Full);
	VT_REQUIRE(Full.Regions.size() > 4);

	WorldView Fewer = Full;
	const uint32 Lost = Fewer.Regions[2].Index;
	Fewer.Regions.erase(Fewer.Regions.begin() + 2);

	ViewDelta D;
	Diff(Full, Fewer, D);
	VAELEN_LOG_INFO(LogDelta, "a region lost: %zu changed, %zu gone, first gone %u", D.Changed.size(), D.Gone.size(),
					D.Gone.empty() ? 0u : D.Gone.front());
	VT_CHECK_EQ(D.Gone.size(), static_cast<usize>(1));
	VT_CHECK_EQ(D.Gone.front(), Lost);
	WorldView Rebuilt = Full;
	Apply(Rebuilt, D);
	VT_CHECK_MSG(MeasureView(Rebuilt).Digest == MeasureView(Fewer).Digest, "and applying it takes the ground away");

	// And the other direction: ground appearing.
	ViewDelta Back;
	Diff(Fewer, Full, Back);
	WorldView Grown = Fewer;
	Apply(Grown, Back);
	VT_CHECK_MSG(MeasureView(Grown).Digest == MeasureView(Full).Digest, "and ground that appears is put back in order");
}
