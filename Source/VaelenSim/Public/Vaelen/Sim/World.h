// VAELEN - VaelenSim
// The simulation world: every piece of Phase 01 state and the code that ticks it.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// A World owns the STATE (id allocator, root random stream, clock, entity
// registry, component pools, pending events, event log) and references the
// CODE that acts on it (component type registrations, systems, listeners).
// Snapshot.h captures and restores the state; the code is re-created by the
// same setup function on both sides, which is what makes a restored world
// continue exactly like the original (01.07).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/ComponentStore.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/EntityRegistry.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/SimClock.h"
#include "Vaelen/Sim/System.h"

namespace Vaelen
{
	struct WorldConfig
	{
		uint64 Seed = 0;
		CalendarRules Calendar{};
		LodSchedule Lods{};
		SimTick StartTick = 0;
	};

	class VAELEN_SIM_API World
	{
	public:
		explicit World(const WorldConfig& InConfig);
		World(const World&) = delete;
		World& operator=(const World&) = delete;

		// ── Setup (code, not state) ──────────────────────────────────────────
		ComponentTypeRegistry& Types() noexcept { return TypeRegistry; }
		const ComponentTypeRegistry& Types() const noexcept { return TypeRegistry; }
		Scheduler& Systems() noexcept { return SystemScheduler; }

		/// Builds the scheduler. Returns false when it reports an error.
		bool Build();

		// ── State ────────────────────────────────────────────────────────────
		const WorldConfig& Config() const noexcept { return Configuration; }
		IdAllocator& Ids() noexcept { return IdAlloc; }
		const IdAllocator& Ids() const noexcept { return IdAlloc; }
		RandomStream& RootStream() noexcept { return Root; }
		const RandomStream& RootStream() const noexcept { return Root; }
		SimClock& Clock() noexcept { return SimulationClock; }
		const SimClock& Clock() const noexcept { return SimulationClock; }
		EntityRegistry& Entities() noexcept { return Registry; }
		const EntityRegistry& Entities() const noexcept { return Registry; }
		ComponentStore& Components() noexcept { return Store; }
		const ComponentStore& Components() const noexcept { return Store; }
		EventBus& Events() noexcept { return Bus; }
		const EventBus& Events() const noexcept { return Bus; }
		EventLog& Log() noexcept { return History; }
		const EventLog& Log() const noexcept { return History; }

		// ── Simulation ───────────────────────────────────────────────────────
		/// One tick: pending events, due systems, clock + 1. Returns the number
		/// of systems that ran; 0 with a Check failure when not built.
		uint32 Tick();
		uint64 TickMany(uint64 Count);
		SimTick Now() const noexcept { return SimulationClock.Now(); }

		/// Creates an entity with a fresh persistent id of the given kind.
		EntityHandle CreateEntity(IdKind Kind);
		/// Removes every component, then destroys the entity.
		bool DestroyEntity(EntityHandle Handle) noexcept;

	private:
		WorldConfig Configuration;
		ComponentTypeRegistry TypeRegistry;
		IdAllocator IdAlloc;
		RandomStream Root;
		SimClock SimulationClock;
		EntityRegistry Registry;
		ComponentStore Store;
		EventLog History;
		EventBus Bus;
		Scheduler SystemScheduler;
	};
} // namespace Vaelen
