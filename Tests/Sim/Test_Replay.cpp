// VAELEN - VaelenSim tests
// Deterministic replay: seed + input stream -> identical event log and snapshot;
// checkpoint at tick k, restore, run to N == uninterrupted run; chained
// checkpoints; divergence on any change; frozen hashes that every compiler and
// platform in CI must reproduce (the Phase 01 determinism gate).
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <cstdio>
#include <memory>
#include <vector>

using namespace Vaelen;

// Frozen reference values (see Replay.FrozenHashesAreReproducedByEveryCompilerAndPlatform).
// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (01.07).
#define VAELEN_REPLAY_FROZEN_EVENTS 11229ull
#define VAELEN_REPLAY_FROZEN_ALIVE 199ull
#define VAELEN_REPLAY_FROZEN_STATE                                                                                     \
	0x2d6381bc5b561332ull /* format 3 (02.03); format 2 was 73ad25a03c340ff5, format 1 dbb98f0004e8cd91 */
#define VAELEN_REPLAY_FROZEN_LOG 0x2c1e775e47e45051ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogReplay);

	// ── Components (plain data, no padding) ────────────────────────────────
	struct Settlement
	{
		uint64 Population = 0;
		uint64 Coins = 0;
		uint64 Founded = 0;
	};
	struct Granary
	{
		uint64 Grain = 0;
	};

	// ── Events ─────────────────────────────────────────────────────────────
	struct Birth
	{
		uint64 NewPopulation = 0;
	};
	struct Death
	{
		uint64 NewPopulation = 0;
	};
	struct Famine
	{
		uint64 Lost = 0;
	};
	constexpr EventType<Birth> BirthEvent = MakeEventType<Birth>("Birth");
	constexpr EventType<Death> DeathEvent = MakeEventType<Death>("Death");
	constexpr EventType<Famine> FamineEvent = MakeEventType<Famine>("Famine");
	constexpr EventType<NoPayload> DecreeEvent = MakeEventType<NoPayload>("Decree");
	constexpr EventType<NoPayload> FoundedEvent = MakeEventType<NoPayload>("Founded");

	// ── Systems: no state of their own (ADR-0015 rule 6) ──────────────────
	struct Types
	{
		ComponentType<Settlement> SettlementType;
		ComponentType<Granary> GranaryType;
	};

	class PopulationSystem final : public ISystem
	{
	public:
		explicit PopulationSystem(Types InTypes) : T(InTypes) {}
		const char* GetName() const noexcept override { return "Population"; }
		void Tick(TickContext& C) override
		{
			ComponentPool<Settlement>& Pool = C.Components->GetPool(T.SettlementType);
			Pool.ForEach(
				[&](EntityHandle H, Settlement& S)
				{
					const PersistentId Subject = C.Entities->GetId(H);
					if (C.Random->Chance(0.05))
					{
						S.Population += 1 + C.Random->Below(3);
						C.Events->Publish(C.Tick, BirthEvent, Birth{S.Population}, Subject);
					}
					if (S.Population > 0 && C.Random->Chance(0.03))
					{
						S.Population -= 1;
						C.Events->Publish(C.Tick, DeathEvent, Death{S.Population}, Subject);
					}
				});
		}
		Types T;
	};

	/// Daily: grain grows with the population; empty granaries starve people.
	class HarvestSystem final : public ISystem
	{
	public:
		explicit HarvestSystem(Types InTypes) : T(InTypes) {}
		const char* GetName() const noexcept override { return "Harvest"; }
		SimLod GetLod() const noexcept override { return SimLod::Detailed; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& C) override
		{
			ComponentPool<Settlement>& Settlements = C.Components->GetPool(T.SettlementType);
			ComponentPool<Granary>& Granaries = C.Components->GetPool(T.GranaryType);
			Settlements.ForEach(
				[&](EntityHandle H, Settlement& S)
				{
					Granary* G = Granaries.TryGet(H);
					if (G == nullptr)
					{
						return;
					}
					G->Grain += S.Population * (1 + C.Random->Below(3));
					const uint64 Eaten = S.Population * 2;
					if (G->Grain >= Eaten)
					{
						G->Grain -= Eaten;
					}
					else
					{
						const uint64 Lost = (S.Population + 3) / 4;
						G->Grain = 0;
						S.Population -= Lost > S.Population ? S.Population : Lost;
						C.Events->Publish(C.Tick, FamineEvent, Famine{Lost}, C.Entities->GetId(H));
					}
					S.Coins += C.Random->Below(5);
				});
		}
		Types T;
	};

	/// Monthly: a crowded settlement founds a new one (entity creation mid-run);
	/// an empty settlement is abandoned (entity destruction mid-run).
	class MigrationSystem final : public ISystem
	{
	public:
		MigrationSystem(Types InTypes, IdAllocator* InIds) : T(InTypes), Ids(InIds) {}
		const char* GetName() const noexcept override { return "Migration"; }
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override { return {"Harvest"}; }
		void Tick(TickContext& C) override
		{
			ComponentPool<Settlement>& Settlements = C.Components->GetPool(T.SettlementType);
			std::vector<EntityHandle> Crowded;
			std::vector<EntityHandle> Empty;
			Settlements.ForEach(
				[&](EntityHandle H, Settlement& S)
				{
					if (S.Population >= 40)
					{
						Crowded.push_back(H);
					}
					else if (S.Population == 0 && C.Tick > S.Founded + 24)
					{
						Empty.push_back(H);
					}
				});
			for (EntityHandle H : Crowded)
			{
				Settlement& Parent = Settlements.Get(H);
				const uint64 Moving = Parent.Population / 2;
				Parent.Population -= Moving;
				const EntityHandle Child = C.Entities->Create(*Ids, IdKind::Entity);
				Settlements.Add(Child, Settlement{Moving, Parent.Coins / 4, C.Tick});
				if (C.Random->Chance(0.5))
				{
					C.Components->GetPool(T.GranaryType).Add(Child, Granary{10});
				}
				C.Events->Publish(C.Tick, FoundedEvent, C.Entities->GetId(Child));
			}
			for (EntityHandle H : Empty)
			{
				C.Components->RemoveAll(H);
				C.Entities->Destroy(H);
			}
		}
		Types T;
		IdAllocator* Ids;
	};

	/// Listener: every famine causes a decree next tick (causal chain through the bus).
	class Chronicle final : public IEventListener
	{
	public:
		const char* GetListenerName() const noexcept override { return "Chronicle"; }
		void OnEvent(const Event& E) override { Bus->Publish(E.Tick, DecreeEvent, E.Subject, E.Id); }
		EventBus* Bus = nullptr;
	};

	// ── Inputs: the external command stream (recorded, replayed) ───────────
	enum class InputKind : uint8
	{
		Found, ///< found a settlement with Value people
		Raid,  ///< the Value-th alive settlement (slot order) loses half its people
		Decree ///< publish a decree with no cause
	};
	struct Input
	{
		SimTick Tick = 0;
		InputKind Kind = InputKind::Found;
		uint64 Value = 0;
	};

	std::vector<Input> MakeInputs(uint64 Seed, SimTick Until)
	{
		RandomStream Script(Seed ^ 0x1e5u);
		std::vector<Input> Inputs;
		Inputs.push_back({0, InputKind::Found, 20});
		Inputs.push_back({0, InputKind::Found, 12});
		for (SimTick Tick = 1; Tick < Until; Tick += 1 + Script.Below(50))
		{
			const uint64 Roll = Script.Below(10);
			if (Roll < 3)
			{
				Inputs.push_back({Tick, InputKind::Found, 5 + Script.Below(30)});
			}
			else if (Roll < 7)
			{
				Inputs.push_back({Tick, InputKind::Raid, Script.Below(4)});
			}
			else
			{
				Inputs.push_back({Tick, InputKind::Decree, 0});
			}
		}
		return Inputs;
	}

	// ── The world under test: setup code identical on every side ───────────
	struct ReplayWorld
	{
		explicit ReplayWorld(uint64 Seed) : Instance(Config(Seed))
		{
			T.SettlementType = Instance.Types().Register<Settlement>("Settlement");
			T.GranaryType = Instance.Types().Register<Granary>("Granary");
			Instance.Components().CreatePool(T.SettlementType);
			Instance.Components().CreatePool(T.GranaryType);
			Population = std::make_unique<PopulationSystem>(T);
			Harvest = std::make_unique<HarvestSystem>(T);
			Migration = std::make_unique<MigrationSystem>(T, &Instance.Ids());
			Instance.Systems().Add(Population.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Migration.get());
			Listener.Bus = &Instance.Events();
			Instance.Events().Subscribe(FamineEvent.TypeHash, &Listener);
			Built = Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}

		void Apply(const Input& In)
		{
			ComponentPool<Settlement>& Settlements = Instance.Components().GetPool(T.SettlementType);
			switch (In.Kind)
			{
			case InputKind::Found:
			{
				const EntityHandle H = Instance.CreateEntity(IdKind::Entity);
				Settlements.Add(H, Settlement{In.Value, 0, Instance.Now()});
				Instance.Components().GetPool(T.GranaryType).Add(H, Granary{In.Value * 3});
				Instance.Events().Publish(Instance.Now(), FoundedEvent, Instance.Entities().GetId(H));
				break;
			}
			case InputKind::Raid:
			{
				uint64 Index = 0;
				Settlements.ForEach(
					[&](EntityHandle, Settlement& S)
					{
						if (Index++ == In.Value)
						{
							S.Population /= 2;
						}
					});
				break;
			}
			case InputKind::Decree:
				Instance.Events().Publish(Instance.Now(), DecreeEvent);
				break;
			}
		}

		/// Runs from the current tick to Until, applying the inputs due at each tick.
		void RunTo(SimTick Until, const std::vector<Input>& Inputs)
		{
			usize Next = 0;
			while (Next < Inputs.size() && Inputs[Next].Tick < Instance.Now())
			{
				++Next; // inputs before the restore point were applied by the saved run
			}
			while (Instance.Now() < Until)
			{
				while (Next < Inputs.size() && Inputs[Next].Tick == Instance.Now())
				{
					Apply(Inputs[Next++]);
				}
				Instance.Tick();
			}
		}

		std::vector<uint8> Save() const
		{
			std::vector<uint8> Bytes;
			SaveSnapshot(Instance, Bytes);
			return Bytes;
		}
		bool Load(const std::vector<uint8>& Bytes)
		{
			return LoadSnapshot(Instance, Bytes.data(), Bytes.size()) == SnapshotResult::Ok;
		}

		World Instance;
		Types T;
		std::unique_ptr<PopulationSystem> Population;
		std::unique_ptr<HarvestSystem> Harvest;
		std::unique_ptr<MigrationSystem> Migration;
		Chronicle Listener;
		bool Built = false;
	};

	struct Outcome
	{
		Hash64 State = 0;
		Hash64 Log = 0;
		uint64 Events = 0;
		uint64 Alive = 0;
		std::vector<uint8> Image;
		bool operator==(const Outcome&) const = default;
	};

	Outcome Measure(const ReplayWorld& W)
	{
		Outcome O;
		O.Image = W.Save();
		O.State = ComputeStateDigest(W.Instance);
		O.Log = W.Instance.Log().Digest();
		O.Events = W.Instance.Log().Count();
		O.Alive = W.Instance.Entities().GetAliveCount();
		return O;
	}

	Outcome Uninterrupted(uint64 Seed, const std::vector<Input>& Inputs, SimTick N)
	{
		ReplayWorld W(Seed);
		W.RunTo(N, Inputs);
		return Measure(W);
	}

	constexpr uint64 ReferenceSeed = 0x5641454c454eull; // "VAELEN"
	constexpr SimTick ReferenceTicks = 2000;			// 83 days: daily and monthly systems both fire
} // namespace

VAELEN_TEST(Replay, SeedAndInputsReplayToIdenticalLogAndSnapshot)
{
	const std::vector<Input> Inputs = MakeInputs(ReferenceSeed, ReferenceTicks);
	VT_REQUIRE(Inputs.size() > 20);
	const Outcome A = Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks);
	const Outcome B = Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks);
	VT_CHECK(A == B);
	VT_CHECK(A.Events > 200);
	VT_CHECK(A.Alive > 2);

	// Event by event, not only by digest.
	ReplayWorld X(ReferenceSeed);
	ReplayWorld Y(ReferenceSeed);
	X.RunTo(ReferenceTicks, Inputs);
	Y.RunTo(ReferenceTicks, Inputs);
	VT_REQUIRE_EQ(X.Instance.Log().Count(), Y.Instance.Log().Count());
	for (uint64 i = 0; i < X.Instance.Log().Count(); ++i)
	{
		if (!(X.Instance.Log().At(i) == Y.Instance.Log().At(i)))
		{
			VT_CHECK_MSG(false, "event %llu differs", static_cast<unsigned long long>(i));
			break;
		}
	}
	// The chain famine -> decree exists and is causal.
	uint64 Chained = 0;
	for (const Event& E : X.Instance.Log().All())
	{
		if (E.Is(DecreeEvent) && E.Cause.IsValid())
		{
			++Chained;
		}
	}
	VT_CHECK(Chained > 0);
}

VAELEN_TEST(Replay, CheckpointRestoreContinueEqualsUninterruptedRun)
{
	const std::vector<Input> Inputs = MakeInputs(ReferenceSeed, ReferenceTicks);
	const Outcome Reference = Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks);

	// Restore points: before anything, mid-day, on a day boundary, on the month
	// boundary, one tick before the end.
	for (SimTick K : {SimTick{0}, SimTick{1}, SimTick{37}, SimTick{720}, SimTick{721}, SimTick{1999}})
	{
		ReplayWorld Saver(ReferenceSeed);
		Saver.RunTo(K, Inputs);
		const std::vector<uint8> Checkpoint = Saver.Save();

		ReplayWorld Restored(ReferenceSeed);
		VT_REQUIRE(Restored.Load(Checkpoint));
		VT_CHECK_EQ(Restored.Instance.Now(), K);
		Restored.RunTo(ReferenceTicks, Inputs);
		const Outcome Continued = Measure(Restored);
		VT_CHECK_MSG(Continued.State == Reference.State, "state digest differs after restore at %llu",
					 static_cast<unsigned long long>(K));
		VT_CHECK_MSG(Continued.Log == Reference.Log, "log digest differs after restore at %llu",
					 static_cast<unsigned long long>(K));
		VT_CHECK(Continued == Reference);
	}
}

VAELEN_TEST(Replay, ChainedCheckpointsEqualUninterruptedRun)
{
	const std::vector<Input> Inputs = MakeInputs(ReferenceSeed, ReferenceTicks);
	const Outcome Reference = Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks);

	// Save every 250 ticks, always restoring from the previous checkpoint into a
	// fresh world: eight generations of restore.
	std::vector<uint8> Checkpoint;
	{
		ReplayWorld First(ReferenceSeed);
		Checkpoint = First.Save();
	}
	for (SimTick Until = 250; Until <= ReferenceTicks; Until += 250)
	{
		ReplayWorld Generation(ReferenceSeed);
		VT_REQUIRE(Generation.Load(Checkpoint));
		Generation.RunTo(Until, Inputs);
		Checkpoint = Generation.Save();
	}
	ReplayWorld Final(ReferenceSeed);
	VT_REQUIRE(Final.Load(Checkpoint));
	VT_CHECK(Measure(Final) == Reference);
}

VAELEN_TEST(Replay, AnyChangeOfSeedOrInputsDiverges)
{
	const std::vector<Input> Inputs = MakeInputs(ReferenceSeed, ReferenceTicks);
	const Outcome Reference = Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks);

	VT_CHECK(Uninterrupted(ReferenceSeed + 1, Inputs, ReferenceTicks).State != Reference.State);
	VT_CHECK(Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks - 1).State != Reference.State);

	std::vector<Input> Changed = Inputs;
	Changed[Changed.size() / 2].Value += 1;
	const Outcome Divergent = Uninterrupted(ReferenceSeed, Changed, ReferenceTicks);
	VT_CHECK(Divergent.State != Reference.State);
	VT_CHECK(Divergent.Log != Reference.Log);

	std::vector<Input> Shifted = Inputs;
	Shifted[Shifted.size() / 2].Tick += 1;
	VT_CHECK(Uninterrupted(ReferenceSeed, Shifted, ReferenceTicks).State != Reference.State);

	std::vector<Input> Dropped = Inputs;
	Dropped.pop_back();
	VT_CHECK(Uninterrupted(ReferenceSeed, Dropped, ReferenceTicks).State != Reference.State);
}

VAELEN_TEST(Replay, FrozenHashesAreReproducedByEveryCompilerAndPlatform)
{
	// Recorded from the clang 18 Linux build; gcc, MSVC and AppleClang must
	// reproduce them in CI. A change here is a change of the simulation's
	// observable behaviour or save format and must be deliberate.
	const std::vector<Input> Inputs = MakeInputs(ReferenceSeed, ReferenceTicks);
	const Outcome O = Uninterrupted(ReferenceSeed, Inputs, ReferenceTicks);
	VAELEN_LOG_INFO(LogReplay, "replay reference: state=%016llx log=%016llx events=%llu alive=%llu image=%llu bytes",
					static_cast<unsigned long long>(O.State), static_cast<unsigned long long>(O.Log),
					static_cast<unsigned long long>(O.Events), static_cast<unsigned long long>(O.Alive),
					static_cast<unsigned long long>(O.Image.size()));
	VT_CHECK_EQ(O.Events, uint64{VAELEN_REPLAY_FROZEN_EVENTS});
	VT_CHECK_EQ(O.Alive, uint64{VAELEN_REPLAY_FROZEN_ALIVE});
	VT_CHECK_EQ(O.State, Hash64{VAELEN_REPLAY_FROZEN_STATE});
	VT_CHECK_EQ(O.Log, Hash64{VAELEN_REPLAY_FROZEN_LOG});
}
