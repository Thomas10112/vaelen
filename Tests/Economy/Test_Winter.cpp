// VAELEN - Tests/Economy
// Phase 18.06: the winter, yearly, as a consequence - fuel through the
// ledger, grain from the stores, chill on the people, deaths where nobody is
// detailed, and the events the chronicle puts into words.
//
// STATUS: PROTOTYPE (Phase 18)
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Winter.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Population/Warmth.h"
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWinter);
	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// What a run is asked for.
	struct Ask
	{
		bool WithWinter = true;
		WinterRules Winter;
		/// The line at frozen through and no yearly recovery: the chill a
		/// winter puts on a person stays where the test can read it and kills
		/// nobody, so the cases are about the winter and not the judgement.
		WarmthRules Warmth{255u, 40u, 0u};
	};

	/// The economy's wiring with the warmth and, when asked, the winter; the
	/// markets and roads are declared (the chronicle names things through
	/// them) but not run, so nothing founds a settlement unless a case does.
	struct Run
	{
		explicit Run(uint64 Seed, const Ask& A) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Warmth = WarmthTypes::Declare(Instance);
			Chronicle = EconomyChronicleTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Context = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}};
			Scribe =
				std::make_unique<EconomyChronicle>(Instance, Ages.Types(), Context, Chronicle, EconomyChronicleRules{});
			Scribe->Attach();
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Body->ObserveWinter(Warmth, A.Warmth);
			if (A.WithWinter)
			{
				Winters = std::make_unique<WinterSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Warmth,
														 A.Winter);
				Winters->RunAfter("Stocks");
				Winters->ObserveSettlements(Trade.Settlement);
				Harvest->RunAfter("Winter");
			}
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			if (Winters)
			{
				Instance.Systems().Add(Winters.get());
			}
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
		const PreHistoryTypes& Types() const { return Ages.Types(); }
		/// The year the next turn judges: the turn at tick T is year T / 8640,
		/// and the winter it judges is the year before.
		uint64 JudgedYear() const { return Instance.Now() / TicksPerYear - 1u; }
		EntityHandle RegionHandle(uint32 Region) const
		{
			EntityHandle Out;
			Instance.Components()
				.GetPool(Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region)
						{
							Out = H;
						}
					});
			return Out;
		}
		uint32 RegionCount() const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Types().World.RegionTypes_.Region)
				.ForEach([&](EntityHandle, const RegionInfo& R) { N = std::max(N, R.Index); });
			return N + 1u;
		}
		const RegionPopulation* Counts(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Types().Population.Population).TryGet(H);
		}
		const RegionStock* Common(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Economy_.Region).TryGet(H);
		}
		/// The peopled region with the largest cold sum (Cold) or the smallest
		/// (Warm) in the year the next turn judges; ties to the lower index.
		uint32 Extreme(bool Cold) const
		{
			uint32 Best = 0;
			int32 BestSum = Cold ? -1 : 0x7fffffff;
			for (uint32 R = 1; R < RegionCount(); ++R)
			{
				const RegionPopulation* P = Counts(R);
				if (P == nullptr || P->Total == 0)
				{
					continue;
				}
				const int32 Sum =
					RegionYear(Instance, Types().World, R, JudgedYear(), ClimateRules{}).ColdSum.FloorToInt();
				if (Cold ? Sum > BestSum : Sum < BestSum)
				{
					BestSum = Sum;
					Best = R;
				}
			}
			return Best;
		}
		/// The most peopled region whose judged year has at least MinColdSum
		/// degree-days of cold; ties to the lower index.
		uint32 Busiest(uint32 MinColdSum) const
		{
			uint32 Best = 0;
			uint32 People = 0;
			for (uint32 R = 1; R < RegionCount(); ++R)
			{
				const RegionPopulation* P = Counts(R);
				if (P == nullptr || P->Total <= People)
				{
					continue;
				}
				const int32 Sum =
					RegionYear(Instance, Types().World, R, JudgedYear(), ClimateRules{}).ColdSum.FloorToInt();
				if (Sum >= static_cast<int32>(MinColdSum))
				{
					People = P->Total;
					Best = R;
				}
			}
			return Best;
		}
		/// Promotes a region AND asks the bridge to keep it: a region promoted
		/// by hand that nobody wants is let go at the next turn (15.03).
		bool Promote(uint32 Region)
		{
			return RequestDetail(Instance, Lod, Region) &&
				   PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0;
		}
		/// Every living person of a region and the chill they carry (255+1 for
		/// one carrying no warmth yet), by index.
		std::map<uint32, uint32> Chills(uint32 Region) const
		{
			std::map<uint32, uint32> Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive))
						{
							const PersonWarmth* C = Instance.Components().GetPool(Warmth.Warmth).TryGet(H);
							Out[P.Index] = C != nullptr ? C->Chill : 256u;
						}
					});
			return Out;
		}
		/// The house stocks of a region: family index -> grain.
		std::map<uint32, uint32> HouseGrain(uint32 Region) const
		{
			std::map<uint32, uint32> Out;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle H, const FamilyInfo& F)
					{
						const HouseStock* S = Instance.Components().GetPool(Economy_.House).TryGet(H);
						if (F.Region == Region && S != nullptr)
						{
							Out[F.Index] = S->Amount[static_cast<uint32>(Good::Grain)];
						}
					});
			return Out;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		WarmthTypes Warmth;
		EconomyChronicleTypes Chronicle;
		EconomyContext Context;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<WinterSystem> Winters;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<EconomyChronicle> Scribe;
	};

	/// The Winter events of one tick, by region.
	std::map<uint32, const Event*> WintersAt(const World& W, SimTick Tick)
	{
		std::map<uint32, const Event*> Out;
		for (const Event& E : W.Log().All())
		{
			if (E.Tick == Tick && E.Is(WinterEvent))
			{
				Out[E.Get<WinterPayload>().Region] = &E;
			}
		}
		return Out;
	}

	/// The StockTaken events of one tick a winter caused, for one region.
	std::vector<const Event*> TakenByTheWinter(const World& W, SimTick Tick, uint32 Region)
	{
		std::vector<const Event*> Out;
		for (const Event& E : W.Log().All())
		{
			if (E.Tick != Tick || !E.Is(StockTakenEvent) || !E.Cause.IsValid())
			{
				continue;
			}
			const StockPayload P = E.Get<StockPayload>();
			const Event* Cause = FindEvent(W.Log(), E.Cause);
			if (P.Region == Region && Cause != nullptr && Cause->Is(WinterEvent))
			{
				Out.push_back(&E);
			}
		}
		return Out;
	}

	constexpr uint32 G_GRAIN = static_cast<uint32>(Good::Grain);
	constexpr uint32 G_TIMBER = static_cast<uint32>(Good::Timber);
} // namespace

VAELEN_TEST(Winter, DefaultsAndThePayloadAreSane)
{
	const WinterRules R;
	VT_CHECK_EQ(R.DegreeDaysPerTimber, 200u);
	VT_CHECK_EQ(R.TimberPerPersons, ProductionRules{}.TimberPerPersons);
	VT_CHECK_EQ(R.ClothPerPersons, ProductionRules{}.ClothWearPerPersons);
	VT_CHECK_EQ(R.WinterGrainPerMille[1], 120u);
	VT_CHECK_EQ(R.ColdDeathsPerMille[0], 0u);
	VT_CHECK_EQ(R.ColdDeathsPerMille[3], 20u);
	VT_CHECK(R.ColdDeathsPerMille[3] < 25u); // under GrowthPerMille: no region is emptied by winter alone
	VT_CHECK_EQ(R.DailyRegion, 0u);
	VT_CHECK_EQ(static_cast<uint32>(sizeof(WinterPayload)), 24u);
	VT_CHECK(WinterEvent.IsValid() && WinterForeseenEvent.IsValid());
}

VAELEN_TEST(Winter, TheColdSumFollowsTheLatitudeAndTheEventTheSeverity)
{
	Run W(AelvorSeed, Ask{});
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	const uint32 Polar = W.Extreme(true);
	const uint32 Warm = W.Extreme(false);
	VT_REQUIRE(Polar != 0 && Warm != 0 && Polar != Warm);
	const YearShape PolarYear = RegionYear(W.Instance, W.Types().World, Polar, W.JudgedYear(), ClimateRules{});
	const YearShape WarmYear = RegionYear(W.Instance, W.Types().World, Warm, W.JudgedYear(), ClimateRules{});
	VT_CHECK(PolarYear.ColdSum.FloorToInt() > 0);
	VT_CHECK(PolarYear.ColdSum.FloorToInt() >= 10 * WarmYear.ColdSum.FloorToInt());
	VT_REQUIRE(WinterSeverity(PolarYear, ClimateRules{}) >= 1u);
	const SimTick Turn = W.Instance.Now();
	W.Ages.Run(1);
	const std::map<uint32, const Event*> Winters = WintersAt(W.Instance, Turn);
	VT_REQUIRE(Winters.count(Polar) == 1u);
	// Every winter published is the closed form's, and none lay on a region
	// whose coldest day was above freezing.
	uint32 AboveFreezing = 0;
	uint32 Checked = 0;
	for (uint32 R = 1; R < W.RegionCount(); ++R)
	{
		const YearShape Y = RegionYear(W.Instance, W.Types().World, R, Turn / TicksPerYear - 1u, ClimateRules{});
		const uint32 S = WinterSeverity(Y, ClimateRules{});
		const auto Found = Winters.find(R);
		VT_CHECK_MSG((Found != Winters.end()) == (S >= 1u), "region %u: severity %u, event %s", R, S,
					 Found != Winters.end() ? "published" : "none");
		if (Found != Winters.end())
		{
			const WinterPayload P = Found->second->Get<WinterPayload>();
			VT_CHECK_EQ(P.Severity, S);
			VT_CHECK_EQ(static_cast<int32>(P.ColdSum), Y.ColdSum.FloorToInt());
			VT_CHECK(P.Usual <= 3u);
			AboveFreezing += Y.Coldest.FloorToInt() >= 0 ? 1u : 0u;
			++Checked;
		}
	}
	VT_CHECK_EQ(AboveFreezing, 0u);
	VT_CHECK(Winters.count(0) == 0u);
	const WinterStats S = MeasureWinters(W.Instance, 0);
	VT_CHECK(S.Winters[1] + S.Winters[2] + S.Winters[3] > 0);
	VAELEN_LOG_INFO(
		LogWinter,
		"AELVOR 64: polar region %u cold sum %d, warm region %u cold sum %d; %u winters at the turn (%u/%u/%u by "
		"severity over 121 years), %u foreseen, %u coarse dead, %llu timber and %llu grain taken",
		Polar, PolarYear.ColdSum.FloorToInt(), Warm, WarmYear.ColdSum.FloorToInt(), Checked, S.Winters[1], S.Winters[2],
		S.Winters[3], S.Foreseen, S.ColdDeaths, static_cast<unsigned long long>(S.TimberTaken),
		static_cast<unsigned long long>(S.GrainTaken));
}

VAELEN_TEST(Winter, TimberCoversTheColdAndTheLedgerNamesIt)
{
	// AELVOR 128: the busiest region with at least a great winter's cold
	// (600 degree-days), so the fuel wanted is worth taking and everybody in
	// it can be chilled.
	Run W(AelvorSeed, Ask{});
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Polar = W.Busiest(600u);
	VT_REQUIRE(Polar != 0 && W.Promote(Polar));
	W.Ages.Run(1); // the turn that gives everyone warmth
	const uint32 ColdSum = static_cast<uint32>(
		RegionYear(W.Instance, W.Types().World, Polar, W.JudgedYear(), ClimateRules{}).ColdSum.FloorToInt());
	const std::map<uint32, uint32> Before = W.Chills(Polar);
	const uint32 People = static_cast<uint32>(Before.size());
	const uint32 Wanted = (ColdSum / 200u) * (People / 10u);
	VAELEN_LOG_INFO(LogWinter, "region %u: %u people carrying warmth, cold sum %u, %u timber wanted (coarse total %u)",
					Polar, People, ColdSum, Wanted, W.Counts(Polar) != nullptr ? W.Counts(Polar)->Total : 0u);
	VT_REQUIRE(People >= 10u && ColdSum >= 200u && Wanted > 0u);
	// 1000 timber in the common stock beyond what the winter wants: every
	// unit wanted is taken, named, and nobody is chilled.
	const RegionStock* Common = W.Common(Polar);
	VT_REQUIRE(Common != nullptr);
	const uint32 Have = Common->Amount[G_TIMBER];
	VT_CHECK_EQ(AddStock(W.Instance, W.Types(), W.Families, W.Economy_, Polar, 0u, Good::Timber,
						 static_cast<int32>(1000u + Wanted), W.Instance.Now()),
				1000u + Wanted);
	SimTick Turn = W.Instance.Now();
	W.Ages.Run(1);
	std::vector<const Event*> Taken = TakenByTheWinter(W.Instance, Turn, Polar);
	uint32 Timber = 0;
	for (const Event* E : Taken)
	{
		const StockPayload P = E->Get<StockPayload>();
		Timber += P.Good == G_TIMBER ? P.Amount : 0u;
		VT_CHECK_EQ(P.Region, Polar);
	}
	// To the unit, on the people the winter found: the year's births came
	// before it in the same tick, and the payload says how many stood there.
	const std::map<uint32, const Event*> Winters = WintersAt(W.Instance, Turn);
	VT_REQUIRE(Winters.count(Polar) == 1u);
	const WinterPayload Lay = Winters.at(Polar)->Get<WinterPayload>();
	VT_CHECK_EQ(Lay.ColdSum, ColdSum);
	VT_CHECK(Lay.People >= People);
	const uint32 WantedThen = (Lay.ColdSum / 200u) * (Lay.People / 10u);
	VT_CHECK_MSG(Timber == WantedThen,
				 "the winter took %u timber; cold sum %u over %u people wanted %u (%u before the turn)", Timber,
				 Lay.ColdSum, Lay.People, WantedThen, Wanted);
	std::map<uint32, uint32> After = W.Chills(Polar);
	uint32 Chilled = 0;
	for (const auto& [Person, Chill] : Before)
	{
		const auto Now = After.find(Person);
		if (Now != After.end() && Chill != 256u && Now->second != 256u)
		{
			Chilled += Now->second > Chill ? 1u : 0u;
		}
	}
	VT_CHECK_EQ(Chilled, 0u);
	VAELEN_LOG_INFO(LogWinter,
					"region %u: %u people, cold sum %u, %u timber wanted and taken (had %u, given %u); nobody chilled",
					Polar, Lay.People, ColdSum, WantedThen, Have, 1000u + Wanted);

	// CONTROL: the same winter with no timber at all chills every person by
	// the same amount, which is the exposure's share of the cold, and takes
	// nothing.
	Common = W.Common(Polar);
	VT_REQUIRE(Common != nullptr);
	AddStock(W.Instance, W.Types(), W.Families, W.Economy_, Polar, 0u, Good::Timber,
			 -static_cast<int32>(Common->Amount[G_TIMBER]), W.Instance.Now());
	VT_CHECK_EQ(W.Common(Polar)->Amount[G_TIMBER], 0u);
	const uint32 ColdSum2 = static_cast<uint32>(
		RegionYear(W.Instance, W.Types().World, Polar, W.JudgedYear(), ClimateRules{}).ColdSum.FloorToInt());
	const std::map<uint32, uint32> Before2 = W.Chills(Polar);
	const uint32 People2 = static_cast<uint32>(Before2.size());
	const uint32 Cover = W.Common(Polar)->Amount[static_cast<uint32>(Good::Cloth)] >= People2 / 25u ? 300u : 0u;
	const uint64 Expected = std::min<uint64>(255u, uint64{1000u - Cover} * ColdSum2 / 1000u / 20u);
	VT_REQUIRE(Expected > 0u);
	Turn = W.Instance.Now();
	W.Ages.Run(1);
	Taken = TakenByTheWinter(W.Instance, Turn, Polar);
	Timber = 0;
	for (const Event* E : Taken)
	{
		Timber += E->Get<StockPayload>().Good == G_TIMBER ? E->Get<StockPayload>().Amount : 0u;
	}
	VT_CHECK_EQ(Timber, 0u);
	After = W.Chills(Polar);
	uint32 Exact = 0;
	uint32 Wrong = 0;
	for (const auto& [Person, Chill] : Before2)
	{
		const auto Now = After.find(Person);
		if (Now == After.end() || Chill == 256u || Now->second == 256u)
		{
			continue;
		}
		const uint32 Rose = std::min<uint32>(255u, Chill + static_cast<uint32>(Expected));
		Exact += Now->second == Rose ? 1u : 0u;
		Wrong += Now->second == Rose ? 0u : 1u;
	}
	VT_CHECK_MSG(Wrong == 0u && Exact > 0u, "%u persons chilled by exactly %llu, %u not", Exact,
				 static_cast<unsigned long long>(Expected), Wrong);
	VAELEN_LOG_INFO(LogWinter, "without timber: %u persons chilled by %llu each (cold sum %u, cover %u)", Exact,
					static_cast<unsigned long long>(Expected), ColdSum2, Cover);
}

VAELEN_TEST(Winter, AGreatWinterTakesItsGrainFromEveryStockAndTheChronicleSaysSo)
{
	// A probe learns the polar region's cold sum; the run is then built with
	// the bands set so that the winter it judges is a great one (severity 2).
	uint32 Polar = 0;
	uint32 ColdSum = 0;
	{
		Run Probe(AelvorSeed, Ask{});
		VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
		Polar = Probe.Busiest(600u);
		VT_REQUIRE(Polar != 0 && Probe.Promote(Polar));
		Probe.Ages.Run(1);
		ColdSum = static_cast<uint32>(
			RegionYear(Probe.Instance, Probe.Types().World, Polar, Probe.JudgedYear(), ClimateRules{})
				.ColdSum.FloorToInt());
	}
	VT_REQUIRE(ColdSum > 1u);
	Ask A;
	A.Winter.Climate.SeverityDegreeDays[0] = 1u;
	A.Winter.Climate.SeverityDegreeDays[1] = ColdSum;
	A.Winter.Climate.SeverityDegreeDays[2] = ColdSum + 1000u;
	Run W(AelvorSeed, A);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(W.Promote(Polar)); // the same region: the cold is the map's, not the people's
	W.Ages.Run(1);
	VT_REQUIRE(WinterSeverity(RegionYear(W.Instance, W.Types().World, Polar, W.JudgedYear(), A.Winter.Climate),
							  A.Winter.Climate) == 2u);
	// A detailed region keeps its grain in its houses; a granary of 1000 in
	// the common stock gives the winter something worth a line (the economy
	// chronicle's floor is twenty units).
	VT_CHECK_EQ(AddStock(W.Instance, W.Types(), W.Families, W.Economy_, Polar, 0u, Good::Grain, 1000, W.Instance.Now()),
				1000u);
	const uint32 CommonBefore = W.Common(Polar)->Amount[G_GRAIN];
	const std::map<uint32, uint32> HousesBefore = W.HouseGrain(Polar);
	VAELEN_LOG_INFO(LogWinter, "region %u: common grain %u, %u houses with a stock", Polar, CommonBefore,
					static_cast<uint32>(HousesBefore.size()));
	VT_REQUIRE(CommonBefore >= 1000u && HousesBefore.size() >= 3u);
	const SimTick Turn = W.Instance.Now();
	const usize Mark = W.Instance.Log().Count();
	W.Ages.Run(1);
	const std::map<uint32, const Event*> Winters = WintersAt(W.Instance, Turn);
	VT_REQUIRE(Winters.count(Polar) == 1u);
	VT_CHECK_EQ(Winters.at(Polar)->Get<WinterPayload>().Severity, 2u);
	// The common stock, untouched by anything before the winter in that tick,
	// lost exactly 120 per mille; every house lost 120 per mille of what it
	// held when the winter came - which is what it held at the year's end
	// unless a death or an inheritance moved its grain first in the same
	// tick, and those houses are counted, not judged.
	uint32 CommonTaken = 0;
	bool CommonMoved = false;
	uint32 HousesExact = 0;
	uint32 HousesMoved = 0;
	uint32 HousesWrong = 0;
	const std::vector<Event>& Events = W.Instance.Log().All();
	auto IsStockMove = [](const Event& E)
	{
		return E.Is(StockAddedEvent) || E.Is(StockTakenEvent) || E.Is(StockSplitEvent) || E.Is(StockFoldedEvent) ||
			   E.Is(StockInheritedEvent) || E.Is(StockReturnedEvent) || E.Is(StockEndowedEvent);
	};
	for (const Event* E : TakenByTheWinter(W.Instance, Turn, Polar))
	{
		const StockPayload P = E->Get<StockPayload>();
		if (P.Good != G_GRAIN)
		{
			continue;
		}
		if (P.House == 0)
		{
			CommonTaken += P.Amount;
			// A house that died out or was endowed in the same tick moved grain
			// through the common stock before the winter came.
			for (usize i = Mark; i < Events.size() && &Events[i] != E; ++i)
			{
				CommonMoved = CommonMoved || (IsStockMove(Events[i]) && Events[i].Get<StockPayload>().Region == Polar &&
											  Events[i].Get<StockPayload>().Good == G_GRAIN);
			}
			continue;
		}
		const auto Held = HousesBefore.find(P.House);
		if (Held == HousesBefore.end())
		{
			++HousesMoved;
			continue;
		}
		bool Moved = false;
		for (usize i = Mark; i < Events.size() && &Events[i] != E; ++i)
		{
			const Event& Other = Events[i];
			if (IsStockMove(Other) && Other.Get<StockPayload>().House == P.House)
			{
				Moved = true;
			}
		}
		if (Moved)
		{
			++HousesMoved;
		}
		else if (P.Amount == Held->second * 120u / 1000u)
		{
			++HousesExact;
		}
		else
		{
			++HousesWrong;
			VT_CHECK_MSG(false, "house %u held %u and the winter took %u, not %u", P.House, Held->second, P.Amount,
						 Held->second * 120u / 1000u);
		}
	}
	if (!CommonMoved)
	{
		VT_CHECK_EQ(CommonTaken, CommonBefore * 120u / 1000u);
	}
	VT_CHECK(CommonTaken > 0u);
	VT_CHECK_EQ(HousesWrong, 0u);
	VT_CHECK(HousesExact > 0u || !CommonMoved); // something was judged to the unit
	// The economy chronicle put it into words.
	std::string Text;
	ExportChronicleWithEconomy(W.Instance, W.Types(), W.Context, Text);
	std::string Name;
	NameRegion(W.Instance, W.Types(), Polar, Name);
	const std::string Line =
		"the winter took " + std::to_string(CommonTaken) + " grain from the stores of " + Name + ".";
	VT_CHECK_MSG(Text.find(Line) != std::string::npos, "the chronicle lacks \"%s\"", Line.c_str());
	VT_CHECK(Text.find("something happened") == std::string::npos);
	VAELEN_LOG_INFO(
		LogWinter,
		"great winter on region %u: common grain %u -> took %u (%s); houses %u exact, %u moved first, %u wrong; \"%s\"",
		Polar, CommonBefore, CommonTaken, CommonMoved ? "moved first" : "exact", HousesExact, HousesMoved, HousesWrong,
		Line.c_str());
}

VAELEN_TEST(Winter, TheCoarseColdDieByTheShareAndTheFaithsFollow)
{
	// AELVOR 128: at 64 no coarse cold region holds the fifty people a
	// terrible winter's twenty per mille needs to kill one.
	Run W(AelvorSeed, Ask{});
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const SimTick Turn = W.Instance.Now();
	W.Ages.Run(1);
	uint32 Checked = 0;
	uint32 WithDeaths = 0;
	for (const auto& [Region, E] : WintersAt(W.Instance, Turn))
	{
		const WinterPayload P = E->Get<WinterPayload>();
		// Nobody is detailed here: the payload's deaths are the share of the
		// people it names, to the unit, and never more than they were.
		VT_CHECK_EQ(P.Deaths,
					static_cast<uint32>(uint64{P.People} * WinterRules{}.ColdDeathsPerMille[P.Severity] / 1000u));
		VT_CHECK(P.Deaths <= P.People);
		WithDeaths += P.Deaths > 0u ? 1u : 0u;
		// And the believers never exceed the living after it.
		const EntityHandle H = W.RegionHandle(Region);
		const RegionPopulation* Counts = W.Counts(Region);
		const RegionFaith* F =
			H.IsNull() ? nullptr : W.Instance.Components().GetPool(W.Types().Religion.Faith).TryGet(H);
		if (Counts != nullptr && F != nullptr)
		{
			VT_CHECK_MSG(F->Total() <= Counts->Total, "region %u: %u believers among %u living", Region, F->Total(),
						 Counts->Total);
		}
		++Checked;
	}
	VT_REQUIRE(Checked > 0u);
	VT_CHECK(WithDeaths > 0u); // somewhere the cold killed: the instrument measures something
	VAELEN_LOG_INFO(LogWinter, "%u winters at the turn, %u with coarse deaths", Checked, WithDeaths);
}

VAELEN_TEST(Winter, NothingHappensWhereThereIsNoCold)
{
	// ADR-0149 rule 1: a winter system whose cold line nothing falls below
	// takes no timber, no grain, chills nobody, kills nobody and publishes
	// nothing - and the world is, to the digest, the world without it.
	Ask None;
	None.Winter.Climate.ColdLine = Fix64::FromInt(-100);
	Ask Without;
	Without.WithWinter = false;
	Run A(AelvorSeed, None);
	Run B(AelvorSeed, Without);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Polar = A.Extreme(true);
	VT_REQUIRE(Polar != 0 && A.Promote(Polar) && B.Promote(Polar));
	A.Ages.Run(3);
	B.Ages.Run(3);
	const WinterStats S = MeasureWinters(A.Instance, 0);
	VT_CHECK_EQ(S.Winters[1] + S.Winters[2] + S.Winters[3], 0u);
	VT_CHECK_EQ(S.Foreseen, 0u);
	VT_CHECK_EQ(S.ColdDeaths, 0u);
	VT_CHECK_EQ(S.TimberTaken, 0ull);
	VT_CHECK_EQ(S.GrainTaken, 0ull);
	uint32 Chilled = 0;
	for (const auto& [Person, Chill] : A.Chills(Polar))
	{
		Chilled += Chill != 256u && Chill > 0u ? 1u : 0u;
	}
	VT_CHECK_EQ(Chilled, 0u);
	VT_CHECK_DIGEST_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_DIGEST_EQ(A.Instance.Log().Digest(), B.Instance.Log().Digest());
	// And the same seed WITH the cold is another world: the instrument reads.
	Run C(AelvorSeed, Ask{});
	VT_REQUIRE(C.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(C.Promote(Polar));
	C.Ages.Run(3);
	VT_CHECK(ComputeStateDigest(C.Instance) != ComputeStateDigest(B.Instance));
	VT_CHECK(MeasureWinters(C.Instance, 0).TimberTaken + MeasureWinters(C.Instance, 0).GrainTaken > 0ull);
}

VAELEN_TEST(Winter, TheSameYearTwiceFromOneImage)
{
	Run A(AelvorSeed, Ask{});
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	const uint32 Polar = A.Extreme(true);
	VT_REQUIRE(Polar != 0 && A.Promote(Polar));
	A.Ages.Run(1);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed, Ask{});
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	A.Ages.Run(2);
	R.Ages.Run(2);
	VT_CHECK_DIGEST_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_DIGEST_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	const WinterStats SA = MeasureWinters(A.Instance, 0);
	const WinterStats SR = MeasureWinters(R.Instance, 0);
	VT_CHECK_EQ(SA.TimberTaken, SR.TimberTaken);
	VT_CHECK_EQ(SA.GrainTaken, SR.GrainTaken);
	VT_CHECK_EQ(SA.ColdDeaths, SR.ColdDeaths);
	VT_CHECK(SA.Winters[1] + SA.Winters[2] + SA.Winters[3] > 0u);
}
