// VAELEN - VaelenSim tests
// Abstract mini-world, end to end over a long duration (the Phase 01
// long-duration gate): four systems at four LOD levels, a listener whose
// effects live in a component, 100 000 ticks (11.6 years) with invariants
// every 1 000 ticks, snapshot -> restore -> replay every 10 000 ticks, LOD
// firing counts, a frozen end-state, and a performance baseline that is
// logged, not asserted.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

using namespace Vaelen;

// Frozen end-state of the reference run (see MiniWorld.FrozenEndStateIsReproduced).
// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (01.08).
#define VAELEN_MINIWORLD_FROZEN_STATE                                                                                  \
	0xae5502465ee0a9acull /* format 3 (02.03); format 2 was 1d5174a4c8f9be17, format 1 0b6f6e9bd5887d35 */
#define VAELEN_MINIWORLD_FROZEN_LOG 0x60cd10a389895804ull
#define VAELEN_MINIWORLD_FROZEN_EVENTS 305027ull
#define VAELEN_MINIWORLD_FROZEN_ALIVE 41ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogMiniWorld);

	constexpr uint64 MiniSeed = 0x41454c564f52ull; // "AELVOR"
	constexpr SimTick LongRun = 100000;			   // 11 years, 208 days
	constexpr SimTick InvariantEvery = 1000;
	constexpr SimTick SnapshotEvery = 10000;
	constexpr SimTick ReplayLength = 700; // crosses a day and a month boundary
	constexpr uint64 Capacity = 60;		  // per settlement: deaths rise above it
	constexpr uint32 MaxVillages = 40;	  // the land: bounds the world's size over the long run

	// ── Components ─────────────────────────────────────────────────────────
	struct Village
	{
		uint64 People = 0;
		uint64 Founded = 0;
	};
	struct Stock
	{
		uint64 Grain = 0;
		uint64 Timber = 0;
	};
	/// Singleton kept by the Annals listener: its tallies are world state.
	struct Tally
	{
		uint64 Births = 0;
		uint64 Deaths = 0;
		uint64 Omens = 0;
		uint64 Years = 0;
		uint64 Founded = 0;
		uint64 Abandoned = 0;
	};

	// ── Events ─────────────────────────────────────────────────────────────
	struct Count
	{
		uint64 Value = 0;
	};
	constexpr EventType<Count> BirthsEvent = MakeEventType<Count>("Births");
	constexpr EventType<Count> DeathsEvent = MakeEventType<Count>("Deaths");
	constexpr EventType<Count> OmenEvent = MakeEventType<Count>("Omen");
	constexpr EventType<Count> NewYearEvent = MakeEventType<Count>("NewYear");
	constexpr EventType<NoPayload> FoundedEvent = MakeEventType<NoPayload>("Founded");
	constexpr EventType<NoPayload> AbandonedEvent = MakeEventType<NoPayload>("Abandoned");

	struct Types
	{
		ComponentType<Village> VillageType;
		ComponentType<Stock> StockType;
		ComponentType<Tally> TallyType;
	};

	// ── Systems (stateless: everything lives in components) ────────────────
	/// LOD 0, every tick: births and deaths, deaths rising above capacity.
	class Demography final : public ISystem
	{
	public:
		explicit Demography(Types InT) : T(InT) {}
		const char* GetName() const noexcept override { return "Demography"; }
		void Tick(TickContext& C) override
		{
			ComponentPool<Village>& Villages = C.Components->GetPool(T.VillageType);
			Villages.ForEach(
				[&](EntityHandle H, Village& V)
				{
					const PersistentId Subject = C.Entities->GetId(H);
					if (V.People > 0 && C.Random->Chance(0.04))
					{
						const uint64 Born = 1 + C.Random->Below(2);
						V.People += Born;
						C.Events->Publish(C.Tick, BirthsEvent, Count{Born}, Subject);
					}
					const double DeathChance = V.People > Capacity ? 0.12 : 0.03;
					if (V.People > 0 && C.Random->Chance(DeathChance))
					{
						const uint64 Dead = V.People > Capacity ? 1 + C.Random->Below(3) : 1;
						const uint64 Applied = Dead > V.People ? V.People : Dead;
						V.People -= Applied;
						C.Events->Publish(C.Tick, DeathsEvent, Count{Applied}, Subject);
					}
				});
		}
		Types T;
	};

	/// LOD 1, every 4 ticks: resources grow with people and are consumed.
	class Stockpile final : public ISystem
	{
	public:
		explicit Stockpile(Types InT) : T(InT) {}
		const char* GetName() const noexcept override { return "Stockpile"; }
		SimLod GetLod() const noexcept override { return SimLod::Detailed; }
		std::vector<std::string_view> GetDependencies() const override { return {"Demography"}; }
		void Tick(TickContext& C) override
		{
			ComponentPool<Village>& Villages = C.Components->GetPool(T.VillageType);
			ComponentPool<Stock>& Stocks = C.Components->GetPool(T.StockType);
			Villages.ForEach(
				[&](EntityHandle H, Village& V)
				{
					Stock& S = Stocks.Get(H); // every village has a stock (invariant)
					S.Grain += V.People * (1 + C.Random->Below(2));
					S.Timber += C.Random->Below(3);
					const uint64 Eaten = V.People;
					S.Grain = S.Grain > Eaten ? S.Grain - Eaten : 0;
					if (S.Grain == 0 && V.People > 0 && C.Random->Chance(0.5))
					{
						V.People -= 1;
						C.Events->Publish(C.Tick, DeathsEvent, Count{1}, C.Entities->GetId(H));
					}
				});
		}
		Types T;
	};

	/// LOD 3, every 720 ticks (monthly): random omens; a crowded village
	/// founds a new one, an empty one is abandoned.
	class Omens final : public ISystem
	{
	public:
		Omens(Types InT, IdAllocator* InIds) : T(InT), Ids(InIds) {}
		const char* GetName() const noexcept override { return "Omens"; }
		SimLod GetLod() const noexcept override { return SimLod::Statistic; }
		std::vector<std::string_view> GetDependencies() const override { return {"Stockpile"}; }
		void Tick(TickContext& C) override
		{
			ComponentPool<Village>& Villages = C.Components->GetPool(T.VillageType);
			const uint64 OmenCount = C.Random->Below(3);
			for (uint64 i = 0; i < OmenCount; ++i)
			{
				C.Events->Publish(C.Tick, OmenEvent, Count{C.Random->Below(1000)});
			}
			std::vector<EntityHandle> Crowded;
			std::vector<EntityHandle> Empty;
			Villages.ForEach(
				[&](EntityHandle H, Village& V)
				{
					if (V.People > Capacity)
					{
						Crowded.push_back(H);
					}
					else if (V.People == 0 && C.Tick > V.Founded + 720)
					{
						Empty.push_back(H);
					}
				});
			for (EntityHandle H : Crowded)
			{
				if (Villages.Size() >= MaxVillages)
				{
					break; // the land is full: crowded villages keep their high death rate
				}
				Village& Parent = Villages.Get(H);
				const uint64 Moving = Parent.People / 3;
				Parent.People -= Moving;
				const EntityHandle Child = C.Entities->Create(*Ids, IdKind::Entity);
				Villages.Add(Child, Village{Moving, C.Tick});
				C.Components->GetPool(T.StockType).Add(Child, Stock{Moving * 2, 0});
				C.Events->Publish(C.Tick, FoundedEvent, C.Entities->GetId(Child));
			}
			for (EntityHandle H : Empty)
			{
				const PersistentId Id = C.Entities->GetId(H);
				C.Components->RemoveAll(H);
				C.Entities->Destroy(H);
				C.Events->Publish(C.Tick, AbandonedEvent, Id);
			}
		}
		Types T;
		IdAllocator* Ids;
	};

	/// LOD 4, every 8640 ticks (yearly): a new year is announced with the
	/// population count; timber is spent on the year's works.
	class Years final : public ISystem
	{
	public:
		explicit Years(Types InT) : T(InT) {}
		const char* GetName() const noexcept override { return "Years"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Omens"}; }
		void Tick(TickContext& C) override
		{
			uint64 People = 0;
			C.Components->GetPool(T.VillageType).ForEach([&](EntityHandle, Village& V) { People += V.People; });
			C.Components->GetPool(T.StockType).ForEach([&](EntityHandle, Stock& S) { S.Timber /= 2; });
			C.Events->Publish(C.Tick, NewYearEvent, Count{People});
		}
		Types T;
	};

	/// Listener: tallies every event into the Tally component of the chronicle entity.
	class Annals final : public IEventListener
	{
	public:
		const char* GetListenerName() const noexcept override { return "Annals"; }
		void OnEvent(const Event& E) override
		{
			Tally& Ledger = Pool->Get(Chronicle);
			if (E.Is(BirthsEvent))
			{
				Ledger.Births += E.Get<Count>().Value;
			}
			else if (E.Is(DeathsEvent))
			{
				Ledger.Deaths += E.Get<Count>().Value;
			}
			else if (E.Is(OmenEvent))
			{
				++Ledger.Omens;
			}
			else if (E.Is(NewYearEvent))
			{
				++Ledger.Years;
			}
			else if (E.Is(FoundedEvent))
			{
				++Ledger.Founded;
			}
			else if (E.Is(AbandonedEvent))
			{
				++Ledger.Abandoned;
			}
		}
		ComponentPool<Tally>* Pool = nullptr;
		EntityHandle Chronicle;
	};

	// ── The world ──────────────────────────────────────────────────────────
	struct MiniWorld
	{
		explicit MiniWorld(uint64 Seed) : Instance(Config(Seed))
		{
			T.VillageType = Instance.Types().Register<Village>("Village");
			T.StockType = Instance.Types().Register<Stock>("Stock");
			T.TallyType = Instance.Types().Register<Tally>("Tally");
			Instance.Components().CreatePool(T.VillageType);
			Instance.Components().CreatePool(T.StockType);
			Instance.Components().CreatePool(T.TallyType);
			DemographySystem = std::make_unique<Demography>(T);
			StockpileSystem = std::make_unique<Stockpile>(T);
			OmensSystem = std::make_unique<Omens>(T, &Instance.Ids());
			YearsSystem = std::make_unique<Years>(T);
			Instance.Systems().Add(DemographySystem.get());
			Instance.Systems().Add(StockpileSystem.get());
			Instance.Systems().Add(OmensSystem.get());
			Instance.Systems().Add(YearsSystem.get());
			for (const Hash64 Type : {BirthsEvent.TypeHash, DeathsEvent.TypeHash, OmenEvent.TypeHash,
									  NewYearEvent.TypeHash, FoundedEvent.TypeHash, AbandonedEvent.TypeHash})
			{
				Instance.Events().Subscribe(Type, &Listener);
			}
			Listener.Pool = &Instance.Components().GetPool(T.TallyType);
			Built = Instance.Build();
			// The chronicle entity is created by the setup code on every side, so
			// it occupies the same slot in the original and in a restored world.
			Listener.Chronicle = EntityHandle::Make(0, 1);
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		/// Initial content: the chronicle, then Count villages.
		void Populate(uint64 Count)
		{
			const EntityHandle Chronicle = Instance.CreateEntity(IdKind::Entity);
			Instance.Components().GetPool(T.TallyType).Add(Chronicle, Tally{});
			for (uint64 i = 0; i < Count; ++i)
			{
				const EntityHandle H = Instance.CreateEntity(IdKind::Entity);
				Instance.Components().GetPool(T.VillageType).Add(H, Village{10 + i * 5, 0});
				Instance.Components().GetPool(T.StockType).Add(H, Stock{50, 5});
			}
		}
		std::vector<uint8> Save() const
		{
			std::vector<uint8> Bytes;
			SaveSnapshot(Instance, Bytes);
			return Bytes;
		}
		SnapshotResult Load(const std::vector<uint8>& Bytes)
		{
			return LoadSnapshot(Instance, Bytes.data(), Bytes.size());
		}
		const Tally& Ledger() const { return Instance.Components().GetPool(T.TallyType).Get(Listener.Chronicle); }
		uint64 People() const
		{
			uint64 Sum = 0;
			Instance.Components()
				.GetPool(T.VillageType)
				.ForEach([&](EntityHandle, const Village& V) { Sum += V.People; });
			return Sum;
		}

		World Instance;
		Types T;
		std::unique_ptr<Demography> DemographySystem;
		std::unique_ptr<Stockpile> StockpileSystem;
		std::unique_ptr<Omens> OmensSystem;
		std::unique_ptr<Years> YearsSystem;
		Annals Listener;
		bool Built = false;
	};

	/// Everything that must hold at any tick. Returns the number of failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, const MiniWorld& W, uint64 InitialPeople, uint64& LastEventCount,
						   Hash64& LastLogDigest)
	{
		uint32 Failures = 0;
		auto Fail = [&](const char* What)
		{
			++Failures;
			char Detail[64];
			std::snprintf(Detail, sizeof(Detail), "at tick %llu", static_cast<unsigned long long>(W.Instance.Now()));
			Ctx.ReportFailure(__FILE__, __LINE__, What, Detail);
		};
		const World& I = W.Instance;
		const uint32 Villages = I.Components().GetPool(W.T.VillageType).Size();
		const uint32 Stocks = I.Components().GetPool(W.T.StockType).Size();
		// Every village has a stock and vice versa; every entity is the chronicle or a village.
		if (Villages != Stocks)
		{
			Fail("village/stock pools out of step");
		}
		if (I.Entities().GetAliveCount() != Villages + 1u)
		{
			Fail("alive entities != villages + chronicle");
		}
		// Every village handle in the pool is alive and the pool agrees with the registry.
		I.Components()
			.GetPool(W.T.VillageType)
			.ForEach(
				[&](EntityHandle H, const Village&)
				{
					if (!I.Entities().IsAlive(H))
					{
						Fail("village on a dead entity");
					}
				});
		// Population book-keeping: initial + births - deaths == people alive
		// (the listener tallies every published event; events publish before
		// the tally is applied, so compare after the pending queue is empty).
		if (I.Events().PendingCount() == 0)
		{
			const Tally& L = W.Ledger();
			if (InitialPeople + L.Births != W.People() + L.Deaths)
			{
				Fail("population ledger does not balance");
			}
			// NewYear fires at ticks 0, 8640, ... and is tallied one tick later.
			const uint64 Expected = (I.Now() + 8640 - 1) / 8640;
			if (L.Years != Expected)
			{
				Fail("new-year count disagrees with the tick");
			}
		}
		// The log only grows and its digest moves whenever it grows.
		if (I.Log().Count() < LastEventCount)
		{
			Fail("event log shrank");
		}
		if (I.Log().Count() > LastEventCount && I.Log().Digest() == LastLogDigest)
		{
			Fail("log grew but digest did not change");
		}
		LastEventCount = I.Log().Count();
		LastLogDigest = I.Log().Digest();
		// Calendar consistency.
		const CalendarDate Date = I.Clock().Date();
		if (Date.Year != I.Now() / 8640 || Date.Month != (I.Now() % 8640) / 720 || Date.Hour != I.Now() % 24)
		{
			Fail("calendar date disagrees with the tick");
		}
		// Registry and pool states validate (the same checks a snapshot load runs).
		EntityRegistry Scratch;
		if (!Scratch.SetState(I.Entities().GetState()))
		{
			Fail("registry state fails validation");
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(MiniWorld, LongRunHoldsEveryInvariantAndLogsABaseline)
{
	MiniWorld W(MiniSeed);
	VT_REQUIRE(W.Built);
	W.Populate(6);
	const uint64 InitialPeople = W.People();
	uint64 LastEvents = 0;
	Hash64 LastDigest = 0;
	uint32 Failures = 0;
	uint64 PeakVillages = 0;

	const auto Start = std::chrono::steady_clock::now();
	while (W.Instance.Now() < LongRun)
	{
		W.Instance.TickMany(InvariantEvery);
		Failures += CheckInvariants(Ctx, W, InitialPeople, LastEvents, LastDigest);
		PeakVillages =
			PeakVillages > W.Instance.Entities().GetAliveCount() ? PeakVillages : W.Instance.Entities().GetAliveCount();
		if (Failures > 5)
		{
			break; // do not flood the report
		}
	}
	const auto End = std::chrono::steady_clock::now();
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK_EQ(W.Instance.Now(), LongRun);
	VT_CHECK(W.People() > 0);
	VT_CHECK(W.Ledger().Years >= 11);
	VT_CHECK(W.Ledger().Founded > 0);

	// Performance baseline: logged, not asserted (build type and machine dependent).
	const double Seconds = std::chrono::duration<double>(End - Start).count();
	const auto SnapStart = std::chrono::steady_clock::now();
	const std::vector<uint8> Image = W.Save();
	const double SnapSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - SnapStart).count();
	VAELEN_LOG_INFO(LogMiniWorld,
					"baseline: %llu ticks in %.3f s (%.0f ticks/s), peak %llu entities, %llu events, snapshot %llu "
					"bytes in %.4f s%s",
					static_cast<unsigned long long>(LongRun), Seconds, static_cast<double>(LongRun) / Seconds,
					static_cast<unsigned long long>(PeakVillages),
					static_cast<unsigned long long>(W.Instance.Log().Count()),
					static_cast<unsigned long long>(Image.size()), SnapSeconds,
					VAELEN_ASSERTS_ENABLED ? " [asserts on]" : " [asserts off]");
}

VAELEN_TEST(MiniWorld, PeriodicSnapshotsRestoreAndReplayIdentically)
{
	MiniWorld Reference(MiniSeed);
	Reference.Populate(6);
	uint32 Replays = 0;
	while (Reference.Instance.Now() < LongRun)
	{
		Reference.Instance.TickMany(SnapshotEvery);
		const std::vector<uint8> Checkpoint = Reference.Save();

		// A fresh world restored from the checkpoint must be byte-identical now...
		MiniWorld Restored(MiniSeed);
		VT_REQUIRE(Restored.Load(Checkpoint) == SnapshotResult::Ok);
		VT_CHECK(Restored.Save() == Checkpoint);

		// ...and after ReplayLength more ticks in both worlds. The reference
		// keeps running from the same state, so the two runs are compared at
		// Now + ReplayLength; the reference then continues from there.
		Reference.Instance.TickMany(ReplayLength);
		Restored.Instance.TickMany(ReplayLength);
		VT_CHECK_EQ(ComputeStateDigest(Restored.Instance), ComputeStateDigest(Reference.Instance));
		VT_CHECK_EQ(Restored.Instance.Log().Digest(), Reference.Instance.Log().Digest());
		VT_CHECK_EQ(Restored.Ledger().Births, Reference.Ledger().Births);
		++Replays;
	}
	VT_CHECK_EQ(Replays, 10u);
}

VAELEN_TEST(MiniWorld, LodSystemsFireExactlyOnSchedule)
{
	MiniWorld W(MiniSeed);
	W.Populate(6);
	W.Instance.TickMany(LongRun);
	const std::vector<uint64>& Counts = W.Instance.Systems().GetTickCounts();
	VT_REQUIRE_EQ(Counts.size(), 4u);
	// Order is Demography -> Stockpile -> Omens -> Years (dependency chain).
	VT_CHECK_EQ(Counts[0], LongRun);
	VT_CHECK_EQ(Counts[1], LongRun / 4);
	VT_CHECK_EQ(Counts[2], (LongRun + 719) / 720);
	VT_CHECK_EQ(Counts[3], (LongRun + 8639) / 8640);
	VT_CHECK_EQ(W.Ledger().Years, (LongRun + 8639) / 8640);
	// Every NewYear event sits on a year boundary.
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(NewYearEvent) && E.Tick % 8640 != 0)
		{
			VT_CHECK_MSG(false, "NewYear at tick %llu", static_cast<unsigned long long>(E.Tick));
			break;
		}
	}
}

VAELEN_TEST(MiniWorld, FrozenEndStateIsReproduced)
{
	// Recorded on clang 18 / Linux; gcc, MSVC and AppleClang must agree in CI.
	MiniWorld W(MiniSeed);
	W.Populate(6);
	W.Instance.TickMany(LongRun);
	const Hash64 State = ComputeStateDigest(W.Instance);
	VAELEN_LOG_INFO(LogMiniWorld, "end state: state=%016llx log=%016llx events=%llu alive=%llu",
					static_cast<unsigned long long>(State), static_cast<unsigned long long>(W.Instance.Log().Digest()),
					static_cast<unsigned long long>(W.Instance.Log().Count()),
					static_cast<unsigned long long>(W.Instance.Entities().GetAliveCount()));
	VT_CHECK_EQ(W.Instance.Log().Count(), uint64{VAELEN_MINIWORLD_FROZEN_EVENTS});
	VT_CHECK_EQ(W.Instance.Entities().GetAliveCount(), uint64{VAELEN_MINIWORLD_FROZEN_ALIVE});
	VT_CHECK_EQ(State, Hash64{VAELEN_MINIWORLD_FROZEN_STATE});
	VT_CHECK_EQ(W.Instance.Log().Digest(), Hash64{VAELEN_MINIWORLD_FROZEN_LOG});
}
