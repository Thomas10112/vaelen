// VAELEN - Tests/Society
// Phase 05.08: the Phase 05 gate - five hundred years with a detailed region
// over the AELVOR 256 pre-history, every Phase 04 and 05 system on, every
// invariant checked each decade, frozen digests.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Decisions.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/SocietyHistory.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.08): AELVOR 256 after
// 300 years of pre-history, the busiest region detailed and lived through 500
// years with every Phase 04 and 05 system, the customs as drawn.
// Refrozen 2026-09-08: person indices are taken from a counter that only ever
// goes up, so a demoted region no longer hands its indices out again to the
// people made after it, and the world's one counter entity is state like any
// other. Every invariant of the gate is unchanged; only the state digests are.
#define VAELEN_SOCGATE_FROZEN_256_250 0x5e183505faa6ea10ull
#define VAELEN_SOCGATE_FROZEN_256_500 0xbbfcd4c296cd2fdfull
#define VAELEN_SOCGATE_LOG_256_500 0xd7b7173341039dc8ull
#define VAELEN_SOCGATE_TEXT_256_500 0xe719182682fdfd8eull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSocietyGate);

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
			PersonRecords = PersonChronicleTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Decisions = DecisionTypes::Declare(Instance);
			State = SocietyChronicleTypes::Declare(Instance);
			Context = SocietyContext{Persons, Families, Organizations};
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Persons_ = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families, PersonRecords,
														 PersonChronicleRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage,
													BondageRules{});
			Acts = std::make_unique<DecisionSystem>(Instance, Ages.Types(), Persons, Traits, Organizations, Decisions,
													DecisionRules{});
			Society_ =
				std::make_unique<SocietyChronicle>(Instance, Ages.Types(), Context, State, SocietyChronicleRules{});
			Houses->RunAfter("Needs");
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Body->ObserveStores(Decisions.Stores);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Orgs->RunAfter("Needs");
			Bonds->RunAfter("Lod");
			Ranks->ObserveBonds(Bondage.Bond);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Acts.get());
			Persons_->Attach();
			Society_->Attach();
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
		PersonChronicleTypes PersonRecords;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		DecisionTypes Decisions;
		SocietyChronicleTypes State;
		SocietyContext Context;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<PersonChronicle> Persons_;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<DecisionSystem> Acts;
		std::unique_ptr<SocietyChronicle> Society_;
	};

	// Every invariant Phase 05 promises, on the live state. Returns the failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, Run& W, uint32 Year, uint32 Region, bool WithChronicle)
	{
		uint32 Failures = 0;
		const World& World_ = W.Instance;
		const PreHistoryTypes& T = W.Ages.Types();
		const DetailStats D = MeasureDetail(World_, T, W.Persons);
		if (D.DetailedRegions != 1 || D.Inconsistent != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u detailed regions, %u inconsistent", Year, D.DetailedRegions,
						 D.Inconsistent);
		}
		const LifeStats L = MeasureLives(World_, W.Persons, Region, World_.Now());
		if (L.Alive == 0 || L.Oldest >= 110)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u alive, oldest %u", Year, L.Alive, L.Oldest);
		}
		const OrganizationStats O = MeasureOrganizations(World_, T, W.Persons, W.Organizations);
		if (O.Astray != 0 || O.CountMismatch != 0 || O.HeadsAlive != O.Alive)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u astray memberships, %u count mismatches, %u heads for %u organisations",
						 Year, O.Astray, O.CountMismatch, O.HeadsAlive, O.Alive);
		}
		const StandingStats R = MeasureStanding(World_, W.Persons, W.Standing, Region);
		if (R.Stale != 0 || R.PerTier[0] + R.PerTier[1] + R.PerTier[2] != R.Ranked)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale standings", Year, R.Stale);
		}
		const NormStats N = MeasureNorms(World_, T, W.Norms);
		if (N.WithNorms != N.Cultures || N.MirrorMismatch != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u of %u cultures with customs, %u mirrors wrong", Year, N.WithNorms,
						 N.Cultures, N.MirrorMismatch);
		}
		const BondageStats B = MeasureBondage(World_, T, W.Persons, W.Bondage, Region);
		const RegionStrata* St = StrataOf(World_, T, W.Bondage, Region);
		if (B.Stale != 0 || B.HolderLost != 0 || St == nullptr || St->Bonded != B.Bonded ||
			St->Enslaved != B.Enslaved || St->Free + St->Bonded + St->Enslaved != L.Alive)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale bonds, %u holders lost, strata %s", Year, B.Stale, B.HolderLost,
						 St == nullptr ? "missing" : "wrong");
		}
		// Every bond's entry and every caused decision resolve to an event in the log.
		uint32 BadCause = 0;
		for (const Event& E : World_.Log().All())
		{
			if (!E.Cause.IsValid() || !(E.Is(BondEnteredEvent) || E.Is(BondLeftEvent) || E.Is(DecisionMadeEvent)))
			{
				continue;
			}
			BadCause += FindEvent(World_.Log(), E.Cause) == nullptr ? 1u : 0u;
		}
		if (BadCause != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u society causes missing from the log", Year, BadCause);
		}
		if (!WithChronicle)
		{
			return Failures;
		}
		const ChronicleStats C = CheckChronicle(World_, T);
		const PersonChronicleStats PC = CheckPersonChronicle(World_, T, W.Persons, W.Families, W.PersonRecords);
		const SocietyChronicleStats SC = CheckSocietyChronicle(World_, T, W.Context, W.State);
		if (C.Resolved != C.Records || C.EraConsistent != C.Records || PC.Described != PC.Records ||
			SC.Described != SC.Records)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: chronicle %u/%u resolved, %u/%u person and %u/%u society records described",
						 Year, C.Resolved, C.Records, PC.Described, PC.Records, SC.Described, SC.Records);
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(SocietyGate, FiveHundredYearsWithADetailedRegionAt256HoldEveryInvariantAndFreeze)
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
			const OrganizationStats O = MeasureOrganizations(W.Instance, W.Ages.Types(), W.Persons, W.Organizations);
			const StandingStats R = MeasureStanding(W.Instance, W.Persons, W.Standing, Region);
			const BondageStats B = MeasureBondage(W.Instance, W.Ages.Types(), W.Persons, W.Bondage, Region);
			const DecisionStats Dc = MeasureDecisions(W.Instance, W.Ages.Types(), W.Decisions);
			const NormStats N = MeasureNorms(W.Instance, W.Ages.Types(), W.Norms);
			const SocietyChronicleStats SC = CheckSocietyChronicle(W.Instance, W.Ages.Types(), W.Context, W.State);
			VAELEN_LOG_INFO(
				LogSocietyGate,
				"year %u: %u organisations (%u alive, %u members), %u ranked (%u elite), %u bonded, %u enslaved, %u "
				"decisions (%u raids), %u customs drifted, %u society records (%u dropped)",
				Decade * 10, O.Total, O.Alive, O.Members, R.Ranked, R.PerTier[2], B.Bonded, B.Enslaved, Dc.Decisions,
				Dc.Raids, N.Drifts, SC.Records, SC.Dropped);
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
	const Hash64 Log = W.Instance.Log().Digest();
	std::string Text;
	ExportChronicleWithSociety(W.Instance, W.Ages.Types(), W.Context, Text, 0);
	const Hash64 TextDigest = HashString(Text);
	VAELEN_LOG_INFO(
		LogSocietyGate,
		"gate: 500 years at 256 with region %u detailed in %.1f s [asserts %s]; frozen: 250=%016llx 500=%016llx "
		"log=%016llx text=%016llx",
		Region, Elapsed, VAELEN_ASSERTS_ENABLED ? "on" : "off", static_cast<unsigned long long>(At250),
		static_cast<unsigned long long>(At500), static_cast<unsigned long long>(Log),
		static_cast<unsigned long long>(TextDigest));
	// The society lived: organisations of every kind, an elite, bonds, decisions, customs that drifted.
	const OrganizationStats O = MeasureOrganizations(W.Instance, W.Ages.Types(), W.Persons, W.Organizations);
	const DecisionStats Dc = MeasureDecisions(W.Instance, W.Ages.Types(), W.Decisions);
	const NormStats N = MeasureNorms(W.Instance, W.Ages.Types(), W.Norms);
	VT_CHECK(O.PerKind[0] > 0 && O.PerKind[1] > 0 && O.PerKind[2] > 0 && O.PerKind[3] > 0);
	VT_CHECK(Dc.Decisions > 500 && Dc.Raids > 10);
	VT_CHECK(N.Drifts > 0);
	VT_CHECK(Text.find(" was founded.") != std::string::npos && Text.find(" planned a raid on ") != std::string::npos);
	VT_CHECK_EQ(At250, Hash64{VAELEN_SOCGATE_FROZEN_256_250});
	VT_CHECK_EQ(At500, Hash64{VAELEN_SOCGATE_FROZEN_256_500});
	VT_CHECK_EQ(Log, Hash64{VAELEN_SOCGATE_LOG_256_500});
	VT_CHECK_EQ(TextDigest, Hash64{VAELEN_SOCGATE_TEXT_256_500});
	// The snapshot of year 250 restored into a fresh object continues to the same year 500.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(IsDetailed(R.Instance, R.Ages.Types(), R.Persons, Region));
	const auto Again = std::chrono::steady_clock::now();
	R.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), At500);
	VT_CHECK_EQ(R.Instance.Log().Digest(), Log);
	std::string TextR;
	ExportChronicleWithSociety(R.Instance, R.Ages.Types(), R.Context, TextR, 0);
	VT_CHECK(TextR == Text);
	VAELEN_LOG_INFO(LogSocietyGate, "snapshot: %llu bytes at year 250, the same year 500 after %.1f s",
					static_cast<unsigned long long>(Image.size()), Seconds(Again));
}
