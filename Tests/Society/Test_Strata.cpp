// VAELEN - Tests/Society
// Phase 05.06: the social shape across the grains - strata kept through a
// demotion and honoured at a promotion, five hundred years of alternation.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
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
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.06): AELVOR 64 at
// year 120, two of the three busiest regions detailed in turn every 25 years
// for 500 years with every Phase 04 and 05 system so far, every institution
// allowed.
#define VAELEN_STRATA_FROZEN_64 0x7d6be89760aa4807ull
#define VAELEN_STRATA_BOUND_64 1168u
#define VAELEN_STRATA_PROMOTIONS_64 21u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogStrata);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint32 AllInstitutions = static_cast<uint32>(Bondage::Debt) | static_cast<uint32>(Bondage::Capture) |
									   static_cast<uint32>(Bondage::Birth);

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InRules = BondageRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage, InRules);
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Bonds->RunAfter("Lod");
			Ranks->ObserveBonds(Bondage.Bond);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Bonds.get());
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
		void AllowEverywhere(uint32 Bits)
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
							Copy.BondageAllowed = Bits;
							SetNorms(Instance, Ages.Types(), Norms, C.Index, Copy);
						}
					});
		}
		BondageStats Stats(uint32 Region = 0) const
		{
			return MeasureBondage(Instance, Ages.Types(), Persons, Bondage, Region);
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<BondageSystem> Bonds;
	};

	/// Alternation: every 25 years another pair of the three busiest regions is wanted.
	void Want(Run& W, const std::vector<uint32>& Ranked, uint32 Year)
	{
		if (Year % 25 != 1)
		{
			return;
		}
		const uint32 Turn = (Year / 25) % 3;
		for (uint32 i = 0; i < 3; ++i)
		{
			if (i == Turn)
			{
				ReleaseDetail(W.Instance, W.Lod, Ranked[i]);
			}
			else
			{
				RequestDetail(W.Instance, W.Lod, Ranked[i]);
			}
		}
	}
} // namespace

VAELEN_TEST(Strata, ThePromotionBindsAgainWhatTheDemotionKept)
{
	BondageRules Still;
	Still.ManumissionPerMille = 0;
	Still.FlightPerMille = 0;
	Still.DebtPerMille = 40;
	Still.HardenAfterYears = 5;
	Run W(AelvorSeed, Still);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(AllInstitutions);
	const uint32 Region = W.Ranked()[0];
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(20);
	const RegionStrata Before = *StrataOf(W.Instance, W.Ages.Types(), W.Bondage, Region);
	VT_REQUIRE(Before.Bonded > 0 && Before.Enslaved > 0);
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	const RegionStrata Coarse = *StrataOf(W.Instance, W.Ages.Types(), W.Bondage, Region);
	VT_CHECK_EQ(Coarse.Bonded, Before.Bonded);
	VT_CHECK_EQ(Coarse.Enslaved, Before.Enslaved);
	// A promote / demote cycle without a tick leaves the strata untouched: no bond
	// remains, the strata are the whole digest.
	const Hash64 Digest = W.Stats().Digest;
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	VT_CHECK_EQ(W.Stats().Digest, Digest);
	// Detailed again: the strata bind as many of the new persons the same tick, by the strata's entry.
	Still.DebtPerMille = 0;
	const usize Mark = W.Instance.Log().Count();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	const BondageStats S = W.Stats(Region);
	VAELEN_LOG_INFO(
		LogStrata, "region %u: %u bonded and %u enslaved kept; %u bonded and %u enslaved bound again, %u by the strata",
		Region, Before.Bonded, Before.Enslaved, S.Bonded, S.Enslaved, S.Entered[4]);
	VT_CHECK(S.Entered[4] >= Before.Bonded + Before.Enslaved - 5); // within the year's deaths and the holders' room
	VT_CHECK(S.Enslaved >= Before.Enslaved - 5 && S.Enslaved <= Before.Enslaved);
	VT_CHECK_EQ(S.Stale, 0u);
	VT_CHECK_EQ(S.HolderLost, 0u);
	uint32 Promoted = 0;
	const std::vector<Event>& All = W.Instance.Log().All();
	for (usize i = Mark; i < All.size(); ++i)
	{
		if (All[i].Is(BondEnteredEvent))
		{
			const BondPayload P = All[i].Get<BondPayload>();
			Promoted += P.Reason == static_cast<uint32>(BondEntry::Promotion) ? 1u : 0u;
			VT_CHECK_EQ(P.Region, Region);
		}
	}
	VT_CHECK_EQ(Promoted, S.Entered[4]);
	// The strata now say what the persons say again.
	const RegionStrata After = *StrataOf(W.Instance, W.Ages.Types(), W.Bondage, Region);
	VT_CHECK_EQ(After.Bonded, S.Bonded);
	VT_CHECK_EQ(After.Enslaved, S.Enslaved);
	VT_CHECK(BondEntryName(BondEntry::Promotion)[0] == 't' && BondExitName(BondExit::Departure)[0] == 'd');
}

VAELEN_TEST(Strata, TheDepartedLeaveTheirBondsBehind)
{
	BondageRules Still;
	Still.ManumissionPerMille = 0;
	Still.FlightPerMille = 0;
	Still.DebtPerMille = 60;
	Run W(AelvorSeed, Still);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(AllInstitutions);
	const uint32 Region = W.Ranked()[0];
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(5);
	// The land shrinks under them: the crowd leaves, the bound among them leave their bond.
	RegionPopulation* Counts = nullptr;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				if (R.Index == Region)
				{
					Counts = W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				}
			});
	VT_REQUIRE(Counts != nullptr);
	Counts->Capacity = Counts->Total / 2;
	W.Ages.Run(4);
	const BondageStats S = W.Stats(Region);
	const LodStats B = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
	VAELEN_LOG_INFO(LogStrata, "region %u: %u left for the coarse grain, %u bonds left by departure, %u by death",
					Region, B.Emigrants, S.Left[5], S.Left[4]);
	VT_CHECK(B.Emigrants > 0);
	VT_CHECK(S.Left[5] > 0);
	VT_CHECK_EQ(S.Stale, 0u);
	// No bond sits on a gone person.
	uint32 OnGone = 0;
	W.Instance.Components()
		.GetPool(W.Bondage.Bond)
		.ForEach(
			[&](EntityHandle H, const BondState&)
			{
				const PersonInfo* P = W.Instance.Components().GetPool(W.Persons.Person).TryGet(H);
				OnGone += P != nullptr && P->State == static_cast<uint8>(LifeState::Gone) ? 1u : 0u;
			});
	VT_CHECK_EQ(OnGone, 0u);
}

VAELEN_TEST(Strata, FiveHundredYearsOfAlternationKeepTheShape)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	A.AllowEverywhere(AllInstitutions);
	B.AllowEverywhere(AllInstitutions);
	const std::vector<uint32> Ranked = A.Ranked();
	VT_REQUIRE(Ranked.size() >= 3);
	uint32 Failures = 0;
	uint32 Bound = 0;
	std::vector<uint8> Image;
	for (uint32 Year = 1; Year <= 500; ++Year)
	{
		Want(A, Ranked, Year);
		Want(B, Ranked, Year);
		A.Ages.Run(1);
		B.Ages.Run(1);
		if (Year == 250)
		{
			SaveSnapshot(A.Instance, Image);
		}
		if (Year % 10 != 0)
		{
			continue;
		}
		const BondageStats S = A.Stats();
		const OrganizationStats O = MeasureOrganizations(A.Instance, A.Ages.Types(), A.Persons, A.Organizations);
		const StandingStats R = MeasureStanding(A.Instance, A.Persons, A.Standing, 0);
		const NormStats N = MeasureNorms(A.Instance, A.Ages.Types(), A.Norms);
		Bound = std::max(Bound, S.Bonded + S.Enslaved);
		// Strata of every detailed region equal its living by kind; coarse strata never exceed the living.
		uint32 BadStrata = 0;
		A.Instance.Components()
			.GetPool(A.Ages.Types().World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const RegionInfo& Rg)
				{
					const RegionStrata* St = A.Instance.Components().GetPool(A.Bondage.Strata).TryGet(H);
					const RegionPopulation* P =
						A.Instance.Components().GetPool(A.Ages.Types().Population.Population).TryGet(H);
					if (St == nullptr || P == nullptr)
					{
						return;
					}
					if (IsDetailed(A.Instance, A.Ages.Types(), A.Persons, Rg.Index))
					{
						const BondageStats Here = A.Stats(Rg.Index);
						const LifeStats L = MeasureLives(A.Instance, A.Persons, Rg.Index, A.Instance.Now());
						BadStrata += St->Bonded != Here.Bonded || St->Enslaved != Here.Enslaved ||
											 St->Free + St->Bonded + St->Enslaved != L.Alive
										 ? 1u
										 : 0u;
					}
					else
					{
						BadStrata += St->Bonded + St->Enslaved > P->Total + P->Total / 2 + 100 ? 1u : 0u;
					}
				});
		if (S.Stale != 0 || S.HolderLost != 0 || O.Astray != 0 || O.CountMismatch != 0 || R.Stale != 0 ||
			N.MirrorMismatch != 0 || BadStrata != 0)
		{
			++Failures;
			VT_CHECK_MSG(false,
						 "year %u: %u stale bonds, %u holders lost, %u astray, %u mismatched, %u stale standings, %u "
						 "mirrors, %u strata wrong",
						 Year, S.Stale, S.HolderLost, O.Astray, O.CountMismatch, R.Stale, N.MirrorMismatch, BadStrata);
		}
		if (ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the two worlds differ", Year);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK(Bound > 0);
	const LodStats L = MeasureLod(A.Instance, A.Ages.Types(), A.Persons, A.Lod);
	const BondageStats S = A.Stats();
	VAELEN_LOG_INFO(
		LogStrata,
		"500 years: %u promotions, %u demotions; bonds entered %u by debt, %u by birth, %u by the strata; left %u by "
		"manumission, %u by flight, %u by the holder's death, %u by death, %u by departure; digest %016llx",
		L.Promotions, L.Demotions, S.Entered[1], S.Entered[2], S.Entered[4], S.Left[1], S.Left[2], S.Left[3], S.Left[4],
		S.Left[5], static_cast<unsigned long long>(S.Digest));
	VT_CHECK(S.Entered[4] > 0); // promotions bound again
	VT_CHECK(L.Promotions >= 20);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_STRATA_FROZEN_64});
	VT_CHECK_EQ(S.Entered[4], uint32{VAELEN_STRATA_BOUND_64});
	VT_CHECK_EQ(L.Promotions, uint32{VAELEN_STRATA_PROMOTIONS_64});
	// The snapshot of year 250 continues to the same year 500.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	for (uint32 Year = 251; Year <= 500; ++Year)
	{
		Want(R, Ranked, Year);
		R.Ages.Run(1);
	}
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, S.Digest);
}
