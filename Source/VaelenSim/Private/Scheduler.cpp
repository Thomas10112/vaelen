// VAELEN - VaelenSim
// Deterministic system ordering (Kahn's algorithm, name-hash tie-break) and
// tick execution with per-system derived random streams.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_Scheduler.cpp
#include "Vaelen/Sim/System.h"
#include "Vaelen/Core/Assert.h"

#include <algorithm>

namespace Vaelen
{
	namespace
	{
		bool NameEquals(const ISystem& System, std::string_view Name) noexcept
		{
			return std::string_view(System.GetName()) == Name;
		}
	} // namespace

	bool Scheduler::Add(ISystem* System)
	{
		VAELEN_CHECKF(System != nullptr, "Scheduler::Add requires a system");
		if (System == nullptr)
		{
			return false;
		}
		const std::string_view Name(System->GetName());
		VAELEN_CHECKF(!Name.empty(), "systems must have a name");
		if (Name.empty())
		{
			return false;
		}
		VAELEN_CHECKF(!Contains(Name), "system '%s' is already scheduled", System->GetName());
		if (Contains(Name))
		{
			return false;
		}
		Entry E;
		E.System = System;
		E.NameHash = HashString(Name);
		Entries.push_back(E);
		Built = false;
		return true;
	}

	bool Scheduler::Remove(std::string_view Name)
	{
		for (usize i = 0; i < Entries.size(); ++i)
		{
			if (NameEquals(*Entries[i].System, Name))
			{
				Entries.erase(Entries.begin() + static_cast<std::ptrdiff_t>(i));
				Built = false;
				return true;
			}
		}
		return false;
	}

	bool Scheduler::Contains(std::string_view Name) const noexcept
	{
		for (const Entry& E : Entries)
		{
			if (NameEquals(*E.System, Name))
			{
				return true;
			}
		}
		return false;
	}

	void Scheduler::SetLodSchedule(const LodSchedule& Schedule) noexcept
	{
		Lods = Schedule;
		Built = false;
	}

	Scheduler::BuildResult Scheduler::Build()
	{
		Built = false;
		Order.clear();
		TickCounts.clear();
		BuildError = {};
		if (!Lods.IsValid())
		{
			return BuildResult::InvalidLodSchedule;
		}

		const usize N = Entries.size();
		// Sort candidates by name hash (then name, for the astronomically
		// unlikely hash collision) so ties are broken identically everywhere.
		std::vector<uint32> ByHash(N);
		for (uint32 i = 0; i < N; ++i)
		{
			ByHash[i] = i;
		}
		std::stable_sort(ByHash.begin(), ByHash.end(),
						 [this](uint32 A, uint32 B)
						 {
							 if (Entries[A].NameHash != Entries[B].NameHash)
							 {
								 return Entries[A].NameHash < Entries[B].NameHash;
							 }
							 return std::string_view(Entries[A].System->GetName()) <
									std::string_view(Entries[B].System->GetName());
						 });

		// Resolve dependencies to indices.
		std::vector<std::vector<uint32>> Dependents(N); // Dependents[i]: systems that run after i
		std::vector<uint32> InDegree(N, 0);
		for (uint32 i = 0; i < N; ++i)
		{
			for (const std::string_view Dep : Entries[i].System->GetDependencies())
			{
				uint32 Found = 0xFFFFFFFFu;
				for (uint32 j = 0; j < N; ++j)
				{
					if (NameEquals(*Entries[j].System, Dep))
					{
						Found = j;
						break;
					}
				}
				if (Found == 0xFFFFFFFFu)
				{
					BuildError = std::string_view(Entries[i].System->GetName());
					return BuildResult::UnknownDependency;
				}
				if (Found == i)
				{
					BuildError = std::string_view(Entries[i].System->GetName());
					return BuildResult::Cycle;
				}
				Dependents[Found].push_back(i);
				++InDegree[i];
			}
		}

		// Kahn's algorithm: always pick the ready system with the smallest hash.
		std::vector<bool> Done(N, false);
		Order.reserve(N);
		while (Order.size() < N)
		{
			uint32 Pick = 0xFFFFFFFFu;
			for (const uint32 Candidate : ByHash)
			{
				if (!Done[Candidate] && InDegree[Candidate] == 0)
				{
					Pick = Candidate;
					break;
				}
			}
			if (Pick == 0xFFFFFFFFu)
			{
				// Every remaining system waits on another remaining system.
				for (const uint32 Candidate : ByHash)
				{
					if (!Done[Candidate])
					{
						BuildError = std::string_view(Entries[Candidate].System->GetName());
						break;
					}
				}
				Order.clear();
				return BuildResult::Cycle;
			}
			Done[Pick] = true;
			Order.push_back(Pick);
			for (const uint32 Dependent : Dependents[Pick])
			{
				--InDegree[Dependent];
			}
		}

		TickCounts.assign(N, 0);
		Built = true;
		return BuildResult::Ok;
	}

	std::vector<std::string_view> Scheduler::GetOrder() const
	{
		std::vector<std::string_view> Names;
		Names.reserve(Order.size());
		for (const uint32 Index : Order)
		{
			Names.emplace_back(Entries[Index].System->GetName());
		}
		return Names;
	}

	bool Scheduler::IsDue(SimLod Lod, SimTick Tick) const noexcept
	{
		const uint32 Level = ToUnderlying(Lod);
		if (Level >= SimLodCount)
		{
			return false;
		}
		return Tick % Lods.Period[Level] == 0;
	}

	uint32 Scheduler::RunTick(SimClock& Clock, const RandomStream& WorldStream, EntityRegistry& Entities,
							  ComponentStore& Components, EventBus* Events)
	{
		VAELEN_CHECKF(Built, "Scheduler::RunTick called before a successful Build()");
		if (!Built)
		{
			return 0;
		}
		const SimTick Tick = Clock.Now();
		uint32 Ran = 0;
		for (usize Position = 0; Position < Order.size(); ++Position)
		{
			Entry& E = Entries[Order[Position]];
			if (!IsDue(E.System->GetLod(), Tick))
			{
				continue;
			}
			// A fresh stream per system per tick: a function of (world seed,
			// system name, tick) only, so replay and partial re-simulation agree.
			E.Stream = WorldStream.Derive(E.NameHash).Fork(Tick);

			TickContext Context;
			Context.Tick = Tick;
			Context.Clock = &Clock;
			Context.Entities = &Entities;
			Context.Components = &Components;
			Context.Random = &E.Stream;
			Context.Events = Events;
			E.System->Tick(Context);
			++TickCounts[Position];
			++Ran;
		}
		Clock.Advance();
		return Ran;
	}
} // namespace Vaelen
