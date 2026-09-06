// VAELEN - Tests/Population
// Phase 04.07: persons in history - records that matter, lines for every person
// event, a person's story and the why of a death.
//
// STATUS: VALIDATED (Phase 04)

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
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (04.07): the busiest
// region of AELVOR 128 detailed at year 300 and chronicled for 100 years with
// lives, families, needs, traits and the bridge.
#define VAELEN_PERSONHISTORY_RECORDS_128 1248u
#define VAELEN_PERSONHISTORY_TEXT_128 0xf097d2b9938ed0bcull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPersonHistory);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, PreHistoryRules Ages_ = PreHistoryRules{},
					 PersonChronicleRules InRules = PersonChronicleRules{})
			: Instance(Config(Seed)), Ages(Instance, Ages_)
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
			Chronicle_ = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families, State, InRules);
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
		std::string Chronicle(uint32 MaxLines = 0)
		{
			std::string Out;
			ExportChronicleWithPersons(Instance, Ages.Types(), Persons, Families, Out, MaxLines);
			return Out;
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

	bool IsPersonEvent(const Event& E)
	{
		return E.Is(PersonBornEvent) || E.Is(PersonDiedEvent) || E.Is(PersonMarriedEvent) || E.Is(FamilyFoundedEvent) ||
			   E.Is(FamilyExtinctEvent) || E.Is(PersonLeftEvent) || E.Is(PersonArrivedEvent) ||
			   E.Is(RegionPromotedEvent) || E.Is(RegionDemotedEvent);
	}
} // namespace

VAELEN_TEST(PersonHistory, EveryPersonEventHasItsOwnLine)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(40);
	uint32 PersonEvents = 0;
	uint32 Generic = 0;
	uint32 BadPrefix = 0;
	uint32 Shown = 0;
	std::string Line;
	std::string Other;
	for (const Event& E : W.Instance.Log().All())
	{
		Line.clear();
		DescribePersonEvent(W.Instance, W.Ages.Types(), W.Persons, W.Families, E, Line);
		if (!IsPersonEvent(E))
		{
			// Every other event reads exactly as Phase 03 writes it.
			Other.clear();
			DescribeEvent(W.Instance, W.Ages.Types(), E, Other);
			VT_CHECK(Line == Other);
			continue;
		}
		++PersonEvents;
		Generic += Line.find("something happened") != std::string::npos ? 1u : 0u;
		BadPrefix += Line.rfind("Year ", 0) == 0 && Line.find(": ") != std::string::npos ? 0u : 1u;
		const char* Expected = E.Is(PersonBornEvent)	   ? " was born"
							   : E.Is(PersonDiedEvent)	   ? " died"
							   : E.Is(PersonMarriedEvent)  ? " married "
							   : E.Is(FamilyFoundedEvent)  ? " founded a house in "
							   : E.Is(FamilyExtinctEvent)  ? " died out in "
							   : E.Is(PersonLeftEvent)	   ? " left "
							   : E.Is(PersonArrivedEvent)  ? " came to "
							   : E.Is(RegionPromotedEvent) ? "the chronicle turned to "
														   : "the chronicle left ";
		VT_CHECK_MSG(Line.find(Expected) != std::string::npos, "line lacks '%s': %s", Expected, Line.c_str());
		VT_CHECK(Line.find("person ") == std::string::npos); // everyone is named
		if (Shown < 8 && (E.Is(PersonMarriedEvent) || E.Is(FamilyFoundedEvent) || E.Is(RegionPromotedEvent) ||
						  (E.Is(PersonDiedEvent) && E.Cause.IsValid())))
		{
			++Shown;
			VAELEN_LOG_INFO(LogPersonHistory, "%s", Line.c_str());
		}
	}
	VT_CHECK(PersonEvents > 1000);
	VT_CHECK_EQ(Generic, 0u);
	VT_CHECK_EQ(BadPrefix, 0u);
	// Names: a known person, a house by its founder, fallbacks for the unknown.
	std::string Name;
	NamePerson(W.Instance, W.Ages.Types(), W.Persons, 0xfffffff0u, Name);
	VT_CHECK(Name == "person 4294967280");
	Name.clear();
	NameFamily(W.Instance, W.Ages.Types(), W.Persons, W.Families, 0xfffffff0u, Name);
	VT_CHECK(Name == "house 4294967280");
	Name.clear();
	NameFamily(W.Instance, W.Ages.Types(), W.Persons, W.Families, 1, Name);
	VT_CHECK(Name.rfind("the house of ", 0) == 0 && Name.size() > 14);
}

VAELEN_TEST(PersonHistory, OnlyWhatMattersIsRecordedAndTheYearlyCapHolds)
{
	PersonChronicleRules Rules;
	Rules.RecordHeadDeaths = 0;
	Rules.RecordHeadMarriages = 0;
	Rules.MaxRecordsPerYear = 3;
	Run W(AelvorSeed, Cursed(), Rules);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	for (uint32 Year = 1; Year <= 12; ++Year)
	{
		VT_REQUIRE(Curse(W, Region, Year % 2 == 1 ? DisasterKind::Drought : DisasterKind::Plague));
		W.Ages.Run(1);
	}
	W.Ages.Run(1); // the last year's events are dispatched
	// What mattered: foundings, extinctions, caused deaths, the focus.
	uint32 Mattered = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Mattered += E.Is(FamilyFoundedEvent) || E.Is(FamilyExtinctEvent) || E.Is(RegionPromotedEvent) ||
							E.Is(RegionDemotedEvent) || (E.Is(PersonDiedEvent) && E.Cause.IsValid())
						? 1u
						: 0u;
	}
	const PersonChronicleStats S = CheckPersonChronicle(W.Instance, W.Ages.Types(), W.Persons, W.Families, W.State);
	VAELEN_LOG_INFO(LogPersonHistory,
					"%u person records (%u dropped) of %u events that mattered: %u founded, %u extinct, %u died, %u "
					"married, %u left, %u arrived, %u turned to, %u left region",
					S.Records, S.Dropped, Mattered, S.ByType[0], S.ByType[1], S.ByType[2], S.ByType[3], S.ByType[4],
					S.ByType[5], S.ByType[6], S.ByType[7]);
	VT_CHECK(S.Records > 0 && S.Dropped > 0);
	VT_CHECK_EQ(S.Records + S.Dropped, Mattered);
	VT_CHECK_EQ(S.ByType[3] + S.ByType[4] + S.ByType[5], 0u); // no marriages, no crossings
	VT_CHECK_EQ(S.ByType[6], 1u);
	VT_CHECK(S.ByType[2] > 0 && S.ByType[0] > 0);
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK_EQ(S.WithRegion, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Records);
	// Every recorded death has a cause, and no year of the region holds more than the cap.
	std::map<uint64, uint32> PerYear;
	uint32 Uncaused = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().History.Record)
		.ForEach(
			[&](EntityHandle, const RecordInfo& R)
			{
				if (R.Type == PersonDiedEvent.TypeHash)
				{
					const Event* E = FindEvent(W.Instance.Log(), PersistentId{R.Event});
					Uncaused += E != nullptr && E->Cause.IsValid() ? 0u : 1u;
				}
				if (R.Type == PersonDiedEvent.TypeHash || R.Type == FamilyFoundedEvent.TypeHash ||
					R.Type == FamilyExtinctEvent.TypeHash)
				{
					++PerYear[(R.Tick / TicksPerYear) * 100000u + R.Region];
				}
			});
	VT_CHECK_EQ(Uncaused, 0u);
	uint32 OverCap = 0;
	for (const auto& [Key, N] : PerYear)
	{
		OverCap += N > Rules.MaxRecordsPerYear ? 1u : 0u;
	}
	VT_CHECK_EQ(OverCap, 0u);
	// Nothing recorded when nothing matters.
	PersonChronicleRules Silent;
	Silent.RecordFoundings = 0;
	Silent.RecordExtinctions = 0;
	Silent.RecordCausedDeaths = 0;
	Silent.RecordHeadDeaths = 0;
	Silent.RecordHeadMarriages = 0;
	Silent.RecordFocus = 0;
	Run X(AelvorSeed, PreHistoryRules{}, Silent);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, X.Busiest()));
	X.Ages.Run(10);
	const PersonChronicleStats SX = CheckPersonChronicle(X.Instance, X.Ages.Types(), X.Persons, X.Families, X.State);
	VT_CHECK_EQ(SX.Records, 0u);
	VT_CHECK_EQ(SX.Dropped, 0u);
	// The Phase 03 chronicle is untouched by the person records: its own check still holds.
	const ChronicleStats C = CheckChronicle(W.Instance, W.Ages.Types());
	VT_CHECK(C.Records >= S.Records);
	VT_CHECK_EQ(C.Resolved, C.Records);
}

VAELEN_TEST(PersonHistory, AStoryReachesTheDisasterThatKilled)
{
	Run W(AelvorSeed, Cursed());
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	for (uint32 Year = 1; Year <= 8; ++Year)
	{
		VT_REQUIRE(Curse(W, Region, Year % 2 == 1 ? DisasterKind::Drought : DisasterKind::Plague));
		W.Ages.Run(1);
	}
	uint32 Plague = 0;
	uint32 Famine = 0;
	uint32 Natural = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(PersonDiedEvent))
		{
			continue;
		}
		const PersonPayload P = E.Get<PersonPayload>();
		if (P.Other == static_cast<uint32>(DeathCause::Plague) && Plague == 0)
		{
			Plague = P.Person;
		}
		if (P.Other == static_cast<uint32>(DeathCause::Famine) && Famine == 0)
		{
			Famine = P.Person;
		}
		if (P.Other == static_cast<uint32>(DeathCause::Natural) && Natural == 0)
		{
			Natural = P.Person;
		}
	}
	VT_REQUIRE(Plague != 0 && Famine != 0 && Natural != 0);
	std::string Story;
	const uint32 Lines = ExportPersonStory(W.Instance, W.Ages.Types(), W.Persons, W.Families, Plague, Story);
	VAELEN_LOG_INFO(LogPersonHistory, "story of person %u:\n%s", Plague, Story.c_str());
	VT_CHECK(Lines >= 2);
	VT_CHECK(Story.find(" died of plague in ") != std::string::npos);
	VT_CHECK(Story.find("  because a") != std::string::npos && Story.find("plague struck") != std::string::npos);
	VT_CHECK(Story.find(" died") < Story.find("  because")); // the death first, then its why
	const Event* Death = DeathOf(W.Instance, W.Persons, Plague);
	VT_REQUIRE(Death != nullptr);
	VT_CHECK(Death->Cause.IsValid());
	std::vector<WhyStep> Steps;
	Why(W.Instance, W.Ages.Types(), Death->Cause, Steps);
	VT_CHECK(Steps.size() >= 1 && Steps.front().Cause != nullptr && Steps.front().Cause->Is(DisasterStruckEvent));
	Story.clear();
	ExportPersonStory(W.Instance, W.Ages.Types(), W.Persons, W.Families, Famine, Story);
	VT_CHECK(Story.find(" died of famine in ") != std::string::npos &&
			 Story.find("drought struck") != std::string::npos);
	// A natural death has no why; a person who never lived has no story.
	Story.clear();
	ExportPersonStory(W.Instance, W.Ages.Types(), W.Persons, W.Families, Natural, Story);
	VT_CHECK(Story.find(" died in ") != std::string::npos && Story.find("because") == std::string::npos);
	VT_CHECK(DeathOf(W.Instance, W.Persons, 0xfffffff0u) == nullptr);
	Story.clear();
	VT_CHECK_EQ(ExportPersonStory(W.Instance, W.Ages.Types(), W.Persons, W.Families, 0xfffffff0u, Story), 0u);
	VT_CHECK(Story.empty());
	// A timeline holds a person's marriage on both sides.
	uint32 Groom = 0;
	uint32 Bride = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(PersonMarriedEvent) && Groom == 0)
		{
			Groom = E.Get<MarriagePayload>().Person;
			Bride = E.Get<MarriagePayload>().Spouse;
		}
	}
	VT_REQUIRE(Groom != 0 && Bride != 0);
	std::vector<const Event*> Timeline;
	PersonTimeline(W.Instance, W.Persons, Bride, Timeline);
	bool Married = false;
	for (const Event* E : Timeline)
	{
		Married = Married || (E->Is(PersonMarriedEvent) && E->Get<MarriagePayload>().Person == Groom);
	}
	VT_CHECK(Married);
	PersonTimeline(W.Instance, W.Persons, 0, Timeline);
	VT_CHECK(Timeline.empty());
}

VAELEN_TEST(PersonHistory, DeterministicAndSnapshotSafe)
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
	const std::string TextA = A.Chronicle();
	VT_CHECK(TextA == B.Chronicle());
	VT_CHECK(CheckPersonChronicle(A.Instance, A.Ages.Types(), A.Persons, A.Families, A.State).Records > 0);
	// A snapshot between yearly ticks: the records, the state and the text continue identically.
	A.Instance.TickMany(100); // the last yearly tick's events are dispatched here
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	const std::string TextMid = A.Chronicle();
	VT_CHECK(TextMid.size() >= TextA.size());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(R.Chronicle() == TextMid);
	A.Ages.Run(30);
	R.Ages.Run(30);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK(R.Chronicle() == A.Chronicle());
	VT_CHECK(A.Chronicle().size() > TextA.size());
}

VAELEN_TEST(PersonHistory, FrozenChronicleIsReproducedByEveryCompilerAndPlatform)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(100);
	const PersonChronicleStats S = CheckPersonChronicle(W.Instance, W.Ages.Types(), W.Persons, W.Families, W.State);
	const std::string Text = W.Chronicle();
	const Hash64 Digest = HashString(Text);
	uint32 Lines = 0;
	for (const char C : Text)
	{
		Lines += C == '\n' ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogPersonHistory, "frozen: personhistory128 records=%u text=%016llx (%u lines, %u dropped)",
					S.Records, static_cast<unsigned long long>(Digest), Lines, S.Dropped);
	VT_CHECK_EQ(S.Records, uint32{VAELEN_PERSONHISTORY_RECORDS_128});
	VT_CHECK_EQ(Digest, Hash64{VAELEN_PERSONHISTORY_TEXT_128});
	VT_CHECK_EQ(S.Described, S.Records);
	// The last lines, for the record.
	usize From = Text.size();
	for (uint32 i = 0; i < 6 && From > 0; ++i)
	{
		From = Text.rfind('\n', From - 1);
		From = From == std::string::npos ? 0 : From;
	}
	VAELEN_LOG_INFO(LogPersonHistory, "...\n%s", Text.c_str() + (From > 0 ? From + 1 : 0));
}
