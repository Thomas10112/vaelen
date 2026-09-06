// VAELEN - Tests/Society
// Phase 05.05: organisations acting - grain against drought, preaching,
// training, raids planned.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Decisions.h"
#include "Vaelen/Society/Organizations.h"

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

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.05): the busiest
// region of AELVOR 128 detailed at year 300 and lived through 100 years with
// lives, families, needs, traits, the bridge, organisations and decisions.
#define VAELEN_DECISIONS_FROZEN_128 0xd99d3be56bcbfe4full
#define VAELEN_DECISIONS_MADE_128 155u
#define VAELEN_DECISIONS_ORGS_128 4u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogDecisions);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, PreHistoryRules Ages_ = PreHistoryRules{}, DecisionRules InRules = DecisionRules{},
					 bool Observe = true)
			: Instance(Config(Seed)), Ages(Instance, Ages_)
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Decisions = DecisionTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Acts = std::make_unique<DecisionSystem>(Instance, Ages.Types(), Persons, Traits, Organizations, Decisions,
													InRules);
			Houses->RunAfter("Needs");
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Orgs->RunAfter("Needs");
			if (Observe)
			{
				Body->ObserveStores(Decisions.Stores);
			}
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Acts.get());
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
		const OrganizationInfo* Of(uint32 Region, OrganizationKind Kind)
		{
			Cache.clear();
			OrganizationsOf(Instance, Organizations, Region, Cache);
			for (const OrganizationInfo& O : Cache)
			{
				if (O.Kind == static_cast<uint32>(Kind) && O.Disbanded == 0)
				{
					return &O;
				}
			}
			return nullptr;
		}
		DecisionStats Stats() const { return MeasureDecisions(Instance, Ages.Types(), Decisions); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		NeedTypes Needs;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		DecisionTypes Decisions;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<DecisionSystem> Acts;
		std::vector<OrganizationInfo> Cache;
	};

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

	uint32 Believers(const Run& W, uint32 Region, uint32 Religion)
	{
		uint32 N = 0;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P) {
					N += P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) && P.Religion == Religion
							 ? 1u
							 : 0u;
				});
		return N;
	}
} // namespace

VAELEN_TEST(Decisions, GuildsAndWarbandsAreFoundedFromTheSkilled)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(40); // skills grow for a generation
	const OrganizationInfo* Guild = W.Of(Region, OrganizationKind::Guild);
	const OrganizationInfo* Band = W.Of(Region, OrganizationKind::Warband);
	VT_REQUIRE(Guild != nullptr && Band != nullptr);
	VT_CHECK(Guild->Members > 0 && Band->Members > 0 && Guild->Head != 0 && Band->Head != 0);
	// Guild members are skilled in craft, its head the most; warband members skilled in fighting.
	std::vector<uint32> Members;
	MembersOf(W.Instance, W.Persons, W.Organizations, Guild->Index, Members);
	uint8 HeadCraft = 0;
	uint8 BestCraft = 0;
	for (const uint32 M : Members)
	{
		uint8 Craft = 0;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == M)
					{
						const PersonTraits* T = W.Instance.Components().GetPool(W.Traits.Traits).TryGet(H);
						Craft = T != nullptr ? T->Skills[static_cast<uint32>(Skill::Craft)] : 0u;
					}
				});
		VT_CHECK(Craft >= OrganizationRules{}.SkilledFrom);
		BestCraft = std::max(BestCraft, Craft);
		HeadCraft = M == Guild->Head ? Craft : HeadCraft;
	}
	VT_CHECK_EQ(HeadCraft, BestCraft);
	MembersOf(W.Instance, W.Persons, W.Organizations, Band->Index, Members);
	for (const uint32 M : Members)
	{
		uint8 Fighting = 0;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == M)
					{
						const PersonTraits* T = W.Instance.Components().GetPool(W.Traits.Traits).TryGet(H);
						Fighting = T != nullptr ? T->Skills[static_cast<uint32>(Skill::Fighting)] : 0u;
					}
				});
		VT_CHECK(Fighting >= OrganizationRules{}.SkilledFrom);
	}
	const DecisionStats S = W.Stats();
	VAELEN_LOG_INFO(LogDecisions,
					"region %u: guild of %u (head %u), warband of %u; %u decisions (%u grain, %u preached, %u trained, "
					"%u raids), %u converted",
					Region, Guild->Members, Guild->Head, Band->Members, S.Decisions, S.PerKind[0], S.PerKind[1],
					S.PerKind[2], S.PerKind[3], S.Converted);
	VT_CHECK(S.PerKind[2] > 0 && S.PerKind[3] > 0);
	VT_CHECK(S.Raids == S.PerKind[3]);
	VT_CHECK(DecisionKindName(DecisionKind::Raid)[0] == 'p' && DecisionKindName(DecisionKind::Count)[0] == '?');
}

VAELEN_TEST(Decisions, ACouncilLaysInGrainAfterADroughtAndTheFamineIsMilder)
{
	// Two cursed worlds: one whose need system observes the stores, one that does not.
	Run W(AelvorSeed, Cursed());
	Run X(AelvorSeed, Cursed(), DecisionRules{}, false);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	W.Ages.Run(1);
	X.Ages.Run(1);
	VT_REQUIRE(W.Of(Region, OrganizationKind::Council) != nullptr);
	VT_CHECK(StoresOf(W.Instance, W.Ages.Types(), W.Decisions, Region) == nullptr);
	for (uint32 Year = 1; Year <= 10; ++Year)
	{
		// Droughts in odd years: a drought every year empties a region (04.04), grain or not.
		if (Year % 2 == 1)
		{
			VT_REQUIRE(Curse(W, Region, DisasterKind::Drought));
			VT_REQUIRE(Curse(X, Region, DisasterKind::Drought));
		}
		W.Ages.Run(1);
		X.Ages.Run(1);
		const RegionStores* St = StoresOf(W.Instance, W.Ages.Types(), W.Decisions, Region);
		VT_REQUIRE(St != nullptr);
		VT_CHECK_EQ(St->GrainPerMille, DecisionRules{}.StorePerMille); // renewed while a drought is in memory
	}
	const NeedStats FedW = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	const NeedStats FedX = MeasureNeeds(X.Instance, X.Persons, X.Needs, Region);
	const DecisionStats S = W.Stats();
	VAELEN_LOG_INFO(
		LogDecisions,
		"ten years of drought: %u famine deaths with grain against %u without; %u grain decisions, %u caused",
		FedW.FamineDeaths, FedX.FamineDeaths, S.PerKind[0], S.Caused);
	VT_CHECK(FedW.FamineDeaths < FedX.FamineDeaths);
	VT_CHECK(FedW.FoodSum > FedX.FoodSum);
	VT_CHECK(S.PerKind[0] >= 10);		// one a year while the droughts are in memory
	VT_CHECK(S.Caused >= S.PerKind[0]); // every grain decision names the drought
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(DecisionMadeEvent) && E.Get<DecisionPayload>().Kind == static_cast<uint32>(DecisionKind::StoreGrain))
		{
			const Event* Cause = FindEvent(W.Instance.Log(), E.Cause);
			VT_CHECK(Cause != nullptr && Cause->Is(DisasterStruckEvent) &&
					 Cause->Get<DisasterPayload>().Kind == static_cast<uint32>(DisasterKind::Drought));
		}
	}
	// The stores are spent once the droughts are years away.
	W.Ages.Run(DecisionRules{}.StoreAfterDroughtYears + 1);
	const RegionStores* Spent = StoresOf(W.Instance, W.Ages.Types(), W.Decisions, Region);
	VT_REQUIRE(Spent != nullptr);
	VT_CHECK_EQ(Spent->GrainPerMille, 0u);
	VT_CHECK_EQ(W.Stats().RegionsWithGrain, 0u);
	// The world without the stores never kept any grain: its council decided the same
	// thing until the famine thinned the region and emptied the council's seats.
	VT_CHECK(StoresOf(X.Instance, X.Ages.Types(), X.Decisions, Region) != nullptr);
	VT_CHECK(X.Stats().PerKind[0] > 0 && X.Stats().PerKind[0] <= S.PerKind[0]);
}

VAELEN_TEST(Decisions, TemplesPreachAndGuildsTrain)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	const OrganizationInfo* Temple = W.Of(Region, OrganizationKind::Temple);
	VT_REQUIRE(Temple != nullptr && Temple->Religion != 0);
	const uint32 Faith = Temple->Religion;
	const uint32 Before = Believers(W, Region, Faith);
	const uint32 AliveBefore = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now()).Alive;
	const uint32 OthersBefore = AliveBefore - Before;
	W.Ages.Run(10);
	const uint32 After = Believers(W, Region, Faith);
	const uint32 AliveAfter = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now()).Alive;
	const DecisionStats S = W.Stats();
	VAELEN_LOG_INFO(LogDecisions, "temple of faith %u: %u -> %u believers of %u others (%u converted by %u sermons)",
					Faith, Before, After, OthersBefore, S.Converted, S.PerKind[1]);
	VT_CHECK(S.PerKind[1] >= 10);
	VT_CHECK(S.Converted > 0);
	// The faith's share of the living rose (the crowded region also loses people to
	// death and emigration, so the head count alone proves nothing).
	VT_CHECK(uint64{After} * AliveBefore > uint64{Before} * AliveAfter);
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region)); // the counts followed the persons
	// The believers of the temple's faith never exceed the living; the sermons stop at the share.
	VT_CHECK(S.Converted <= OthersBefore);
	// Guild training: the members' craft rises year by year until the guild exists.
	W.Ages.Run(30);
	const OrganizationInfo* Guild = W.Of(Region, OrganizationKind::Guild);
	VT_REQUIRE(Guild != nullptr);
	std::vector<uint32> Members;
	MembersOf(W.Instance, W.Persons, W.Organizations, Guild->Index, Members);
	VT_REQUIRE(!Members.empty());
	uint8 CraftBefore = 0;
	uint8 CraftAfter = 0;
	const uint32 Pupil = Members.front();
	auto CraftOf = [&](uint32 Person)
	{
		uint8 Out = 0;
		W.Instance.Components()
			.GetPool(W.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == Person)
					{
						const PersonTraits* T = W.Instance.Components().GetPool(W.Traits.Traits).TryGet(H);
						Out = T != nullptr ? T->Skills[static_cast<uint32>(Skill::Craft)] : 0u;
					}
				});
		return Out;
	};
	CraftBefore = CraftOf(Pupil);
	W.Ages.Run(1);
	CraftAfter = CraftOf(Pupil);
	const PersonInfo* P = FindPerson(W.Instance, W.Persons, Pupil);
	VT_REQUIRE(P != nullptr);
	if (P->State == static_cast<uint8>(LifeState::Alive) && CraftBefore < 250 && AgeYears(*P, W.Instance.Now()) < 60)
	{
		VT_CHECK(CraftAfter >= CraftBefore + DecisionRules{}.TrainGain); // training on top of the year's growth
	}
	VT_CHECK(W.Stats().PerKind[2] > 0);
	// Rules: nothing preached, trained or raided when the rules say zero.
	DecisionRules Idle;
	Idle.PreachPerMille = 0;
	Idle.TrainGain = 0;
	Idle.RaidEveryYears = 0;
	Idle.StoreAfterDroughtYears = 0;
	Run X(AelvorSeed, PreHistoryRules{}, Idle);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, X.Busiest()));
	X.Ages.Run(20);
	const DecisionStats SX = X.Stats();
	VT_CHECK_EQ(SX.Converted, 0u);
	VT_CHECK_EQ(SX.Raids, 0u);
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
	Q.Acts->Tick(Tc);
	VT_CHECK_EQ(ComputeStateDigest(Q.Instance), Digest);
}

VAELEN_TEST(Decisions, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(30);
	B.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Instance.Log().Digest(), B.Instance.Log().Digest());
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	A.Ages.Run(30);
	R.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	// Frozen: the busiest region of AELVOR 128 for 100 years.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(100);
	const DecisionStats S = W.Stats();
	const OrganizationStats O = MeasureOrganizations(W.Instance, W.Ages.Types(), W.Persons, W.Organizations);
	VAELEN_LOG_INFO(LogDecisions,
					"frozen: decisions128=%016llx made=%u organisations=%u (%u grain, %u preached, %u trained, %u "
					"raids; orgs %016llx)",
					static_cast<unsigned long long>(S.Digest), S.Decisions, O.Total, S.PerKind[0], S.PerKind[1],
					S.PerKind[2], S.PerKind[3], static_cast<unsigned long long>(O.Digest));
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_DECISIONS_FROZEN_128});
	VT_CHECK_EQ(S.Decisions, uint32{VAELEN_DECISIONS_MADE_128});
	VT_CHECK_EQ(O.Total, uint32{VAELEN_DECISIONS_ORGS_128});
	VT_CHECK_EQ(O.Astray, 0u);
	VT_CHECK_EQ(O.CountMismatch, 0u);
}
