// VAELEN - Tests/Society
// Phase 05.07: society in history - records that matter, lines for every
// society event, the why of a decision.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Disasters.h"
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

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.07): the busiest
// region of AELVOR 128 detailed at year 300 and chronicled for 100 years with
// every Phase 04 and 05 system, every institution allowed.
#define VAELEN_SOCIETYHISTORY_RECORDS_128 622u
#define VAELEN_SOCIETYHISTORY_TEXT_128 0x2e503b54e8c9604dull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSocietyHistory);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint32 AllInstitutions = static_cast<uint32>(Bondage::Debt) | static_cast<uint32>(Bondage::Capture) |
									   static_cast<uint32>(Bondage::Birth);

	struct Run
	{
		explicit Run(uint64 Seed, PreHistoryRules Ages_ = PreHistoryRules{},
					 SocietyChronicleRules InRules = SocietyChronicleRules{})
			: Instance(Config(Seed)), Ages(Instance, Ages_)
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
			Society_ = std::make_unique<SocietyChronicle>(Instance, Ages.Types(), Context, State, InRules);
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
		std::string Chronicle(uint32 MaxLines = 0)
		{
			std::string Out;
			ExportChronicleWithSociety(Instance, Ages.Types(), Context, Out, MaxLines);
			return Out;
		}
		SocietyChronicleStats Stats() const { return CheckSocietyChronicle(Instance, Ages.Types(), Context, State); }
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

	bool IsSociety(const Event& E)
	{
		return E.Is(OrganizationFoundedEvent) || E.Is(OrganizationDisbandedEvent) || E.Is(MemberJoinedEvent) ||
			   E.Is(MemberLeftEvent) || E.Is(HeadSeatedEvent) || E.Is(DecisionMadeEvent) || E.Is(RaidPlannedEvent) ||
			   E.Is(NormChangedEvent) || E.Is(BondEnteredEvent) || E.Is(BondLeftEvent);
	}
} // namespace

VAELEN_TEST(SocietyHistory, EverySocietyEventHasItsOwnLine)
{
	Run W(AelvorSeed, Cursed());
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(AllInstitutions);
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	for (uint32 Year = 1; Year <= 30; ++Year)
	{
		if (Year % 6 == 1)
		{
			VT_REQUIRE(Curse(W, Region, DisasterKind::Drought));
		}
		W.Ages.Run(1);
	}
	W.Instance.Events().Publish(W.Instance.Now(), SchismEvent, ReligionPayload{1, Region, 1, 1});
	W.Ages.Run(2);
	std::map<Hash64, uint32> Seen;
	uint32 Generic = 0;
	uint32 BadPrefix = 0;
	uint32 Shown = 0;
	std::string Line;
	std::string Other;
	const PersonIndex Index = BuildPersonIndex(W.Instance, W.Persons);
	for (const Event& E : W.Instance.Log().All())
	{
		Line.clear();
		DescribeSocietyEvent(W.Instance, W.Ages.Types(), W.Context, E, Line, &Index);
		if (!IsSociety(E))
		{
			Other.clear();
			DescribePersonEvent(W.Instance, W.Ages.Types(), W.Persons, W.Families, E, Other, &Index);
			VT_CHECK(Line == Other);
			continue;
		}
		++Seen[E.TypeHash];
		Generic += Line.find("something happened") != std::string::npos ? 1u : 0u;
		BadPrefix += Line.rfind("Year ", 0) == 0 && Line.find(": ") != std::string::npos ? 0u : 1u;
		VT_CHECK(Line.find("person ") == std::string::npos && Line.find("organisation ") == std::string::npos &&
				 Line.find("culture ") == std::string::npos);
		if (Shown < 12 && (E.Is(OrganizationFoundedEvent) || E.Is(HeadSeatedEvent) || E.Is(RaidPlannedEvent) ||
						   E.Is(NormChangedEvent) || (E.Is(DecisionMadeEvent) && E.Get<DecisionPayload>().Kind != 2) ||
						   (E.Is(BondEnteredEvent) && E.Get<BondPayload>().Kind == 2) ||
						   (E.Is(BondLeftEvent) && E.Get<BondPayload>().Reason == 1)))
		{
			++Shown;
			VAELEN_LOG_INFO(LogSocietyHistory, "%s", Line.c_str());
		}
	}
	VT_CHECK_EQ(Generic, 0u);
	VT_CHECK_EQ(BadPrefix, 0u);
	VT_CHECK(Seen.size() >= 8); // foundings, joins, leaves, heads, decisions, raids, customs, bonds
	VT_CHECK(Seen[OrganizationFoundedEvent.TypeHash] >= 2);
	VT_CHECK(Seen[DecisionMadeEvent.TypeHash] > 10);
	VT_CHECK(Seen[BondEnteredEvent.TypeHash] > 0 && Seen[NormChangedEvent.TypeHash] > 0);
	// Names: an organisation, a culture, and the fallbacks.
	std::string Name;
	NameOrganization(W.Instance, W.Ages.Types(), W.Context, 1, Name);
	VT_CHECK(Name.rfind("the council of ", 0) == 0);
	Name.clear();
	NameOrganization(W.Instance, W.Ages.Types(), W.Context, 0xfffffff0u, Name);
	VT_CHECK(Name == "organisation 4294967280");
	Name.clear();
	NameCulture(W.Instance, W.Ages.Types(), 1, Name);
	VT_CHECK(Name.rfind("the ", 0) == 0 && Name.size() > 5);
	Name.clear();
	NameCulture(W.Instance, W.Ages.Types(), 0xfffffff0u, Name);
	VT_CHECK(Name == "culture 4294967280");
}

VAELEN_TEST(SocietyHistory, OnlyWhatMattersIsRecordedAndTheWhyReachesTheDrought)
{
	SocietyChronicleRules Rules;
	Rules.MaxRecordsPerYear = 4;
	Run W(AelvorSeed, Cursed(), Rules);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(AllInstitutions);
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	for (uint32 Year = 1; Year <= 12; ++Year)
	{
		VT_REQUIRE(Curse(W, Region, DisasterKind::Drought));
		W.Ages.Run(1);
	}
	W.Ages.Run(1); // the last year's events are dispatched
	uint32 Mattered = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		bool M = E.Is(OrganizationFoundedEvent) || E.Is(OrganizationDisbandedEvent) || E.Is(HeadSeatedEvent) ||
				 E.Is(RaidPlannedEvent) || E.Is(NormChangedEvent);
		M = M ||
			(E.Is(DecisionMadeEvent) && E.Get<DecisionPayload>().Kind == static_cast<uint32>(DecisionKind::StoreGrain));
		M = M || (E.Is(BondEnteredEvent) && E.Get<BondPayload>().Kind == static_cast<uint32>(BondKind::Enslaved) &&
				  E.Get<BondPayload>().Reason != static_cast<uint32>(BondEntry::Promotion));
		M = M || (E.Is(BondLeftEvent) && E.Get<BondPayload>().Reason == static_cast<uint32>(BondExit::Manumission));
		Mattered += M ? 1u : 0u;
	}
	const SocietyChronicleStats S = W.Stats();
	VAELEN_LOG_INFO(LogSocietyHistory,
					"%u society records (%u dropped) of %u that mattered: %u founded, %u disbanded, %u heads, %u "
					"grain, %u raids, %u customs, %u enslaved, %u freed",
					S.Records, S.Dropped, Mattered, S.ByType[0], S.ByType[1], S.ByType[2], S.ByType[3], S.ByType[4],
					S.ByType[5], S.ByType[6], S.ByType[7]);
	VT_CHECK(S.Records > 0);
	VT_CHECK_EQ(S.Records + S.Dropped, Mattered);
	VT_CHECK(S.ByType[3] > 0 && S.ByType[0] >= 2);
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Records);
	// The cap per year and region holds.
	std::map<uint64, uint32> PerYear;
	W.Instance.Components()
		.GetPool(W.Ages.Types().History.Record)
		.ForEach(
			[&](EntityHandle, const RecordInfo& R)
			{
				const bool Society = R.Type == OrganizationFoundedEvent.TypeHash ||
									 R.Type == HeadSeatedEvent.TypeHash || R.Type == DecisionMadeEvent.TypeHash ||
									 R.Type == RaidPlannedEvent.TypeHash || R.Type == BondEnteredEvent.TypeHash ||
									 R.Type == BondLeftEvent.TypeHash || R.Type == OrganizationDisbandedEvent.TypeHash;
				if (Society)
				{
					++PerYear[(R.Tick / TicksPerYear) * 100000u + R.Region];
				}
			});
	uint32 OverCap = 0;
	for (const auto& [Key, N] : PerYear)
	{
		OverCap += N > Rules.MaxRecordsPerYear ? 1u : 0u;
	}
	VT_CHECK_EQ(OverCap, 0u);
	// The why of a grain decision reaches the drought that struck.
	PersistentId Grain;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(DecisionMadeEvent) && E.Get<DecisionPayload>().Kind == static_cast<uint32>(DecisionKind::StoreGrain) &&
			!Grain.IsValid())
		{
			Grain = E.Id;
		}
	}
	VT_REQUIRE(Grain.IsValid());
	std::string Why;
	const uint32 Lines = ExportWhyWithSociety(W.Instance, W.Ages.Types(), W.Context, Grain, Why);
	VAELEN_LOG_INFO(LogSocietyHistory, "why:\n%s", Why.c_str());
	VT_CHECK(Lines >= 2);
	VT_CHECK(Why.find(" laid in grain against the next drought.") != std::string::npos);
	VT_CHECK(Why.find("  because a") != std::string::npos && Why.find("drought struck") != std::string::npos);
	// Silent rules record nothing; the person and Phase 03 chronicles are untouched.
	SocietyChronicleRules Silent;
	Silent.RecordFoundings = 0;
	Silent.RecordHeads = 0;
	Silent.RecordGrain = 0;
	Silent.RecordRaids = 0;
	Silent.RecordCustoms = 0;
	Silent.RecordEnslavements = 0;
	Silent.RecordManumissions = 0;
	Run X(AelvorSeed, PreHistoryRules{}, Silent);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, X.Busiest()));
	X.Ages.Run(10);
	VT_CHECK_EQ(X.Stats().Records, 0u);
	VT_CHECK_EQ(X.Stats().Dropped, 0u);
	const ChronicleStats C = CheckChronicle(W.Instance, W.Ages.Types());
	VT_CHECK_EQ(C.Resolved, C.Records);
	const PersonChronicleStats PC =
		CheckPersonChronicle(W.Instance, W.Ages.Types(), W.Persons, W.Families, W.PersonRecords);
	VT_CHECK_EQ(PC.Described, PC.Records);
}

VAELEN_TEST(SocietyHistory, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	A.AllowEverywhere(AllInstitutions);
	B.AllowEverywhere(AllInstitutions);
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(30);
	B.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	const std::string TextA = A.Chronicle();
	VT_CHECK(TextA == B.Chronicle());
	VT_CHECK(A.Stats().Records > 0);
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	const std::string TextMid = A.Chronicle();
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(R.Chronicle() == TextMid);
	A.Ages.Run(30);
	R.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK(R.Chronicle() == A.Chronicle());
	// Frozen: the busiest region of AELVOR 128 for 100 years.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.AllowEverywhere(AllInstitutions);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(100);
	const SocietyChronicleStats S = W.Stats();
	const std::string Text = W.Chronicle();
	const Hash64 Digest = HashString(Text);
	uint32 Lines = 0;
	for (const char C : Text)
	{
		Lines += C == '\n' ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogSocietyHistory, "frozen: societyhistory128 records=%u text=%016llx (%u lines, %u dropped)",
					S.Records, static_cast<unsigned long long>(Digest), Lines, S.Dropped);
	VT_CHECK_EQ(S.Records, uint32{VAELEN_SOCIETYHISTORY_RECORDS_128});
	VT_CHECK_EQ(Digest, Hash64{VAELEN_SOCIETYHISTORY_TEXT_128});
	VT_CHECK_EQ(S.Described, S.Records);
	usize From = Text.size();
	for (uint32 i = 0; i < 8 && From > 0; ++i)
	{
		From = Text.rfind('\n', From - 1);
		From = From == std::string::npos ? 0 : From;
	}
	VAELEN_LOG_INFO(LogSocietyHistory, "...\n%s", Text.c_str() + (From > 0 ? From + 1 : 0));
}
