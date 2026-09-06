// VAELEN - Tests/Population
// Phase 04.08: the Phase 04 gate - five hundred years with a detailed region
// over the AELVOR 256 pre-history, every Phase 04 system on, every invariant
// checked each decade, frozen digests.
//
// STATUS: VALIDATED (Phase 04)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.08): AELVOR 256 after
// 300 years of pre-history, the busiest region detailed and lived through 500
// years with lives, families, needs, traits, the bridge and the chronicle.
#define VAELEN_POPGATE_FROZEN_256_250 0x04f8a05bee8c3313ull
#define VAELEN_POPGATE_FROZEN_256_500 0xde9467e086c9560eull
#define VAELEN_POPGATE_PERSONS_256_500 0xf5f4643d97ce5068ull
#define VAELEN_POPGATE_LOG_256_500 0xba7af3a8312954d8ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPopulationGate);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	double Seconds(std::chrono::steady_clock::time_point Start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	}

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			State = PersonChronicleTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Chronicle_ = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families, State,
														   PersonChronicleRules{});
			// Heads and spouses follow every death and departure of the year.
			Houses->RunAfter("Needs");
			Houses->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Chronicle_->Attach();
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
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		NeedTypes Needs;
		TraitTypes Traits;
		LodTypes Lod;
		PersonChronicleTypes State;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<PersonChronicle> Chronicle_;
	};

	// Every invariant Phase 04 promises, on the live state. Returns the failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, Run& W, uint32 Year, uint32 Region, bool WithChronicle)
	{
		uint32 Failures = 0;
		const World& World_ = W.Instance;
		const PreHistoryTypes& T = W.Ages.Types();
		// One detailed region, the one asked for, and its two grains agree.
		const DetailStats D = MeasureDetail(World_, T, W.Persons);
		if (D.DetailedRegions != 1 || !IsDetailed(World_, T, W.Persons, Region))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u detailed regions", Year, D.DetailedRegions);
		}
		if (D.Inconsistent != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the persons and the counts disagree", Year);
		}
		// Every living person has needs, traits and a name; nobody outlives the last band.
		const LifeStats L = MeasureLives(World_, W.Persons, Region, World_.Now());
		const NeedStats N = MeasureNeeds(World_, W.Persons, W.Needs, Region);
		const TraitStats Tr = MeasureTraits(World_, T, W.Persons, W.Traits, Region);
		if (L.Alive == 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the detailed region is empty", Year);
		}
		if (N.WithNeeds != L.Alive || Tr.WithTraits != L.Alive || Tr.Unnamed != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u alive, %u with needs, %u with traits, %u unnamed", Year, L.Alive,
						 N.WithNeeds, Tr.WithTraits, Tr.Unnamed);
		}
		if (L.Oldest >= 110)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: someone is %u", Year, L.Oldest);
		}
		// Families: no broken spouse link, heads alive and in their family.
		const FamilyStats F = MeasureFamilies(World_, W.Persons, W.Families, FamilyRules{}, World_.Now());
		if (F.Broken != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u broken spouse links", Year, F.Broken);
		}
		uint32 BadHeads = 0;
		const PersonIndex Index = BuildPersonIndex(World_, W.Persons);
		World_.Components()
			.GetPool(W.Families.Family)
			.ForEach(
				[&](EntityHandle, const FamilyInfo& Fam)
				{
					if (Fam.Head == 0)
					{
						return;
					}
					const PersonInfo* Head =
						Fam.Head < Index.Handles.size() && !Index.Handles[Fam.Head].IsNull()
							? World_.Components().GetPool(W.Persons.Person).TryGet(Index.Handles[Fam.Head])
							: nullptr;
					BadHeads += Head == nullptr || Head->State != static_cast<uint8>(LifeState::Alive) ||
										Head->Family != Fam.Index
									? 1u
									: 0u;
				});
		if (BadHeads != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u heads dead or astray", Year, BadHeads);
		}
		// Every caused death points at a disaster of the region; the counts respect capacity.
		uint32 BadCause = 0;
		for (const Event& E : World_.Log().All())
		{
			if (!E.Is(PersonDiedEvent) || !E.Cause.IsValid())
			{
				continue;
			}
			const Event* C = FindEvent(World_.Log(), E.Cause);
			BadCause += C != nullptr && C->Is(DisasterStruckEvent) &&
								C->Get<DisasterPayload>().Region == E.Get<PersonPayload>().Region
							? 0u
							: 1u;
		}
		if (BadCause != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u deaths with a wrong cause", Year, BadCause);
		}
		World_.Components()
			.GetPool(T.Population.Population)
			.ForEach(
				[&](EntityHandle H, const RegionPopulation& P)
				{
					RegionPopulation Copy = P;
					Copy.Recount();
					if (Copy.Total != P.Total || Copy.Majority != P.Majority)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: region population bookkeeping stale", Year);
					}
					const RegionFaith* Faith = World_.Components().GetPool(T.Religion.Faith).TryGet(H);
					const RegionInfo* Info = World_.Components().GetPool(T.World.RegionTypes_.Region).TryGet(H);
					if (Info != nullptr && Info->Index == Region && Faith != nullptr && Faith->Total() > P.Total)
					{
						++Failures;
						VT_CHECK_MSG(false, "year %u: %u believers for %u people in the detailed region", Year,
									 Faith->Total(), P.Total);
					}
				});
		// The chronicle resolves completely, person records included (every fifty
		// years: it is text over thousands of records).
		if (!WithChronicle)
		{
			return Failures;
		}
		const ChronicleStats C = CheckChronicle(World_, T);
		const PersonChronicleStats PC = CheckPersonChronicle(World_, T, W.Persons, W.Families, W.State);
		if (C.Resolved != C.Records || C.EraConsistent != C.Records || PC.Described != PC.Records)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: chronicle %u/%u resolved, %u/%u person records described", Year, C.Resolved,
						 C.Records, PC.Described, PC.Records);
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(PopulationGate, FiveHundredYearsWithADetailedRegionAt256HoldEveryInvariantAndFreeze)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(256), 300));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(Region != 0);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	const auto Start = std::chrono::steady_clock::now();
	uint32 Failures = 0;
	Hash64 At250 = 0;
	std::vector<uint8> Image;
	for (uint32 Decade = 1; Decade <= 50; ++Decade)
	{
		W.Ages.Run(10);
		Failures += CheckInvariants(Ctx, W, Decade * 10, Region, Decade % 5 == 0);
		if (Failures > 20)
		{
			break;
		}
		if (Decade % 10 == 0)
		{
			const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
			const FamilyStats F = MeasureFamilies(W.Instance, W.Persons, W.Families, FamilyRules{}, W.Instance.Now());
			const NeedStats N = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
			const LodStats B = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
			const PersonChronicleStats PC =
				CheckPersonChronicle(W.Instance, W.Ages.Types(), W.Persons, W.Families, W.State);
			VAELEN_LOG_INFO(LogPopulationGate,
							"year %u: %u alive (%u children, %u elders), %u families (%u extinct), %u married; deaths "
							"famine %u plague %u natural %u; %u left, %u arrived; %u person records (%u dropped)",
							Decade * 10, L.Alive, L.Children, L.Elders, F.Families, F.Extinct, F.Married,
							N.FamineDeaths, N.PlagueDeaths, N.NaturalDeaths, B.Emigrants, B.Immigrants, PC.Records,
							PC.Dropped);
		}
		if (Decade == 25)
		{
			At250 = ComputeStateDigest(W.Instance);
			SaveSnapshot(W.Instance, Image);
		}
	}
	const double Elapsed = Seconds(Start);
	VT_CHECK_EQ(Failures, 0u);
	const Hash64 At500 = ComputeStateDigest(W.Instance);
	const DetailStats D = MeasureDetail(W.Instance, W.Ages.Types(), W.Persons);
	const Hash64 Log = W.Instance.Log().Digest();
	VAELEN_LOG_INFO(LogPopulationGate,
					"gate: 500 years at 256 with region %u detailed in %.1f s [asserts %s]; frozen: 250=%016llx "
					"500=%016llx persons=%016llx log=%016llx",
					Region, Elapsed, VAELEN_ASSERTS_ENABLED ? "on" : "off", static_cast<unsigned long long>(At250),
					static_cast<unsigned long long>(At500), static_cast<unsigned long long>(D.PersonsDigest),
					static_cast<unsigned long long>(Log));
	// The region lived: generations passed, houses rose and fell, the chronicle tells it.
	const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
	const FamilyStats F = MeasureFamilies(W.Instance, W.Persons, W.Families, FamilyRules{}, W.Instance.Now());
	const PersonChronicleStats PC = CheckPersonChronicle(W.Instance, W.Ages.Types(), W.Persons, W.Families, W.State);
	VT_CHECK(L.Alive > 0 && L.Dead > L.Alive * 5); // twenty generations passed
	VT_CHECK(F.Families > 10 && F.Extinct > 0);
	VT_CHECK(PC.Records > 100);
	std::string Tail;
	ExportChronicleWithPersons(W.Instance, W.Ages.Types(), W.Persons, W.Families, Tail, 0);
	VT_CHECK(Tail.find(" founded a house in ") != std::string::npos);
	// The rest of the world went on as in Phase 03.
	const PreHistoryReport R = ReportPreHistory(W.Instance, W.Ages.Types());
	VT_CHECK(R.Population.People * 2 > R.Population.Capacity);
	VT_CHECK(R.Population.Cultures >= 4);
	VT_CHECK_EQ(At250, Hash64{VAELEN_POPGATE_FROZEN_256_250});
	VT_CHECK_EQ(At500, Hash64{VAELEN_POPGATE_FROZEN_256_500});
	VT_CHECK_EQ(D.PersonsDigest, Hash64{VAELEN_POPGATE_PERSONS_256_500});
	VT_CHECK_EQ(Log, Hash64{VAELEN_POPGATE_LOG_256_500});
	// The snapshot of year 250 restored into a fresh object continues to the same year 500.
	VT_REQUIRE(!Image.empty());
	Run Restored(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(Restored.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(IsDetailed(Restored.Instance, Restored.Ages.Types(), Restored.Persons, Region));
	VT_CHECK(IsWanted(Restored.Instance, Restored.Lod, Region));
	const auto Again = std::chrono::steady_clock::now();
	Restored.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(Restored.Instance), At500);
	VT_CHECK_EQ(Restored.Instance.Log().Digest(), Log);
	VT_CHECK_EQ(MeasureDetail(Restored.Instance, Restored.Ages.Types(), Restored.Persons).PersonsDigest,
				D.PersonsDigest);
	VAELEN_LOG_INFO(LogPopulationGate, "snapshot: %llu bytes at year 250, the same year 500 after %.1f s",
					static_cast<unsigned long long>(Image.size()), Seconds(Again));
}
