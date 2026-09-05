// VAELEN - VaelenSim tests
// History 03.01: eras cover time without gaps, span and requested eras with
// causes, the chronicle records every chronicled event with era and region,
// cause chains resolve, queries, determinism and snapshot continuation.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

namespace
{
	struct Omen
	{
		uint32 Strength = 0;
	};
	constexpr EventType<Omen> OmenEvent = MakeEventType<Omen>("Omen");
	constexpr EventType<NoPayload> CollapseEvent = MakeEventType<NoPayload>("Collapse");

	/// Yearly: publishes an omen about a region; every fifth year a collapse
	/// caused by that omen, which the collapse listener turns into an era request.
	class OmenSystem final : public ISystem
	{
	public:
		OmenSystem(World& InWorld, RegionTypes InRegions) : Owner(&InWorld), Regions(InRegions) {}
		const char* GetName() const noexcept override { return "Omens"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Eras"}; }
		void Tick(TickContext& C) override
		{
			PersistentId Subject;
			uint32 Wanted = static_cast<uint32>(C.Random->Below(4)) + 1;
			Owner->Components()
				.GetPool(Regions.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Wanted)
						{
							Subject = Owner->Entities().GetId(H);
						}
					});
			const PersistentId OmenId =
				C.Events->Publish(C.Tick, OmenEvent, Omen{static_cast<uint32>(C.Tick / 8640)}, Subject);
			if ((C.Tick / 8640) % 5 == 4)
			{
				C.Events->Publish(C.Tick, CollapseEvent, Subject, OmenId);
			}
		}
		World* Owner;
		RegionTypes Regions;
	};

	class CollapseListener final : public IEventListener
	{
	public:
		explicit CollapseListener(EraSystem* InEras) : Eras(InEras) {}
		const char* GetListenerName() const noexcept override { return "CollapseListener"; }
		void OnEvent(const Event& E) override { Eras->RequestEra(E.Id); }
		EraSystem* Eras;
	};

	struct HistWorld
	{
		explicit HistWorld(uint64 Seed, uint64 SpanYears = 100) : Instance(Config(Seed))
		{
			Setup = WorldSetup::Declare(Instance);
			Types = HistoryTypes::Declare(Instance);
			EraRules Rules;
			Rules.SpanTicks = 8640ull * SpanYears;
			Eras = std::make_unique<EraSystem>(Instance, Types, Rules);
			Omens = std::make_unique<OmenSystem>(Instance, Setup.RegionTypes_);
			Log = std::make_unique<Chronicle>(Instance, Types, Setup.RegionTypes_);
			Collapse = std::make_unique<CollapseListener>(Eras.get());
			Instance.Systems().Add(Eras.get());
			Instance.Systems().Add(Omens.get());
			Log->Chronicle_(EraOpenedEvent.TypeHash);
			Log->Chronicle_(CollapseEvent.TypeHash);
			Instance.Events().Subscribe(CollapseEvent.TypeHash, Collapse.get());
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool Start(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			if (!GenerateWorld(Instance, Setup, Gen))
			{
				return false;
			}
			InitializeHistory(Instance, Types);
			return true;
		}
		World Instance;
		WorldSetup Setup;
		HistoryTypes Types;
		std::unique_ptr<EraSystem> Eras;
		std::unique_ptr<OmenSystem> Omens;
		std::unique_ptr<Chronicle> Log;
		std::unique_ptr<CollapseListener> Collapse;
	};

	constexpr uint64 Year = 8640;
} // namespace

VAELEN_TEST(History, ErasCoverTimeAndOpenOnSpanOrRequest)
{
	HistWorld W(1, 100);
	VT_REQUIRE(W.Start(32));
	W.Instance.TickMany(Year * 260 + 1);
	// Eras: founding at 0; collapses every 5th year request eras; spans of 100 never reached.
	std::vector<EraInfo> Eras;
	W.Instance.Components()
		.GetPool(W.Types.Era)
		.ForEach(
			[&](EntityHandle H, const EraInfo& E)
			{
				Eras.push_back(E);
				VT_CHECK(W.Instance.Entities().GetId(H).IsKind(IdKind::Era));
			});
	VT_REQUIRE(Eras.size() >= 50);
	uint64 Expected = 0;
	uint32 Open = 0;
	uint32 Requested = 0;
	for (usize i = 0; i < Eras.size(); ++i)
	{
		VT_CHECK_EQ(Eras[i].Index, static_cast<uint32>(i + 1));
		VT_CHECK_EQ(Eras[i].Start, Expected); // no gap, no overlap
		if (Eras[i].End == 0)
		{
			++Open;
		}
		else
		{
			VT_CHECK(Eras[i].End > Eras[i].Start);
			Expected = Eras[i].End;
		}
		if (Eras[i].Trigger == static_cast<uint32>(EraTrigger::Requested))
		{
			++Requested;
			VT_CHECK(Eras[i].Cause != 0);
			// The cause is a collapse whose own cause is an omen: the chain resolves.
			std::vector<const Event*> Chain;
			CauseChain(W.Instance.Log(), PersistentId(Eras[i].Cause), Chain);
			VT_REQUIRE_EQ(Chain.size(), 2u);
			VT_CHECK(Chain[0]->Is(CollapseEvent));
			VT_CHECK(Chain[1]->Is(OmenEvent));
		}
	}
	VT_CHECK_EQ(Open, 1u);
	VT_CHECK_EQ(Eras[0].Trigger, static_cast<uint32>(EraTrigger::Founding));
	VT_CHECK(Requested >= 50);
	VT_CHECK_EQ(EraAt(W.Instance, W.Types, 0), 1u);
	VT_CHECK_EQ(EraAt(W.Instance, W.Types, W.Instance.Now()), Eras.back().Index);
	// The requested era opens at the yearly tick after the request (next-tick delivery + yearly LOD).
	VT_CHECK_EQ(Eras[1].Start, Year * 5);

	// Span-triggered eras in a world without collapses: 30 years -> eras at 0, 30, 60...
	HistWorld S(2, 30);
	VT_REQUIRE(S.Start(16));
	S.Instance.Events().Unsubscribe(CollapseEvent.TypeHash, S.Collapse.get());
	S.Instance.TickMany(Year * 100 + 1);
	uint32 Spans = 0;
	S.Instance.Components()
		.GetPool(S.Types.Era)
		.ForEach(
			[&](EntityHandle, const EraInfo& E)
			{
				if (E.Trigger == static_cast<uint32>(EraTrigger::Span))
				{
					++Spans;
					VT_CHECK_EQ(E.Start % (Year * 30), 0u);
					VT_CHECK_EQ(E.Cause, 0u);
				}
			});
	VT_CHECK_EQ(Spans, 3u);
}

VAELEN_TEST(History, ChronicleRecordsEveryChronicledEventWithEraAndRegion)
{
	HistWorld W(3, 100);
	VT_REQUIRE(W.Start(32));
	W.Instance.TickMany(Year * 40 + 2);
	uint32 Records = 0;
	uint32 WithRegion = 0;
	W.Instance.Components()
		.GetPool(W.Types.Record)
		.ForEach(
			[&](EntityHandle H, const RecordInfo& R)
			{
				++Records;
				VT_CHECK(W.Instance.Entities().GetId(H).IsKind(IdKind::Document));
				const Event* E = FindEvent(W.Instance.Log(), PersistentId(R.Event));
				VT_REQUIRE(E != nullptr);
				VT_CHECK_EQ(E->Tick, R.Tick);
				VT_CHECK_EQ(E->TypeHash, R.Type);
				VT_CHECK_EQ(E->Subject.Value, R.Subject);
				VT_CHECK_EQ(R.Era, EraAt(W.Instance, W.Types, R.Tick));
				VT_CHECK(R.Type == EraOpenedEvent.TypeHash || R.Type == CollapseEvent.TypeHash);
				if (R.Region != 0)
				{
					++WithRegion;
					VT_CHECK(E->Is(CollapseEvent)); // eras are not regions
				}
			});
	// One record per chronicled event in the log, no more, no less.
	uint32 Chronicled = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Chronicled += (E.Is(EraOpenedEvent) || E.Is(CollapseEvent)) ? 1u : 0u;
	}
	VT_CHECK_EQ(Records, Chronicled);
	VT_CHECK(WithRegion >= 8); // 40 years: 8 collapses, each about a region
	uint32 StateRecords = 0;
	W.Instance.Components()
		.GetPool(W.Types.State)
		.ForEach([&](EntityHandle, const HistoryState& S) { StateRecords = S.RecordCount; });
	VT_CHECK_EQ(StateRecords, Records);
	// Queries.
	std::vector<const Event*> InEra;
	EventsInEra(W.Instance, W.Types, 1, InEra);
	VT_CHECK(!InEra.empty());
	for (const Event* E : InEra)
	{
		VT_CHECK(E->Tick < Year * 5);
	}
	EventsInEra(W.Instance, W.Types, 999, InEra);
	VT_CHECK(InEra.empty());
	std::vector<const Event*> About;
	PersistentId FirstEra;
	W.Instance.Components()
		.GetPool(W.Types.Era)
		.ForEach(
			[&](EntityHandle H, const EraInfo& E)
			{
				if (E.Index == 1)
				{
					FirstEra = W.Instance.Entities().GetId(H);
				}
			});
	EventsAbout(W.Instance.Log(), FirstEra, About);
	VT_REQUIRE_EQ(About.size(), 2u); // opened, closed
	VT_CHECK(About[0]->Is(EraOpenedEvent) && About[1]->Is(EraClosedEvent));
	VT_CHECK(FindEvent(W.Instance.Log(), PersistentId(0xffffffffull)) == nullptr);
	std::vector<const Event*> Chain;
	CauseChain(W.Instance.Log(), PersistentId(0xffffffffull), Chain);
	VT_CHECK(Chain.empty());
}

VAELEN_TEST(History, DeterministicAndSnapshotSafe)
{
	HistWorld A(7);
	HistWorld B(7);
	VT_REQUIRE(A.Start(32) && B.Start(32));
	A.Instance.TickMany(Year * 12);
	B.Instance.TickMany(Year * 12);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	// A pending request survives the snapshot: request just before a yearly tick.
	A.Instance.TickMany(Year - 3);
	B.Instance.TickMany(Year - 3);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	HistWorld R(7);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	A.Instance.TickMany(Year * 20);
	R.Instance.TickMany(Year * 20);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(EraAt(R.Instance, R.Types, R.Instance.Now()), EraAt(A.Instance, A.Types, A.Instance.Now()));
	// Misuse: a second initialisation is refused; ticking without history reports and does nothing.
	VaelenTest::ScopedAssertCapture Capture;
	VT_CHECK(InitializeHistory(A.Instance, A.Types).IsNull());
	HistWorld Empty(9);
	WorldGenConfig Gen;
	Gen.Width = 8;
	Gen.Height = 8;
	VT_REQUIRE(GenerateWorld(Empty.Instance, Empty.Setup, Gen));
	Empty.Instance.TickMany(2);
	VT_CHECK_EQ(Empty.Instance.Components().GetPool(Empty.Types.Era).Size(), 0u);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK(Capture.CheckCount >= 2);
#endif
}
