// VAELEN - Tests/Sim
// Phase 03.07: queryable history and the chronicle as text.
//
// STATUS: VALIDATED (Phase 03)

#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (03.07): FNV-1a of the
// whole chronicle text of AELVOR 128 after 300 years, and its line count.
#define VAELEN_CHRONICLE_TEXT_128 0xc0a39beace60c36bull
#define VAELEN_CHRONICLE_LINES_128 179u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogHistoryText);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{}) { Instance.Build(); }
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
		World Instance;
		PreHistory Ages;
	};

	void LogHead(const char* Title, const std::string& Text, usize MaxLines)
	{
		std::string Slice;
		usize Lines = 0;
		for (usize i = 0; i < Text.size() && Lines < MaxLines; ++i)
		{
			Slice += Text[i];
			Lines += Text[i] == '\n' ? 1u : 0u;
		}
		VAELEN_LOG_INFO(LogHistoryText, "%s:\n%s", Title, Slice.c_str());
	}
} // namespace

VAELEN_TEST(HistoryText, EveryRecordResolvesAndReadsAsALine)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const ChronicleStats S = CheckChronicle(W.Instance, W.Ages.Types());
	VAELEN_LOG_INFO(LogHistoryText,
					"chronicle: %u records, %u resolved, %u era-consistent, %u with a region, %u described", S.Records,
					S.Resolved, S.EraConsistent, S.WithRegion, S.Described);
	VT_CHECK(S.Records > 100);
	VT_CHECK_EQ(S.Resolved, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Records);
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK(S.WithRegion * 2 > S.Records); // most history happens somewhere
	// Every line starts with its year and ends with a full stop; no fallback names
	// for regions that are settled (they were named by 03.03).
	std::string Text;
	const uint32 Lines = ExportChronicle(W.Instance, W.Ages.Types(), Text);
	VT_CHECK_EQ(Lines, S.Records);
	LogHead("AELVOR 128, the first lines of the chronicle", Text, 24);
	usize Start = 0;
	uint32 Checked = 0;
	uint64 LastYear = 0;
	while (Start < Text.size())
	{
		const usize End = Text.find('\n', Start);
		const std::string Line = Text.substr(Start, End - Start);
		Start = End + 1;
		++Checked;
		VT_CHECK(Line.rfind("Year ", 0) == 0);
		VT_CHECK(!Line.empty() && Line.back() == '.');
		const uint64 Year = std::strtoull(Line.c_str() + 5, nullptr, 10);
		VT_CHECK(Year >= LastYear); // tick order
		LastYear = Year;
		VT_CHECK(Line.find("something happened") == std::string::npos);
	}
	VT_CHECK_EQ(Checked, Lines);
	// The head of the chronicle is capped when asked.
	std::string Head;
	VT_CHECK_EQ(ExportChronicle(W.Instance, W.Ages.Types(), Head, 5), 5u);
	VT_CHECK(Text.rfind(Head, 0) == 0);
}

VAELEN_TEST(HistoryText, WhyWalksCausesFromEventsAndEntities)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 500));
	const EventLog& Log = W.Instance.Log();
	// Every disaster explains itself by its omen; every schism by its split.
	uint32 Disasters = 0;
	uint32 Schisms = 0;
	for (const Event& E : Log.All())
	{
		if (E.Is(DisasterStruckEvent))
		{
			std::vector<WhyStep> Steps;
			Why(W.Instance, W.Ages.Types(), E.Id, Steps);
			VT_REQUIRE(Steps.size() >= 2);
			VT_CHECK(Steps[0].Cause == &E);
			VT_CHECK(Steps[1].Cause->Is(OmenEvent));
			VT_CHECK_EQ(Steps[0].Region, E.Get<DisasterPayload>().Region);
			VT_CHECK_EQ(Steps[0].Era, EraAt(W.Instance, W.Ages.Types().History, E.Tick));
			++Disasters;
		}
		if (E.Is(SchismEvent))
		{
			std::vector<WhyStep> Steps;
			Why(W.Instance, W.Ages.Types(), E.Id, Steps);
			VT_REQUIRE(Steps.size() >= 2);
			VT_CHECK(Steps[1].Cause->Is(CultureSplitEvent));
			++Schisms;
		}
	}
	VT_CHECK(Disasters > 10);
	VT_CHECK(Schisms >= 1);
	// From an entity: a religion's origin is its founding event; the chain reaches
	// a root without a cause.
	uint32 Religions = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().Religion.Religion)
		.ForEach(
			[&](EntityHandle H, const ReligionInfo& Rg)
			{
				const PersistentId Id = W.Instance.Entities().GetId(H);
				const Event* Origin = OriginOf(W.Instance, W.Ages.Types(), Id);
				VT_REQUIRE(Origin != nullptr);
				VT_CHECK(Origin->Is(ReligionFoundedEvent) || Origin->Is(SchismEvent));
				VT_CHECK_EQ(Origin->Cause.Value, Rg.FoundingEvent);
				std::vector<WhyStep> Steps;
				Why(W.Instance, W.Ages.Types(), Id, Steps);
				VT_REQUIRE(Steps.size() >= 2);
				VT_CHECK(Steps.front().Cause == Origin);
				VT_CHECK(!Steps.back().Cause->Cause.IsValid()); // the root has no cause
				++Religions;
			});
	VT_CHECK(Religions >= 1);
	// Text of an explanation: one line per step, "because" from the second on.
	uint32 Explained = 0;
	for (const Event& E : Log.All())
	{
		if (E.Is(DisasterStruckEvent) && Explained < 3)
		{
			std::string Text;
			const uint32 Steps = ExportWhy(W.Instance, W.Ages.Types(), E.Id, Text);
			VT_CHECK(Steps >= 2);
			VT_CHECK(Text.find("\nbecause Year") != std::string::npos);
			VAELEN_LOG_INFO(LogHistoryText, "why:\n%s", Text.c_str());
			++Explained;
		}
	}
	// Unknown ids explain nothing.
	std::vector<WhyStep> None;
	Why(W.Instance, W.Ages.Types(), PersistentId{}, None);
	VT_CHECK(None.empty());
	Why(W.Instance, W.Ages.Types(), PersistentId{0xffffffffffffull}, None);
	VT_CHECK(None.empty());
	VT_CHECK(OriginOf(W.Instance, W.Ages.Types(), PersistentId{}) == nullptr);
}

VAELEN_TEST(HistoryText, RegionTimelinesAreCompleteAndOrdered)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	uint32 Regions = 0;
	uint32 Total = 0;
	uint32 Longest = 0;
	uint32 LongestRegion = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle, const RegionInfo& R)
			{
				++Regions;
				std::vector<RecordInfo> Timeline;
				RegionTimeline(W.Instance, W.Ages.Types(), R.Index, Timeline);
				for (usize i = 0; i < Timeline.size(); ++i)
				{
					VT_CHECK_EQ(Timeline[i].Region, R.Index);
					if (i > 0)
					{
						VT_CHECK(
							Timeline[i - 1].Tick < Timeline[i].Tick ||
							(Timeline[i - 1].Tick == Timeline[i].Tick && Timeline[i - 1].Event < Timeline[i].Event));
					}
				}
				Total += static_cast<uint32>(Timeline.size());
				if (Timeline.size() > Longest)
				{
					Longest = static_cast<uint32>(Timeline.size());
					LongestRegion = R.Index;
				}
			});
	// The timelines partition the records that have a region.
	const ChronicleStats S = CheckChronicle(W.Instance, W.Ages.Types());
	VT_CHECK_EQ(Total, S.WithRegion);
	VT_CHECK(Longest >= 3);
	std::string Text;
	const uint32 Lines = ExportRegionChronicle(W.Instance, W.Ages.Types(), LongestRegion, Text);
	VT_CHECK_EQ(Lines, Longest);
	std::string Name;
	NameRegion(W.Instance, W.Ages.Types(), LongestRegion, Name);
	VAELEN_LOG_INFO(LogHistoryText, "the story of %s (region %u, %u records):\n%s", Name.c_str(), LongestRegion, Lines,
					Text.c_str());
	// Region 0 and unknown regions have no timeline.
	std::vector<RecordInfo> None;
	RegionTimeline(W.Instance, W.Ages.Types(), 0, None);
	VT_CHECK(None.empty());
	RegionTimeline(W.Instance, W.Ages.Types(), 9999, None);
	VT_CHECK(None.empty());
	VT_CHECK_EQ(ExportRegionChronicle(W.Instance, W.Ages.Types(), 9999, Text), 0u);
	VT_CHECK(Text.empty());
}

VAELEN_TEST(HistoryText, NamesFallBackDeterministically)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 30));
	std::string Out;
	NameRegion(W.Instance, W.Ages.Types(), 9999, Out);
	VT_CHECK_STREQ(Out.c_str(), "region 9999");
	NameEntity(W.Instance, W.Ages.Types(), PersistentId{}, Out);
	VT_CHECK_STREQ(Out.c_str(), "entity 0");
	// A river without a name (its source region is unsettled) falls back to its index.
	uint32 Fallbacks = 0;
	uint32 Named = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.Types.River)
		.ForEach(
			[&](EntityHandle H, const RiverInfo& R)
			{
				NameEntity(W.Instance, W.Ages.Types(), W.Instance.Entities().GetId(H), Out);
				if (Out.rfind("river ", 0) == 0)
				{
					VT_CHECK_EQ(Out, "river " + std::to_string(R.Index));
					++Fallbacks;
				}
				else
				{
					++Named;
				}
			});
	VT_CHECK(Fallbacks + Named > 0);
	// Cultures are named, and the name is the one on the entity.
	W.Instance.Components()
		.GetPool(W.Ages.Types().Population.Culture)
		.ForEach(
			[&](EntityHandle H, const CultureInfo&)
			{
				NameEntity(W.Instance, W.Ages.Types(), W.Instance.Entities().GetId(H), Out);
				const NameInfo* N = NameOf(W.Instance, W.Ages.Types().Languages, H);
				VT_REQUIRE(N != nullptr);
				VT_CHECK_STREQ(Out.c_str(), N->Text.Chars);
			});
	// An event of an unknown type gets the generic line.
	Event Odd;
	Odd.Tick = TicksPerYear * 12;
	Odd.TypeHash = HashString("Unknown");
	DescribeEvent(W.Instance, W.Ages.Types(), Odd, Out);
	VT_CHECK(Out.rfind("Year 12", 0) == 0);
	VT_CHECK(Out.find("something happened to entity 0") != std::string::npos);
}

VAELEN_TEST(HistoryText, TextIsDeterministicAcrossWorldsSnapshotsAndPlatforms)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	std::string TextA;
	std::string TextB;
	const uint32 Lines = ExportChronicle(A.Instance, A.Ages.Types(), TextA);
	ExportChronicle(B.Instance, B.Ages.Types(), TextB);
	VT_CHECK(TextA == TextB);
	// The same text from a restored snapshot.
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	std::string TextR;
	ExportChronicle(R.Instance, R.Ages.Types(), TextR);
	VT_CHECK(TextR == TextA);
	const Hash64 Digest = HashString(TextA);
	VAELEN_LOG_INFO(LogHistoryText, "frozen: chronicle128=%016llx lines=%u bytes=%zu",
					static_cast<unsigned long long>(Digest), Lines, TextA.size());
	VT_CHECK_EQ(Digest, Hash64{VAELEN_CHRONICLE_TEXT_128});
	VT_CHECK_EQ(Lines, uint32{VAELEN_CHRONICLE_LINES_128});
	// Another seed tells another story.
	Run C(11);
	VT_REQUIRE(C.Ages.Generate(Run::Square(128), 300));
	std::string TextC;
	ExportChronicle(C.Instance, C.Ages.Types(), TextC);
	VT_CHECK(TextC != TextA);
}
