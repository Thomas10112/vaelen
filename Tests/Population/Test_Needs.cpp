// VAELEN - Tests/Population
// Phase 04.04: needs and body - food, health, famine from drought, disease from plague.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Disasters.h"
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
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.04): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 200 years with
// lives and needs.
#define VAELEN_NEEDS_FROZEN_128 0x0e64632a2f2ded8cull
#define VAELEN_NEEDS_ALIVE_128 1497u
#define VAELEN_NEEDS_CAUSED_128 42u
#define VAELEN_NEEDS_HEALTH_128 381420ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogNeeds);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, PreHistoryRules Ages_ = PreHistoryRules{}, NeedRules InRules = NeedRules{})
			: Instance(Config(Seed)), Ages(Instance, Ages_)
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Needs = NeedTypes::Declare(Instance);
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, LifeRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, InRules);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Body.get());
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
						if (P != nullptr && P->Total > People)
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		RegionPopulation* Counts(uint32 Region)
		{
			RegionPopulation* Found = nullptr;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region)
						{
							Found = Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						}
					});
			return Found;
		}
		bool Promote(uint32 Region)
		{
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		NeedTypes Needs;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<NeedSystem> Body;
	};

	/// A world without omens of its own where every omen strikes.
	PreHistoryRules Cursed()
	{
		PreHistoryRules R;
		for (uint32 K = 0; K < static_cast<uint32>(DisasterKind::Count); ++K)
		{
			R.Disasters.OmenPerMille[K] = 0;
		}
		R.Disasters.StrikePerMille = 1000;
		return R;
	}

	/// Queues an omen of full risk on a region: it strikes at the next yearly tick.
	bool Curse(Run& W, uint32 Region, DisasterKind Kind)
	{
		bool Queued = false;
		W.Instance.Components()
			.GetPool(W.Ages.Types().Disasters.State)
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

	struct CauseCheck
	{
		uint32 Deaths = 0;
		uint32 Caused = 0;
		uint32 Famine = 0;
		uint32 Plague = 0;
		uint32 BadCause = 0; ///< a cause that is not a DisasterStruck of the right kind and region
	};

	CauseCheck CheckCauses(const World& W, uint32 Region, usize From)
	{
		CauseCheck C;
		const std::vector<Event>& Events = W.Log().All();
		for (usize i = From; i < Events.size(); ++i)
		{
			const Event& E = Events[i];
			if (!E.Is(PersonDiedEvent) || E.Get<PersonPayload>().Region != Region)
			{
				continue;
			}
			++C.Deaths;
			const PersonPayload P = E.Get<PersonPayload>();
			C.Famine += P.Other == static_cast<uint32>(DeathCause::Famine) ? 1u : 0u;
			C.Plague += P.Other == static_cast<uint32>(DeathCause::Plague) ? 1u : 0u;
			if (!E.Cause.IsValid())
			{
				continue;
			}
			++C.Caused;
			const Event* Cause = FindEvent(W.Log(), E.Cause);
			bool Good = Cause != nullptr && Cause->Is(DisasterStruckEvent) && Cause->Tick <= E.Tick;
			if (Good)
			{
				const DisasterPayload D = Cause->Get<DisasterPayload>();
				const uint32 Kind = P.Other == static_cast<uint32>(DeathCause::Famine)
										? static_cast<uint32>(DisasterKind::Drought)
										: static_cast<uint32>(DisasterKind::Plague);
				Good = D.Region == Region && D.Kind == Kind && D.Deaths == 0;
			}
			C.BadCause += Good ? 0u : 1u;
		}
		return C;
	}
} // namespace

VAELEN_TEST(Needs, DefaultsAndRulesAreSane)
{
	const PersonNeeds N;
	VT_CHECK_EQ(N.Food, uint8{200});
	VT_CHECK_EQ(N.Health, uint8{200});
	VT_CHECK_EQ(N.Hungry, uint8{0});
	const NeedRules R;
	// A full ration feeds more than a year burns; a starving year cannot kill a whole person at once.
	VT_CHECK(R.FoodRefillMax > R.FoodBurn &&
			 R.FoodRefillMax - R.FoodBurn >= R.HungerLine / 2); // good years rebuild the stores
	VT_CHECK(R.HungerLine < 255 && R.HungerDamage < 200);
	for (uint32 S = 1; S < 3; ++S)
	{
		VT_CHECK(R.DroughtCutPerMille[S] > R.DroughtCutPerMille[S - 1]);
		VT_CHECK(R.PlagueSharePerMille[S] > R.PlagueSharePerMille[S - 1]);
		VT_CHECK(R.PlagueDamage[S] > R.PlagueDamage[S - 1]);
		VT_CHECK(R.DroughtCutPerMille[S] <= 1000 && R.PlagueSharePerMille[S] <= 1000);
	}
	VT_CHECK_EQ(static_cast<uint32>(DeathCause::Natural), 0u);
}

VAELEN_TEST(Needs, AFedRegionStaysWholeAndDisastersKillWithACause)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	const uint32 Start = W.Counts(Region)->Total;
	VT_REQUIRE(W.Promote(Region));
	const usize Mark = W.Instance.Log().Count();
	uint32 Failures = 0;
	for (uint32 Year = 1; Year <= 200; ++Year)
	{
		W.Ages.Run(1);
		if (!IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the persons and the counts disagree", Year);
		}
		const NeedStats S = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
		const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
		VT_CHECK_EQ(S.WithNeeds, L.Alive); // everyone alive carries needs after a year
		if (Year % 50 == 0)
		{
			VAELEN_LOG_INFO(LogNeeds,
							"year %u: %u alive, %u hungry, %u weak, mean food %llu, mean health %llu, deaths "
							"famine %u starvation %u plague %u natural %u",
							Year, S.WithNeeds, S.Hungry, S.Weak,
							static_cast<unsigned long long>(S.WithNeeds > 0 ? S.FoodSum / S.WithNeeds : 0u),
							static_cast<unsigned long long>(S.WithNeeds > 0 ? S.HealthSum / S.WithNeeds : 0u),
							S.FamineDeaths, S.StarvationDeaths, S.PlagueDeaths, S.NaturalDeaths);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const RegionPopulation End = *W.Counts(Region);
	VT_CHECK(End.Total * 2 >= End.Capacity); // fed: the region stays peopled
	VT_CHECK(End.Total * 4 > Start);
	const NeedStats S = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	VT_CHECK(S.NaturalDeaths > 0);
	VT_CHECK(S.HealthSum > uint64{S.WithNeeds} * 150); // most people are whole most of the time
	// Every caused death points at a disaster that struck this region earlier,
	// of the matching kind, whose coarse record killed nobody.
	const CauseCheck C = CheckCauses(W.Instance, Region, Mark);
	VT_CHECK_EQ(C.BadCause, 0u);
	VT_CHECK_EQ(C.Caused, C.Famine + C.Plague);
	VT_CHECK_EQ(C.Deaths, S.FamineDeaths + S.StarvationDeaths + S.PlagueDeaths + S.NaturalDeaths);
	// A drought or a plague struck here in two centuries: the region felt it.
	uint32 Struck = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(DisasterStruckEvent) && E.Get<DisasterPayload>().Region == Region && E.Tick >= TicksPerYear * 300)
		{
			const DisasterPayload D = E.Get<DisasterPayload>();
			Struck += D.Kind == static_cast<uint32>(DisasterKind::Drought) ||
							  D.Kind == static_cast<uint32>(DisasterKind::Plague)
						  ? 1u
						  : 0u;
		}
	}
	VAELEN_LOG_INFO(LogNeeds, "region %u: %u droughts or plagues, %u caused deaths (%u famine, %u plague)", Region,
					Struck, C.Caused, C.Famine, C.Plague);
	VT_CHECK(C.Caused == 0 || Struck > 0);
}

VAELEN_TEST(Needs, DroughtsStarveAndPlaguesSickenTheDetailedRegion)
{
	constexpr uint32 CursedYears = 10;
	Run W(AelvorSeed, Cursed());
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	const uint32 Start = W.Counts(Region)->Total;
	VT_REQUIRE(W.Promote(Region));
	const usize Mark = W.Instance.Log().Count();
	uint32 HungryYears = 0;
	uint32 Droughts = 0;
	uint32 Plagues = 0;
	for (uint32 Year = 1; Year <= CursedYears; ++Year)
	{
		// Droughts in odd years, plagues in even years.
		VT_REQUIRE(Curse(W, Region, Year % 2 == 1 ? DisasterKind::Drought : DisasterKind::Plague));
		W.Ages.Run(1);
		VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
		const NeedStats S = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
		HungryYears += S.Hungry > 0 ? 1u : 0u;
		VAELEN_LOG_INFO(LogNeeds, "cursed year %u: %u alive, %u hungry, %u weak, famine %u, plague %u, natural %u",
						Year, S.WithNeeds, S.Hungry, S.Weak, S.FamineDeaths, S.PlagueDeaths, S.NaturalDeaths);
	}
	const std::vector<Event>& Events = W.Instance.Log().All();
	for (usize i = Mark; i < Events.size(); ++i)
	{
		if (Events[i].Is(DisasterStruckEvent) && Events[i].Get<DisasterPayload>().Region == Region)
		{
			const DisasterPayload D = Events[i].Get<DisasterPayload>();
			Droughts += D.Kind == static_cast<uint32>(DisasterKind::Drought) ? 1u : 0u;
			Plagues += D.Kind == static_cast<uint32>(DisasterKind::Plague) ? 1u : 0u;
			VT_CHECK(D.Severity >= 1 && D.Severity <= 3);
			VT_CHECK_EQ(D.Deaths, 0u); // the coarse system kills nobody in a detailed region
		}
	}
	const NeedStats S = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	const CauseCheck C = CheckCauses(W.Instance, Region, Mark);
	VAELEN_LOG_INFO(LogNeeds,
					"cursed region %u: %u droughts, %u plagues in %u years; %u -> %u people; deaths famine %u "
					"starvation %u plague %u natural %u (%u hungry years, %u hungry now, %u weak now)",
					Region, Droughts, Plagues, CursedYears, Start, W.Counts(Region)->Total, S.FamineDeaths,
					S.StarvationDeaths, S.PlagueDeaths, S.NaturalDeaths, HungryYears, S.Hungry, S.Weak);
	VT_CHECK_EQ(Droughts, CursedYears / 2);
	VT_CHECK_EQ(Plagues, CursedYears / 2);
	VT_CHECK(S.FamineDeaths > 0 && S.PlagueDeaths > 0);
	VT_CHECK(HungryYears >= 3);
	VT_CHECK_EQ(C.BadCause, 0u);
	VT_CHECK_EQ(C.Caused, C.Famine + C.Plague);
	VT_CHECK(C.Caused > S.NaturalDeaths);		   // the curse, not old age, empties the region
	VT_CHECK(W.Counts(Region)->Total * 2 < Start); // the curse costs people
	VT_CHECK(W.Counts(Region)->Total > 0);		   // but ten years do not empty the region
	// Relief: fed again, the survivors recover and the region grows back.
	const uint32 AtRelief = W.Counts(Region)->Total;
	W.Ages.Run(30);
	const NeedStats After = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	VT_CHECK_EQ(After.FamineDeaths, S.FamineDeaths);
	VT_CHECK_EQ(After.PlagueDeaths, S.PlagueDeaths);
	VT_CHECK_EQ(After.Hungry, 0u);
	VT_CHECK(After.HealthSum > uint64{After.WithNeeds} * 200);
	VT_CHECK(W.Counts(Region)->Total > AtRelief);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	// Rules matter: without a plague's bite, no plague death; without a cut, no famine.
	NeedRules Immune;
	for (uint32 K = 0; K < 3; ++K)
	{
		Immune.PlagueSharePerMille[K] = 0;
		Immune.DroughtCutPerMille[K] = 0;
	}
	Run X(AelvorSeed, Cursed(), Immune);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(X.Promote(Region));
	for (uint32 Year = 1; Year <= CursedYears; ++Year)
	{
		VT_REQUIRE(Curse(X, Region, Year % 2 == 1 ? DisasterKind::Drought : DisasterKind::Plague));
		X.Ages.Run(1);
	}
	const NeedStats SX = MeasureNeeds(X.Instance, X.Persons, X.Needs, Region);
	VT_CHECK_EQ(SX.PlagueDeaths, 0u);
	VT_CHECK_EQ(SX.FamineDeaths, 0u);
	VT_CHECK_EQ(SX.Hungry, 0u);
	VT_CHECK(SX.NaturalDeaths > 0);
}

VAELEN_TEST(Needs, ARegionPastItsCapacityStarvesWithoutADisaster)
{
	PreHistoryRules Calm;
	for (uint32 K = 0; K < static_cast<uint32>(DisasterKind::Count); ++K)
	{
		Calm.Disasters.OmenPerMille[K] = 0;
	}
	Run W(AelvorSeed, Calm);
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	RegionPopulation* Counts = W.Counts(Region);
	VT_REQUIRE(Counts != nullptr);
	const uint32 Start = Counts->Total;
	Counts->Capacity = Start / 4; // the land shrinks under them
	const usize Mark = W.Instance.Log().Count();
	W.Ages.Run(20);
	const NeedStats S = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	const CauseCheck C = CheckCauses(W.Instance, Region, Mark);
	VAELEN_LOG_INFO(LogNeeds, "shrunk region %u: %u -> %u people, %u starved, %u famine, %u plague, %u caused", Region,
					Start, W.Counts(Region)->Total, S.StarvationDeaths, S.FamineDeaths, S.PlagueDeaths, C.Caused);
	VT_CHECK(S.StarvationDeaths > 0);
	VT_CHECK_EQ(S.FamineDeaths, 0u);
	VT_CHECK_EQ(S.PlagueDeaths, 0u);
	VT_CHECK_EQ(C.Caused, 0u); // no disaster to blame
	VT_CHECK(W.Counts(Region)->Total < Start);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	// Without a detailed region the need system does nothing.
	Run Q(11, Calm);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(64), 60));
	const Hash64 Before = ComputeStateDigest(Q.Instance);
	RandomStream Stream(1);
	TickContext Tc;
	Tc.Tick = Q.Instance.Now();
	Tc.Entities = &Q.Instance.Entities();
	Tc.Components = &Q.Instance.Components();
	Tc.Random = &Stream;
	Tc.Events = &Q.Instance.Events();
	Q.Body->Tick(Tc);
	VT_CHECK_EQ(ComputeStateDigest(Q.Instance), Before);
}

VAELEN_TEST(Needs, DeterministicAndSnapshotSafe)
{
	Run A(AelvorSeed, Cursed());
	Run B(AelvorSeed, Cursed());
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_REQUIRE(A.Promote(Region));
	VT_REQUIRE(B.Promote(Region));
	for (uint32 Year = 1; Year <= 6; ++Year)
	{
		VT_REQUIRE(Curse(A, Region, Year % 2 == 0 ? DisasterKind::Plague : DisasterKind::Drought));
		VT_REQUIRE(Curse(B, Region, Year % 2 == 0 ? DisasterKind::Plague : DisasterKind::Drought));
		A.Ages.Run(1);
		B.Ages.Run(1);
	}
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Instance.Log().Digest(), B.Instance.Log().Digest());
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed, Cursed());
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(MeasureNeeds(R.Instance, R.Persons, R.Needs, Region).WithNeeds,
				MeasureNeeds(A.Instance, A.Persons, A.Needs, Region).WithNeeds);
	A.Ages.Run(20);
	R.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK(IsConsistent(R.Instance, R.Ages.Types(), R.Persons, Region));
	VT_CHECK(MeasureNeeds(R.Instance, R.Persons, R.Needs, Region).CausedDeaths > 0);
}

VAELEN_TEST(Needs, FrozenNeedsAreReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	W.Ages.Run(200);
	const DetailStats D = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
	const NeedStats S = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	VAELEN_LOG_INFO(LogNeeds, "frozen: needs128=%016llx alive=%u caused=%u health=%llu",
					static_cast<unsigned long long>(D.PersonsDigest), L.Alive, S.CausedDeaths,
					static_cast<unsigned long long>(S.HealthSum));
	VT_CHECK_EQ(D.PersonsDigest, Hash64{VAELEN_NEEDS_FROZEN_128});
	VT_CHECK_EQ(L.Alive, uint32{VAELEN_NEEDS_ALIVE_128});
	VT_CHECK_EQ(S.CausedDeaths, uint32{VAELEN_NEEDS_CAUSED_128});
	VT_CHECK_EQ(S.HealthSum, uint64{VAELEN_NEEDS_HEALTH_128});
	VT_CHECK_EQ(D.Inconsistent, 0u);
}
