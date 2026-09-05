// VAELEN - VaelenSim
// World assembly and ticking.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_World.cpp and Test_Snapshot.cpp
#include "Vaelen/Sim/World.h"
#include "Vaelen/Core/Assert.h"

namespace Vaelen
{
	World::World(const WorldConfig& InConfig)
		: Configuration(InConfig), Root(InConfig.Seed), SimulationClock(InConfig.StartTick, InConfig.Calendar),
		  Store(TypeRegistry), Bus(IdAlloc, History)
	{
		SystemScheduler.SetLodSchedule(InConfig.Lods);
	}

	bool World::Build()
	{
		return SystemScheduler.Build() == Scheduler::BuildResult::Ok;
	}

	uint32 World::Tick()
	{
		return SystemScheduler.RunTick(SimulationClock, Root, Registry, Store, &Bus);
	}

	uint64 World::TickMany(uint64 Count)
	{
		uint64 Ran = 0;
		for (uint64 i = 0; i < Count; ++i)
		{
			Ran += Tick();
		}
		return Ran;
	}

	EntityHandle World::CreateEntity(IdKind Kind)
	{
		return Registry.Create(IdAlloc, Kind);
	}

	bool World::DestroyEntity(EntityHandle Handle) noexcept
	{
		if (!Registry.IsAlive(Handle))
		{
			return false;
		}
		Store.RemoveAll(Handle);
		return Registry.Destroy(Handle);
	}
} // namespace Vaelen
