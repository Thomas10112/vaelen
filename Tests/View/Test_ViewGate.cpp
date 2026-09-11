// VAELEN - Tests/View
// Phase 13.05: the Phase 13 kernel gate.
//
// A century at 256, and then a year of it watched a day at a time: every frame
// taken, the difference from the frame before worked out, applied to a running
// screen - and the screen compared against a frame taken fresh, EVERY DAY. Not
// once at the end, because a delta that is right 359 times and wrong once is a
// renderer that shows the wrong world on the 360th day and never recovers.
//
// ADR-0095 is why the volumes are asserted before anything else is believed. The
// Phase 11 gate passed its first run on 1081 intents instead of 14400, and the
// Phase 12 gate passed its first run with the whole phase measuring zero. A gate
// that watches a year of a world in which nothing moves would be the third.
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
#include <chrono>
#include <type_traits>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::View;
using namespace Vaelen::WorldGen;

// Recorded on gcc 13 / Linux x86_64 on 2026-09-10 (13.05): AELVOR 256 at year
// 300, a century more with the busiest region simulated person by person, then
// a year watched a day at a time - 360 frames, the screen kept up to date by
// nothing but deltas and checked against a fresh frame every day.
#define VAELEN_VIEWGATE_FROZEN_VIEW 0x115c2ff70a5327c4ull
#define VAELEN_VIEWGATE_FROZEN_STATE 0x5bc8478689958433ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogGate);

	constexpr uint32 BeforeYears = 100; ///< a century of the world moving before anybody looks
	constexpr uint32 WatchedDays = 360; ///< then a year of it, a day at a time

	// The frozen digests live at file scope as macros, not as constexpr values in
	// the namespace, because every other gate in this project does - and because
	// MSVC is right about why. `if (FrozenView != 0)` on a constexpr is C4127,
	// "conditional expression is constant", and the Windows leg treats warnings
	// as errors. The seven gates before this one all use `#if`; deviating from
	// the idiom cost a red build and taught nothing else.

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

VAELEN_TEST(ViewGate, ACenturyWatchedADayAtATimeRebuildsExactlyEveryFrame)
{
	const auto Start = std::chrono::steady_clock::now();
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(256), 300));
	const uint32 Middle = W.Busiest();
	VT_REQUIRE(Middle != 0);
	VT_REQUIRE(RequestDetail(W.Instance, W.Lod, Middle));
	VT_REQUIRE(PromoteRegion(W.Instance, W.Ages.Types(), W.Persons, MaterialiseRules{}, Middle, W.Instance.Now()) > 0);

	// A century of the world moving with nobody watching, so what the year below
	// sees is a world with a history and not a world just made.
	uint32 Failures = 0;
	for (uint32 Year = 1; Year <= BeforeYears; ++Year)
	{
		W.Instance.TickMany(TicksPerYear);
		if (Year % 10 != 0)
		{
			continue;
		}
		WorldView Decade;
		TakeView(W.Instance, W.Sources(), Decade);
		const ViewStats S = MeasureView(Decade);
		// The invariants of this layer, every decade.
		const bool Ordered = [&]
		{
			for (usize i = 1; i < Decade.Regions.size(); ++i)
			{
				if (Decade.Regions[i - 1].Index >= Decade.Regions[i].Index)
				{
					return false;
				}
			}
			return true;
		}();
		uint32 Counted = 0;
		for (const RegionView& R : Decade.Regions)
		{
			Counted += R.People;
		}
		Failures += Ordered ? 0u : 1u;
		VT_CHECK_MSG(Ordered, "the regions are in index order, every decade of the century");
		Failures += Counted == Decade.People ? 0u : 1u;
		VT_CHECK_MSG(Counted == Decade.People, "and the frame's head count is the sum of its regions'");
		Failures += S.Detailed <= S.Regions ? 0u : 1u;
		VT_CHECK_MSG(S.Detailed <= S.Regions, "no more ground is simulated finely than there is ground");
		VAELEN_LOG_INFO(LogGate, "year %u: %u regions (%u peopled, %u detailed), %u people, %u bytes", 300 + Year,
						S.Regions, S.Peopled, S.Detailed, Decade.People, S.Bytes);
	}

	// The year somebody watches. A running screen, brought up to date by nothing
	// but deltas, checked against a fresh frame every single day.
	WorldView Screen;
	TakeView(W.Instance, W.Sources(), Screen);
	const Hash64 FirstFrame = MeasureView(Screen).Digest;

	uint32 Frames = 0;
	uint32 Moved = 0;
	uint32 Torn = 0;
	uint64 DeltaBytes = 0;
	uint64 FrameBytes = 0;
	uint32 MostChanged = 0;
	for (uint32 Day = 0; Day < WatchedDays; ++Day)
	{
		W.Instance.TickMany(24);
		WorldView Fresh;
		TakeView(W.Instance, W.Sources(), Fresh);
		ViewDelta D;
		Diff(Screen, Fresh, D);
		const DeltaStats S = MeasureDelta(D, Fresh);
		Apply(Screen, D);
		++Frames;
		Moved += S.Changed != 0 ? 1u : 0u;
		MostChanged = S.Changed > MostChanged ? S.Changed : MostChanged;
		DeltaBytes += S.Bytes;
		FrameBytes += S.Whole;
		if (MeasureView(Screen).Digest != MeasureView(Fresh).Digest)
		{
			++Torn;
		}
	}
	const Hash64 LastFrame = MeasureView(Screen).Digest;
	const Hash64 State = ComputeStateDigest(W.Instance);
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();

	// ADR-0095: the volume, before the verdict.
	VAELEN_LOG_INFO(LogGate,
					"a century and a watched year in %.1f s: %u frames, %u of them carrying anything, "
					"%u regions at the most; %llu bytes of delta against %llu of frames",
					Seconds, Frames, Moved, MostChanged, static_cast<unsigned long long>(DeltaBytes),
					static_cast<unsigned long long>(FrameBytes));
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK_EQ(Frames, WatchedDays);
	VT_CHECK_MSG(Moved > 0, "the world moved during the year that was watched, which is what makes this a gate");
	VT_CHECK_MSG(MostChanged > 0, "and at least one frame carried real ground");
	VT_CHECK_MSG(DeltaBytes < FrameBytes, "and watching by difference cost less than watching by re-reading");
	VT_CHECK_MSG(Torn == 0, "the screen matched a fresh frame on every one of the 360 days, not just the last");
	VT_CHECK_MSG(LastFrame != FirstFrame, "and a year of a world is visible in it");

	// Taken fresh at the end, with no deltas anywhere: the screen built entirely
	// out of differences must equal it.
	WorldView Final;
	TakeView(W.Instance, W.Sources(), Final);
	VT_CHECK_MSG(MeasureView(Final).Digest == LastFrame,
				 "a screen built out of nothing but differences is the world taken whole");

	VAELEN_LOG_INFO(LogGate, "frozen: view=%016llx state=%016llx", static_cast<unsigned long long>(LastFrame),
					static_cast<unsigned long long>(State));
#if VAELEN_VIEWGATE_FROZEN_VIEW != 0x0ull
	VT_CHECK_EQ(LastFrame, Hash64{VAELEN_VIEWGATE_FROZEN_VIEW});
	VT_CHECK_EQ(State, Hash64{VAELEN_VIEWGATE_FROZEN_STATE});
#endif
}
