// VAELEN - Tests/Society
// Phase 05.04: bondage and slavery - entries by debt and birth, exits, holders,
// strata kept per region.
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.04): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 200 years with
// every Phase 04 and 05 system so far, every institution allowed.
#define VAELEN_BONDAGE_FROZEN_128 0x99ec62d866000c68ull
#define VAELEN_BONDAGE_ENTERED_128 2888u
#define VAELEN_BONDAGE_LEFT_128 2227u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogBondage);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

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
			Ranks->ObserveBonds(Bondage.Bond);
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
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
		/// Sets the bondage customs of every culture at once.
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

	constexpr uint32 All = static_cast<uint32>(Bondage::Debt) | static_cast<uint32>(Bondage::Capture) |
						   static_cast<uint32>(Bondage::Birth);
} // namespace

VAELEN_TEST(Bondage, DebtBindsTheCommonToTheEliteWhereTheCultureAllows)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(All);
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(10);
	const BondageStats S = W.Stats(Region);
	VAELEN_LOG_INFO(LogBondage,
					"region %u after ten years: %u bonded, %u enslaved; entered by debt %u, birth %u; left by "
					"manumission %u, flight %u, holder's death %u, death %u (%u caused)",
					Region, S.Bonded, S.Enslaved, S.Entered[1], S.Entered[2], S.Left[1], S.Left[2], S.Left[3],
					S.Left[4], S.Caused);
	VT_CHECK(S.Bonded > 10);
	VT_CHECK_EQ(S.Enslaved, 0u); // nothing hardens in ten years
	VT_CHECK(S.Entered[1] > S.Bonded);
	VT_CHECK_EQ(S.Entered[2], 0u);
	VT_CHECK_EQ(S.Stale, 0u);
	VT_CHECK_EQ(S.HolderLost, 0u);
	// Every bond: a living common adult of the region, held by a living member of the elite, never over the cap.
	std::vector<std::pair<uint32, uint32>> Held;
	uint32 Bad = 0;
	const uint64 YearTick = W.Instance.Now() - TicksPerYear; // the last yearly tick
	W.Instance.Components()
		.GetPool(W.Bondage.Bond)
		.ForEach(
			[&](EntityHandle H, const BondState& B)
			{
				const PersonInfo* P = W.Instance.Components().GetPool(W.Persons.Person).TryGet(H);
				VT_REQUIRE(P != nullptr);
				// The bound are not ranked; the holder is a living person of the region (a holder may fall from the
				// elite later).
				const PersonStanding* St = W.Instance.Components().GetPool(W.Standing.Standing).TryGet(H);
				// (Those bound at the last tick keep that tick's standing until the next one.)
				Bad += P->Region != Region || P->State != static_cast<uint8>(LifeState::Alive) ||
							   (St != nullptr && B.Since < YearTick) ||
							   B.Entry != static_cast<uint8>(BondEntry::Debt) || B.Holder == 0 ||
							   B.Since < TicksPerYear * 300
						   ? 1u
						   : 0u;
				const PersonInfo* Holder = FindPerson(W.Instance, W.Persons, B.Holder);
				Bad += Holder == nullptr || Holder->State != static_cast<uint8>(LifeState::Alive) ||
							   Holder->Region != Region
						   ? 1u
						   : 0u;
				bool Found = false;
				for (auto& [Index, Count] : Held)
				{
					if (Index == B.Holder)
					{
						++Count;
						Found = true;
					}
				}
				if (!Found)
				{
					Held.push_back({B.Holder, 1u});
				}
			});
	VT_CHECK_EQ(Bad, 0u);
	for (const auto& [Index, Count] : Held)
	{
		VT_CHECK(Count <= BondageRules{}.MaxHeldPerHolder);
	}
	VT_CHECK(Held.size() > 1);
	// The strata of the region count the living by kind.
	const RegionStrata* Strata = StrataOf(W.Instance, W.Ages.Types(), W.Bondage, Region);
	VT_REQUIRE(Strata != nullptr);
	const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
	VT_CHECK_EQ(Strata->Free + Strata->Bonded + Strata->Enslaved, L.Alive);
	VT_CHECK_EQ(Strata->Bonded, S.Bonded);
	VT_CHECK(StrataOf(W.Instance, W.Ages.Types(), W.Bondage, 0xfffffff0u) == nullptr);
	VT_CHECK(BondOf(W.Instance, W.Persons, W.Bondage, 0xfffffff0u) == nullptr);
	VT_CHECK(BondKindName(BondKind::Enslaved)[0] == 'e' && BondEntryName(BondEntry::Birth)[0] == 'b' &&
			 BondExitName(BondExit::Flight)[0] == 'f');
	// Forbidden everywhere: nobody is bound, and the bound are not freed for it.
	Run X(AelvorSeed);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	X.AllowEverywhere(0);
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	X.Ages.Run(10);
	const BondageStats SX = X.Stats();
	VT_CHECK_EQ(SX.Bonded + SX.Enslaved, 0u);
	VT_CHECK_EQ(SX.Entered[1] + SX.Entered[2], 0u);
	VT_CHECK(StrataOf(X.Instance, X.Ages.Types(), X.Bondage, Region) != nullptr);
	VT_CHECK_EQ(StrataOf(X.Instance, X.Ages.Types(), X.Bondage, Region)->Bonded, 0u);
}

VAELEN_TEST(Bondage, BondageHardensAndTheChildrenOfTheEnslavedAreBornEnslaved)
{
	BondageRules Fast;
	Fast.HardenAfterYears = 3;
	Fast.ManumissionPerMille = 0;
	Fast.FlightPerMille = 0;
	Fast.DebtPerMille = 40;
	Run W(AelvorSeed, Fast);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(All);
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(40);
	const BondageStats S = W.Stats(Region);
	VAELEN_LOG_INFO(LogBondage,
					"hardening: %u bonded, %u enslaved; entered by debt %u, birth %u; left by holder's death %u, death "
					"%u; %u caused",
					S.Bonded, S.Enslaved, S.Entered[1], S.Entered[2], S.Left[3], S.Left[4], S.Caused);
	VT_CHECK(S.Enslaved > S.Bonded);
	VT_CHECK(S.Entered[2] > 0);
	VT_CHECK(S.Left[3] > 0 && S.Left[4] > 0);
	VT_CHECK_EQ(S.Left[1] + S.Left[2], 0u);
	VT_CHECK(S.Caused >=
			 S.Entered[2] + S.Left[4]); // births and deaths carry their event (a holder gone abroad has none)
	VT_CHECK_EQ(S.HolderLost, 0u);
	// Every enslaved by birth has (or had) an enslaved mother; the bonded are younger than the hardening.
	uint32 Bad = 0;
	W.Instance.Components()
		.GetPool(W.Bondage.Bond)
		.ForEach(
			[&](EntityHandle H, const BondState& B)
			{
				const PersonInfo* P = W.Instance.Components().GetPool(W.Persons.Person).TryGet(H);
				VT_REQUIRE(P != nullptr);
				if (B.Entry == static_cast<uint8>(BondEntry::Birth))
				{
					Bad += B.Kind != static_cast<uint8>(BondKind::Enslaved) || P->Mother == 0 ? 1u : 0u;
					const BondState* M = BondOf(W.Instance, W.Persons, W.Bondage, P->Mother);
					const PersonInfo* Mother = FindPerson(W.Instance, W.Persons, P->Mother);
					// The mother is enslaved still, or dead (her bond left with her).
					Bad += (M == nullptr || M->Kind != static_cast<uint8>(BondKind::Enslaved)) &&
								   (Mother == nullptr || Mother->State == static_cast<uint8>(LifeState::Alive))
							   ? 1u
							   : 0u;
				}
				else if (B.Kind == static_cast<uint8>(BondKind::Bonded))
				{
					Bad += W.Instance.Now() > B.Since + uint64{Fast.HardenAfterYears + 1} * TicksPerYear ? 1u : 0u;
				}
			});
	VT_CHECK_EQ(Bad, 0u);
	// Every entry and exit is about a person of the region and says its kind and reason.
	uint32 BadEvents = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(BondEnteredEvent) || E.Is(BondLeftEvent))
		{
			const BondPayload P = E.Get<BondPayload>();
			BadEvents += P.Region != Region || P.Person == 0 || !E.Subject.IsValid() || P.Reason == 0 ? 1u : 0u;
		}
	}
	VT_CHECK_EQ(BadEvents, 0u);
	// Birth bondage refused by the culture: nobody is born enslaved.
	Run Y(AelvorSeed, Fast);
	VT_REQUIRE(Y.Ages.Generate(Run::Square(128), 300));
	Y.AllowEverywhere(static_cast<uint32>(Bondage::Debt));
	VT_CHECK(RequestDetail(Y.Instance, Y.Lod, Region));
	Y.Ages.Run(40);
	const BondageStats SY = Y.Stats(Region);
	VT_CHECK(SY.Enslaved > 0);
	VT_CHECK_EQ(SY.Entered[2], 0u);
}

VAELEN_TEST(Bondage, ManumissionFlightAndTheGrain)
{
	BondageRules Kind;
	Kind.ManumissionPerMille = 200;
	Kind.FlightPerMille = 100;
	Kind.DebtPerMille = 40;
	Run W(AelvorSeed, Kind);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(All);
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(20);
	BondageStats S = W.Stats(Region);
	VT_CHECK(S.Left[1] > 0 && S.Left[2] > 0);
	VT_CHECK(S.Left[1] > S.Left[2]);					// manumission twice as likely as flight
	VT_CHECK(S.Bonded + S.Enslaved < S.Entered[1] / 2); // most are freed within years
	const RegionStrata Before = *StrataOf(W.Instance, W.Ages.Types(), W.Bondage, Region);
	// Demoted: the bonds go with the persons, the strata stay as the last count.
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(5);
	uint32 Bonds = 0;
	W.Instance.Components().GetPool(W.Bondage.Bond).ForEach([&](EntityHandle, const BondState&) { ++Bonds; });
	VT_CHECK_EQ(Bonds, 0u);
	const RegionStrata* Coarse = StrataOf(W.Instance, W.Ages.Types(), W.Bondage, Region);
	VT_REQUIRE(Coarse != nullptr);
	VT_CHECK_EQ(Coarse->Bonded, Before.Bonded);
	VT_CHECK_EQ(Coarse->Enslaved, Before.Enslaved);
	VT_CHECK_EQ(Coarse->Free, Before.Free);
	// Promoted again: bound again from the new persons within a few years.
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(5);
	S = W.Stats(Region);
	VT_CHECK(S.Bonded > 0);
	VT_CHECK_EQ(S.Stale, 0u);
	// Without a detailed region the system does nothing.
	Run Q(11);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(64), 60));
	const Hash64 Digest = ComputeStateDigest(Q.Instance);
	RandomStream Stream(1);
	TickContext Tc;
	Tc.Tick = Q.Instance.Now();
	Tc.Entities = &Q.Instance.Entities();
	Tc.Components = &Q.Instance.Components();
	Tc.Random = &Stream;
	Tc.Events = &Q.Instance.Events();
	Q.Bonds->Tick(Tc);
	VT_CHECK_EQ(ComputeStateDigest(Q.Instance), Digest);
}

VAELEN_TEST(Bondage, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	A.AllowEverywhere(All);
	B.AllowEverywhere(All);
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(20);
	B.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);
	VT_CHECK(A.Stats().Bonded > 0);
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	A.Ages.Run(20);
	R.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	// Frozen: the busiest region of AELVOR 128 for 200 years, every institution allowed.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(All);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(200);
	const BondageStats S = W.Stats();
	const uint32 Entered = S.Entered[1] + S.Entered[2] + S.Entered[3];
	const uint32 Left = S.Left[1] + S.Left[2] + S.Left[3] + S.Left[4];
	VAELEN_LOG_INFO(LogBondage, "frozen: bondage128=%016llx entered=%u left=%u (%u bonded, %u enslaved now)",
					static_cast<unsigned long long>(S.Digest), Entered, Left, S.Bonded, S.Enslaved);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_BONDAGE_FROZEN_128});
	VT_CHECK_EQ(Entered, uint32{VAELEN_BONDAGE_ENTERED_128});
	VT_CHECK_EQ(Left, uint32{VAELEN_BONDAGE_LEFT_128});
	VT_CHECK_EQ(S.Stale, 0u);
	VT_CHECK_EQ(S.HolderLost, 0u);
}
