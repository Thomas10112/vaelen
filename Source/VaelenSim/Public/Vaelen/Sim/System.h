// VAELEN - VaelenSim
// Simulation systems and the deterministic scheduler.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// A system is a named unit of simulation logic with:
//   - explicit dependencies ("run after these systems"), by name;
//   - a simulation LOD level 0-4 that decides how often it ticks
//     (LOD 0 every tick, level n every LodPeriod[n] ticks);
//   - its own random stream, derived by name from the world stream
//     (ADR-0003), so adding or removing another system never perturbs it.
//
// The scheduler orders systems by their dependencies (Kahn's algorithm) with
// a stable tie-break on the name hash: the execution order is a pure function
// of the set of systems, independent of registration order and of memory
// addresses. A dependency cycle or an unknown dependency is reported and the
// scheduler refuses to run until it is fixed.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/ComponentStore.h"
#include "Vaelen/Sim/EntityRegistry.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/SimClock.h"

#include <string_view>
#include <vector>

namespace Vaelen
{
	class EventBus; // 01.05

	/// Simulation level of detail of a system (master prompt section 36).
	enum class SimLod : uint8
	{
		Full = 0,	   ///< every tick
		Detailed = 1,  ///< local: every LodPeriod[1] ticks
		Aggregate = 2, ///< town/region
		Statistic = 3, ///< distant region
		World = 4,	   ///< very distant world
	};

	inline constexpr uint32 SimLodCount = 5;

	/// Everything a system may touch during a tick. Systems must not keep
	/// pointers into it beyond the call.
	struct TickContext
	{
		SimTick Tick = 0;
		const SimClock* Clock = nullptr;
		EntityRegistry* Entities = nullptr;
		ComponentStore* Components = nullptr;
		RandomStream* Random = nullptr; ///< the system's own stream
		EventBus* Events = nullptr;		///< null until 01.05
	};

	class VAELEN_SIM_API ISystem
	{
	public:
		virtual ~ISystem() = default;

		/// Unique, stable name (string literal). Determines stream derivation
		/// and tie-break order: renaming a system changes its random sequence.
		virtual const char* GetName() const noexcept = 0;

		/// Names of the systems that must run before this one, each tick.
		virtual std::vector<std::string_view> GetDependencies() const { return {}; }

		virtual SimLod GetLod() const noexcept { return SimLod::Full; }

		virtual void Tick(TickContext& Context) = 0;
	};

	/// Tick periods per LOD level, in ticks (index = SimLod). Level 0 is always 1.
	struct LodSchedule
	{
		uint32 Period[SimLodCount] = {1, 4, 24, 720, 8640};

		constexpr bool IsValid() const noexcept
		{
			return Period[0] == 1 && Period[1] > 0 && Period[2] > 0 && Period[3] > 0 && Period[4] > 0;
		}
		constexpr bool operator==(const LodSchedule&) const noexcept = default;
	};

	/// Orders and runs systems. Systems are not owned; they must outlive the
	/// scheduler or be removed first.
	class VAELEN_SIM_API Scheduler
	{
	public:
		enum class BuildResult : uint8
		{
			Ok,
			DuplicateName,
			UnknownDependency,
			Cycle,
			InvalidLodSchedule,
		};

		/// Adds a system. Returns false (and reports) for a null system or a
		/// duplicate name. Invalidates the current order until Build().
		bool Add(ISystem* System);
		bool Remove(std::string_view Name);
		uint32 Count() const noexcept { return static_cast<uint32>(Entries.size()); }
		bool Contains(std::string_view Name) const noexcept;

		void SetLodSchedule(const LodSchedule& Schedule) noexcept;
		const LodSchedule& GetLodSchedule() const noexcept { return Lods; }

		/// Computes the execution order. Must be called after Add/Remove and
		/// before Run. Idempotent.
		BuildResult Build();
		bool IsBuilt() const noexcept { return Built; }

		/// The execution order, valid after a successful Build().
		std::vector<std::string_view> GetOrder() const;

		/// Name of the offending system after a failed Build() (empty otherwise).
		std::string_view GetBuildError() const noexcept { return BuildError; }

		/// Runs every system due at Clock.Now() in order, with its own stream
		/// derived from WorldStream, then advances the clock by one tick.
		/// Returns the number of systems that ticked; 0 with a Check failure
		/// when the scheduler is not built.
		uint32 RunTick(SimClock& Clock, const RandomStream& WorldStream, EntityRegistry& Entities,
					   ComponentStore& Components, EventBus* Events = nullptr);

		/// True when a system of the given LOD is due at the tick.
		bool IsDue(SimLod Lod, SimTick Tick) const noexcept;

		/// Total ticks executed per system, in execution order (diagnostics).
		const std::vector<uint64>& GetTickCounts() const noexcept { return TickCounts; }

	private:
		struct Entry
		{
			ISystem* System = nullptr;
			Hash64 NameHash = 0;
			RandomStream Stream; ///< re-derived on every RunTick from the world stream
		};

		std::vector<Entry> Entries; ///< registration order (irrelevant to results)
		std::vector<uint32> Order;	///< indices into Entries, execution order
		std::vector<uint64> TickCounts;
		LodSchedule Lods;
		std::string_view BuildError;
		bool Built = false;
	};
} // namespace Vaelen
