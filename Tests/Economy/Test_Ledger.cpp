// VAELEN - Tests/Economy
// What the log can and cannot account for.
//
// 11.03 established the rule and paid for it: the ore of a colony is credited
// through Economy::AddStock and nothing else, "so that the chronicle can say
// where it came from". 01.05 built the causal edge and 03.07 answers "why did
// this happen" by walking it. Both work only on what the log holds.
//
// This file asks a question nobody had asked: of everything that moves in a
// region's stores in a year, how much of it does the log actually explain? It
// is here because 12.01 could not answer it - a conservation test had to be
// written against a control world rather than against the log, because the log
// could not account for the grain.
//
// It measures rather than asserts a target, in the manner of the mini-world
// baseline. The number is the finding.
//
// STATUS: PROTOTYPE

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.06): AELVOR 64 at
// year 120, three regions detailed in turn every 25 years for 500 years.
#define VAELEN_GRAINS_STILL_64 0xace29fbb38633cbfull
// Refrozen 2026-09-08: person indices are taken from a counter that only
// goes up, so a demoted region no longer hands its indices out again (see
// PersonCounter).
#define VAELEN_GRAINS_LIVING_64 0x477efde8a0ccb23dull
#define VAELEN_GRAINS_PROMOTIONS_64 21u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLedger);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, bool Living = true) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Economy = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Wealth = WealthTypes::Declare(Instance);
			Stores = Instance.Types().Register<RegionStores>("RegionStores"); // a council's granary (05.05)
			Instance.Components().CreatePool(Stores);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy, Production,
														 ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Purses = std::make_unique<WealthSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Norms,
													Wealth, WealthRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Ranks->RunAfter("Wealth");
			Ranks->ObserveWealth(Wealth.Wealth);
			if (Living)
			{
				Stocks->ObserveHeirs(Wealth.Heir);
			}
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveStores(Stores);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			// A still world holds only the owners of goods: nothing is made, eaten,
			// priced, carried or inherited, so the world's whole stock can only be
			// moved between a region's commons and its houses.
			if (Living)
			{
				Instance.Systems().Add(Minds.get());
				Instance.Systems().Add(Body.get());
				Instance.Systems().Add(Harvest.get());
				Instance.Systems().Add(Fair.get());
				Instance.Systems().Add(Roads.get());
				Instance.Systems().Add(Orgs.get());
				Instance.Systems().Add(Customs.get());
				Instance.Systems().Add(Purses.get());
				Instance.Systems().Add(Ranks.get());
			}
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
		const RegionPopulation* Counts(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
		}
		WealthStats Stats(uint32 Region = 0) const { return MeasureWealth(Instance, Families, Wealth, Region); }
		TradeStats Roads_() const { return MeasureTrade(Instance, Ages.Types(), Trade, TradeRules{}); }
		MarketStats Fairs() const { return MeasureMarkets(Instance, Ages.Types(), Markets, 0); }
		ProductionStats Fields() const { return MeasureProduction(Instance, Ages.Types(), Production, 0); }
		/// The whole world's goods: every region's commons and its houses.
		void WorldStock(uint64 Out[GoodCount]) const
		{
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				Out[g] = 0;
			}
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle, const RegionInfo& R)
					{
						uint32 Here[GoodCount];
						TotalStock(Instance, Ages.Types(), Families, Economy, R.Index, Here);
						for (uint32 g = 0; g < GoodCount; ++g)
						{
							Out[g] += Here[g];
						}
					});
		}
		/// House stocks held by a house that is extinct or whose region is coarse.
		uint32 Orphans() const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle H, const FamilyInfo& F)
					{
						if (Instance.Components().GetPool(Economy.House).TryGet(H) == nullptr)
						{
							return;
						}
						N += F.Extinct != 0 || !IsDetailed(Instance, Ages.Types(), Persons, F.Region) ? 1u : 0u;
					});
			return N;
		}
		std::vector<uint32> Ranked() const
		{
			std::vector<std::pair<uint32, uint32>> All;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && P->Total > 0)
						{
							All.push_back({P->Total, R.Index});
						}
					});
			std::sort(All.begin(), All.end(), [](const auto& A, const auto& B)
					  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
			std::vector<uint32> Out;
			for (const auto& [People, Index] : All)
			{
				Out.push_back(Index);
			}
			return Out;
		}
		void Total(uint32 Region, uint32 Out[GoodCount]) const
		{
			TotalStock(Instance, Ages.Types(), Families, Economy, Region, Out);
		}
		const Society::HouseWealth* Purse(uint32 Family) const { return WealthOf(Instance, Families, Wealth, Family); }
		const HouseHeir* Heir(uint32 Family) const { return HeirOf(Instance, Families, Wealth, Family); }
		const FamilyInfo* House(uint32 Family) const
		{
			const FamilyInfo* Found = nullptr;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle, const FamilyInfo& F)
					{
						if (F.Index == Family && Found == nullptr)
						{
							Found = &F;
						}
					});
			return Found;
		}
		/// Living houses of a region, in index order.
		std::vector<uint32> HousesOf(uint32 Region) const
		{
			std::vector<uint32> Out;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle, const FamilyInfo& F)
					{
						if (F.Region == Region && F.Extinct == 0)
						{
							Out.push_back(F.Index);
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		}
		/// Every culture's descent custom set to one line.
		void DescendBy(Descent Line)
		{
			Instance.Components()
				.GetPool(Ages.Types().Population.Culture)
				.ForEach(
					[&](EntityHandle, const CultureInfo& C)
					{
						const NormSet* N = NormsOf(Instance, Ages.Types(), Norms, C.Index);
						if (N != nullptr)
						{
							NormSet Copy = *N;
							Copy.Descent_ = static_cast<uint32>(Line);
							SetNorms(Instance, Ages.Types(), Norms, C.Index, Copy);
						}
					});
		}
		/// Every good of every region's common stock set to Amount (a flood, or a drain).
		void FillEvery(int32 Delta)
		{
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle, const RegionInfo& R)
					{
						for (uint32 g = 0; g < GoodCount; ++g)
						{
							AddStock(Instance, Ages.Types(), Families, Economy, R.Index, 0, static_cast<Good>(g), Delta,
									 Instance.Now());
						}
					});
		}
		const RegionMarket* Market(uint32 Region) const { return MarketOf(Instance, Ages.Types(), Markets, Region); }
		uint32 Price(uint32 Region, Good G) const
		{
			const RegionMarket* M = Market(Region);
			return M != nullptr ? M->Price[static_cast<uint32>(G)] : 0u;
		}
		StockStats Stock(uint32 Region = 0) const
		{
			return MeasureStocks(Instance, Ages.Types(), Persons, Families, Economy, Region);
		}
		/// Units harvested in a region during the last Years years.
		uint64 Harvested(uint32 Region, uint32 Years) const
		{
			uint64 Sum = 0;
			const std::vector<Event>& All = Instance.Log().All();
			for (usize i = All.size(); i > 0; --i)
			{
				const Event& E = All[i - 1];
				if (E.Tick + uint64{TicksPerYear} * Years < Instance.Now())
				{
					break;
				}
				if (E.Is(HarvestEvent) && E.Get<StockPayload>().Region == Region)
				{
					Sum += E.Get<StockPayload>().Amount;
				}
			}
			return Sum;
		}
		bool Curse(uint32 Region, DisasterKind Kind)
		{
			bool Queued = false;
			Instance.Components()
				.GetPool(Ages.Types().Disasters.State)
				.ForEach(
					[&](EntityHandle, DisasterState& S)
					{
						if (!Queued && S.PendingCount < DisasterState::MaxPending)
						{
							S.Pending[S.PendingCount] = PendingOmen{Region, static_cast<uint32>(Kind), 1000, 0, 0};
							++S.PendingCount;
							Queued = true;
						}
					});
			return Queued;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		EconomyTypes Economy;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		WealthTypes Wealth;
		ComponentType<RegionStores> Stores;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<WealthSystem> Purses;
		std::unique_ptr<StandingSystem> Ranks;
	};

} // namespace

VAELEN_TEST(Ledger, HowMuchOfAYearTheLogCanAccountFor)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	// The busiest region, because a region with nobody in it moves nothing and
	// a ledger of nothing balances perfectly.
	uint32 Where = 0;
	uint32 Most = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionPopulation* P =
					W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				if (P != nullptr && (P->Total > Most || (P->Total == Most && R.Index < Where)))
				{
					Most = P->Total;
					Where = R.Index;
				}
			});
	VT_REQUIRE(Where != 0);

	// Everything the region holds: the common stock and every house standing in
	// it. A ledger that counted only the common stock would call a meal eaten
	// out of a family's own granary a disappearance.
	const auto Holdings = [&]()
	{
		std::array<int64, static_cast<usize>(Good::Count)> Out{};
		const RegionStock* S = StockOf(W.Instance, W.Ages.Types(), W.Economy, Where);
		if (S != nullptr)
		{
			for (usize g = 0; g < Out.size(); ++g)
			{
				Out[g] += static_cast<int64>(S->Amount[g]);
			}
		}
		W.Instance.Components()
			.GetPool(W.Families.Family)
			.ForEach(
				[&](EntityHandle H, const FamilyInfo& F)
				{
					if (F.Region != Where)
					{
						return;
					}
					const HouseStock* HS = W.Instance.Components().GetPool(W.Economy.House).TryGet(H);
					if (HS == nullptr)
					{
						return;
					}
					for (usize g = 0; g < Out.size(); ++g)
					{
						Out[g] += static_cast<int64>(HS->Amount[g]);
					}
				});
		return Out;
	};

	// Settle, and then step ONE tick off the year boundary before measuring.
	//
	// That single tick is the whole difference between a measurement and a
	// mirage, and this project has now paid for the same lesson four times. The
	// yearly systems run ON the boundary tick, so a window (From, To] whose From
	// sits on one excludes that year's harvest while the Before snapshot was
	// taken after it had already run - the window then contains no yearly pass
	// at all and every good looks unaccounted for. Measured that way this test
	// first reported "0% explained", which was true of the window and false of
	// the world.
	W.Instance.TickMany(TicksPerYear * 3 + 1);
	const auto Before = Holdings();
	const uint64 From = static_cast<uint64>(W.Instance.Now());
	W.Instance.TickMany(TicksPerYear);
	const auto After = Holdings();
	const uint64 To = static_cast<uint64>(W.Instance.Now());

	// What the log says moved in or out of this region during that year.
	std::array<int64, static_cast<usize>(Good::Count)> Explained{};
	uint32 Events = 0;
	uint64 Carried = 0; ///< units 06.04 moved in or out, of no good in particular
	for (const Event& E : W.Instance.Log().All())
	{
		const uint64 At = static_cast<uint64>(E.Tick);
		if (At <= From || At > To)
		{
			continue;
		}
		if (E.Is(HarvestEvent) || E.Is(StockAddedEvent) || E.Is(StockTakenEvent))
		{
			const StockPayload& P = E.Get<StockPayload>();
			if (P.Region != Where || P.Good >= Explained.size())
			{
				continue;
			}
			++Events;
			// Harvest and StockAdded put goods in; StockTaken takes them out.
			// AddStock's own two events already cover a move between a house and
			// the common stock as a pair, so they cancel and that is right.
			Explained[P.Good] += E.Is(StockTakenEvent) ? -static_cast<int64>(P.Amount) : static_cast<int64>(P.Amount);
			continue;
		}
		if (E.Is(GoodsCarriedEvent))
		{
			++Events;
			// ADR-0131 put the good in the payload and made From and To the
			// seller and the buyer, so these finally go where they belong. The
			// comment that stood here said they could not, and listed exactly
			// what was missing; both halves of that list are now present.
			const TradePayload& P = E.Get<TradePayload>();
			if (P.Good >= Explained.size())
			{
				continue;
			}
			if (P.From == Where)
			{
				Explained[P.Good] -= static_cast<int64>(P.Amount);
				Carried += P.Amount;
			}
			else if (P.To == Where)
			{
				Explained[P.Good] += static_cast<int64>(P.Amount);
				Carried += P.Amount;
			}
		}
	}

	int64 Logged = 0;
	int64 Dark = 0;
	std::array<int64, static_cast<usize>(Good::Count)> Missing{};
	for (usize g = 0; g < Explained.size(); ++g)
	{
		const int64 Net = After[g] - Before[g];
		const int64 Miss = Net - Explained[g]; // what moved that no event names
		Missing[g] = Miss < 0 ? -Miss : Miss;
		Logged += Explained[g] < 0 ? -Explained[g] : Explained[g];
		Dark += Miss < 0 ? -Miss : Miss;
		if (Net != 0 || Explained[g] != 0)
		{
			VAELEN_LOG_INFO(LogLedger, "  %-8s the stores end %+lld; the log names %+lld; %lld units moved unnamed",
							GoodName(static_cast<Good>(g)), static_cast<long long>(Net),
							static_cast<long long>(Explained[g]), static_cast<long long>(Miss < 0 ? -Miss : Miss));
		}
	}
	VAELEN_LOG_INFO(LogLedger,
					"one year of region %u (%u people): the log names %lld units in %u events, and %lld units moved "
					"that it does not name at all",
					Where, Most, static_cast<long long>(Logged), Events, static_cast<long long>(Dark));

	// Diagnostic: what stock-shaped events exist in the whole log, anywhere.
	uint32 Harvests = 0;
	uint32 AddedAll = 0;
	uint32 TakenAll = 0;
	uint32 HarvestsHere = 0;
	uint64 FirstHarvestTick = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(HarvestEvent))
		{
			++Harvests;
			const StockPayload& P = E.Get<StockPayload>();
			if (P.Region == Where)
			{
				++HarvestsHere;
				FirstHarvestTick = static_cast<uint64>(E.Tick); // keeps the LAST one, which is what matters here
			}
		}
		AddedAll += E.Is(StockAddedEvent) ? 1u : 0u;
		TakenAll += E.Is(StockTakenEvent) ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogLedger,
					"whole log: %u harvests (%u here, LAST at tick %llu), %u added, %u taken; window (%llu, %llu]",
					Harvests, HarvestsHere, static_cast<unsigned long long>(FirstHarvestTick), AddedAll, TakenAll,
					static_cast<unsigned long long>(From), static_cast<unsigned long long>(To));

	VT_CHECK_MSG(Logged > 0, "the log names SOMETHING, or the window missed the year's pass entirely");
	VT_CHECK_MSG(HarvestsHere > 0, "and the region did harvest during it");

	// THAT DAY CAME TWICE. This block held two assertions with a paragraph
	// between them explaining why one line could never reach zero. Both halves
	// of that explanation are now gone, and each went the same way: not by
	// arguing with the test, but by making the log say the thing it could not.
	//
	//   ADR-0111   06.02 publishes its spoilage and its meals
	//     before   grain  the stores end +6; the log names +7329; 7323 unnamed
	//     after    grain  the stores end +6; the log names    +6;    0 unnamed
	//
	//   ADR-0131   GoodsCarried names the good it carried
	//     before   ore    the stores end +0; the log names   -37;   37 unnamed
	//     after    ore    the stores end +0; the log names    +0;    0 unnamed
	//
	// So the line that stood here - "what is still unnamed is what 06.04
	// carried, because GoodsCarried has no Good in it" - was true when it was
	// written and is false now. It asserted `Dark > 0`. Keeping it would have
	// meant asserting that the world still lies about something, which is not a
	// property worth defending; inverting it is the honest move, and it is the
	// stronger claim of the two.
	//
	// A region's ledger closes. Everything that entered or left its stores in a
	// year is named by some event, to the unit: what it grew, ate, spoilt,
	// burnt, wove, forged and wore out (06.02), what it dug (05.03), and now
	// what crossed its roads in either direction and which good it was (06.04).
	//
	// `Carried > 0` is what keeps this from being vacuous. A region that never
	// trades would close its books trivially, and the second half of the claim
	// would be untested. This one trades.
	VT_CHECK_MSG(Dark == 0, "every unit that entered or left this region's stores in a year is named by the log");
	VT_CHECK_MSG(Carried > 0, "and that is a finding, not an accident, because this region actually trades");
}
