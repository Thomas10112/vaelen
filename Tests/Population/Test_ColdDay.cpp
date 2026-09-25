// VAELEN - Tests/Population
// Phase 18.08: the chill of a day - the exact-sum schedule of the colony's
// food, for a cold that falls on the frost days only.
//
// STATUS: PROTOTYPE (Phase 18)
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Warmth.h"
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <map>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogColdDay);
	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// What the yearly winter (18.06) puts on a person at an exposure: the
	/// figure a year of days must sum to.
	uint32 YearlyChill(Fix64 ColdSum, uint32 Exposure, uint32 DegreeDaysPerChill)
	{
		const int32 C = ColdSum.FloorToInt();
		return C > 0 ? static_cast<uint32>(static_cast<uint64>(C) * Exposure / 1000u / DegreeDaysPerChill) : 0u;
	}

	/// Lives, needs and warmth, and a colony living at the day on Region (0:
	/// none), chilled by the day when Cold. A system added to a built world is
	/// not scheduled, so the region is known before the world is built - from
	/// a probe of the same seed.
	struct Run
	{
		Run(uint64 Seed, uint32 Region, bool Cold, uint32 Exposure)
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Needs = NeedTypes::Declare(Instance);
			Warmth = WarmthTypes::Declare(Instance);
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, LifeRules{});
			NeedRules Body_;
			Body_.DailyRegion = Region; // the colony eats by the day
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, Body_);
			// The line at frozen through and no yearly recovery: the chill the
			// days put on stays where the test can read it, and kills nobody.
			Body->ObserveWinter(Warmth, WarmthRules{255u, 40u, 0u});
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Body.get());
			if (Region != 0u)
			{
				ColonyDayRules Day;
				Day.Region = Region;
				Days = std::make_unique<ColonyDaySystem>(Instance, Persons, Needs, Day);
				if (Cold)
				{
					Days->ObserveCold(Ages.Types().World, Warmth, ClimateRules{}, Exposure, 20u);
				}
				Instance.Systems().Add(Days.get());
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
		/// The peopled region with the largest cold sum in Year; ties low.
		uint32 Coldest(uint64 Year) const
		{
			uint32 Best = 0;
			int32 BestSum = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P == nullptr || P->Total < 20u)
						{
							return;
						}
						const int32 Sum = RegionYear(Instance, Ages.Types().World, R.Index, Year, ClimateRules{})
											  .ColdSum.FloorToInt();
						if (Sum > BestSum || (Sum == BestSum && Sum > 0 && R.Index < Best))
						{
							BestSum = Sum;
							Best = R.Index;
						}
					});
			return Best;
		}
		bool Promote(uint32 Region)
		{
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0;
		}
		/// The chill of every living person of a region carrying warmth.
		std::map<uint32, uint32> Chills(uint32 Region) const
		{
			std::map<uint32, uint32> Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						const PersonWarmth* C = Instance.Components().GetPool(Warmth.Warmth).TryGet(H);
						if (P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) && C != nullptr)
						{
							Out[P.Index] = C->Chill;
						}
					});
			return Out;
		}
		std::map<uint32, PersonNeeds> NeedBytes() const
		{
			std::map<uint32, PersonNeeds> Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						const PersonNeeds* N = Instance.Components().GetPool(Needs.Needs).TryGet(H);
						if (N != nullptr && P.State == static_cast<uint8>(LifeState::Alive))
						{
							Out[P.Index] = *N;
						}
					});
			return Out;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		NeedTypes Needs;
		WarmthTypes Warmth;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<ColonyDaySystem> Days;
	};

	bool SameBytes(const PersonNeeds& A, const PersonNeeds& B)
	{
		return A.Food == B.Food && A.Health == B.Health && A.Rest == B.Rest && A.Hungry == B.Hungry &&
			   A.Reserved == B.Reserved;
	}
} // namespace

VAELEN_TEST(ColdDay, TheDaysOfAYearSumToTheWintersChillExactly)
{
	const ClimateRules Rules;
	struct Place
	{
		const char* Name;
		Fix64 Mean;
		Fix64 Latitude;
	};
	const Place Places[] = {
		{"the pole", Fix64::FromInt(-15), Fix64::FromInt(1)},
		{"tundra", Fix64::FromInt(-2), Fix64::FromRatio(7, 10)},
		{"grassland", Fix64::FromInt(8), Fix64::FromRatio(489, 1000)},
		{"a mild coast", Fix64::FromInt(3), Fix64::FromRatio(1, 2)},
		{"the equator", Fix64::FromInt(20), Fix64::Zero()},
	};
	for (const Place& P : Places)
	{
		const YearShape Y = ShapeYear(P.Mean, P.Latitude, Rules);
		VT_CHECK_MSG(ColdSumThrough(P.Mean, P.Latitude, Rules, DaysOfAYear - 1u).Raw == Y.ColdSum.Raw,
					 "%s: the last day's cold sum is not the year's", P.Name);
		for (const uint32 Exposure : {1000u, 700u, 300u, 1u})
		{
			uint64 Sum = 0;
			uint32 OffFrost = 0;
			for (uint32 Day = 0; Day < DaysOfAYear; ++Day)
			{
				const uint32 Chill = ChillOfDay(P.Mean, P.Latitude, Rules, Day, DaysOfAYear, Exposure, 20u);
				Sum += Chill;
				// Only a frost day moves the cold sum, so only a frost day chills.
				OffFrost += Chill > 0u && TemperatureOn(P.Mean, P.Latitude, Day) >= Rules.ColdLine ? 1u : 0u;
			}
			VT_CHECK_MSG(Sum == YearlyChill(Y.ColdSum, Exposure, 20u),
						 "%s at exposure %u: the days sum to %llu, the winter puts %u", P.Name, Exposure,
						 static_cast<unsigned long long>(Sum), YearlyChill(Y.ColdSum, Exposure, 20u));
			VT_CHECK_EQ(OffFrost, 0u);
		}
		VAELEN_LOG_INFO(LogColdDay, "%s: %u frost days, cold sum %d, a year of days at full exposure %u", P.Name,
						Y.FrostDays, Y.ColdSum.FloorToInt(), YearlyChill(Y.ColdSum, 1000u, 20u));
	}
	// CONTROL, ADR-0149 rule 1: the instrument measures something - the pole
	// has frost and a chill - and nothing where there is none.
	const YearShape Pole = ShapeYear(Fix64::FromInt(-15), Fix64::FromInt(1), Rules);
	VT_REQUIRE(Pole.FrostDays > 0u && YearlyChill(Pole.ColdSum, 1000u, 20u) > 0u);
	for (uint32 Day = 0; Day < DaysOfAYear; ++Day)
	{
		VT_CHECK_EQ(ChillOfDay(Fix64::FromInt(20), Fix64::Zero(), Rules, Day, DaysOfAYear, 1000u, 20u), 0u);
	}
	// The day wraps, so year two takes the same shares as year one; a year of
	// no days or a rule of no degree-days takes nothing.
	VT_CHECK_EQ(ChillOfDay(Fix64::FromInt(-15), Fix64::FromInt(1), Rules, 300u, 360u, 1000u, 20u),
				ChillOfDay(Fix64::FromInt(-15), Fix64::FromInt(1), Rules, 660u, 360u, 1000u, 20u));
	VT_CHECK_EQ(ChillOfDay(Fix64::FromInt(-15), Fix64::FromInt(1), Rules, 5u, 0u, 1000u, 20u), 0u);
	VT_CHECK_EQ(ChillOfDay(Fix64::FromInt(-15), Fix64::FromInt(1), Rules, 5u, 360u, 1000u, 0u), 0u);
}

VAELEN_TEST(ColdDay, AColonyIsChilledADayAtATimeAndTheYearAgrees)
{
	constexpr uint32 Exposure = 300u;
	// The probe: the coldest peopled region of the year the colony will be
	// measured over (the second after the one it is promoted in).
	uint32 Region = 0;
	{
		Run Probe(AelvorSeed, 0u, false, Exposure);
		VT_REQUIRE(Probe.Ages.Generate(Run::Square(128), 300));
		Region = Probe.Coldest(Probe.Instance.Now() / TicksPerYear + 1u);
	}
	VT_REQUIRE(Region != 0u);
	Run Cold(AelvorSeed, Region, true, Exposure);
	Run Warm(AelvorSeed, Region, false, Exposure);
	VT_REQUIRE(Cold.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Warm.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(Cold.Promote(Region) && Warm.Promote(Region));
	Cold.Ages.Run(1); // the turn gives everyone warmth; the year's days follow
	Warm.Ages.Run(1);
	const uint64 Year = Cold.Instance.Now() / TicksPerYear; // the turn not yet ticked opens this year
	const YearShape Y = RegionYear(Cold.Instance, Cold.Ages.Types().World, Region, Year, ClimateRules{});
	const uint32 Expected = YearlyChill(Y.ColdSum, Exposure, 20u);
	VT_REQUIRE(Y.FrostDays > 0u && Expected > 0u);
	const std::map<uint32, uint32> Before = Cold.Chills(Region);
	VT_REQUIRE(Before.size() >= 10u);
	Cold.Instance.TickMany(TicksPerYear);
	Warm.Instance.TickMany(TicksPerYear);
	// Every person alive and warm through the whole year took exactly the
	// winter's chill at that exposure, a day at a time.
	const std::map<uint32, uint32> After = Cold.Chills(Region);
	uint32 Exact = 0;
	uint32 Wrong = 0;
	uint32 Saturated = 0;
	for (const auto& [Person, Chill] : Before)
	{
		const auto Now = After.find(Person);
		if (Now == After.end())
		{
			continue; // died in the year
		}
		if (Chill + Expected > 255u)
		{
			++Saturated;
			continue;
		}
		if (Now->second == Chill + Expected)
		{
			++Exact;
		}
		else
		{
			++Wrong;
			VT_CHECK_MSG(false, "person %u: %u at the turn, %u a year later, %u expected", Person, Chill, Now->second,
						 Chill + Expected);
		}
	}
	VT_CHECK_EQ(Wrong, 0u);
	VT_CHECK(Exact >= 10u);
	// CONTROL, ADR-0149 rule 2: the same colony not told the cold chills
	// nobody, and every need byte and the whole log are the cold one's - the
	// chill below the line is state and nothing else.
	uint32 WarmChilled = 0;
	for (const auto& [Person, Chill] : Warm.Chills(Region))
	{
		WarmChilled += Chill > 0u ? 1u : 0u;
	}
	VT_CHECK_EQ(WarmChilled, 0u);
	const std::map<uint32, PersonNeeds> CB = Cold.NeedBytes();
	const std::map<uint32, PersonNeeds> WB = Warm.NeedBytes();
	VT_CHECK_EQ(static_cast<uint32>(CB.size()), static_cast<uint32>(WB.size()));
	uint32 Differ = 0;
	for (const auto& [Person, N] : CB)
	{
		const auto O = WB.find(Person);
		Differ += O == WB.end() || !SameBytes(N, O->second) ? 1u : 0u;
	}
	VT_CHECK_EQ(Differ, 0u);
	VT_CHECK_DIGEST_EQ(Cold.Instance.Log().Digest(), Warm.Instance.Log().Digest());
	VAELEN_LOG_INFO(LogColdDay,
					"colony region %u, year %llu: %u frost days, cold sum %d, %u chill a person at exposure %u; %u "
					"persons exact, %u saturated; %u need records equal to the colony not told the cold",
					Region, static_cast<unsigned long long>(Year), Y.FrostDays, Y.ColdSum.FloorToInt(), Expected,
					Exposure, Exact, Saturated, static_cast<uint32>(CB.size()));
}
