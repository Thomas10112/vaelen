// VAELEN - VaelenSim
// Phase 03.06: the pre-history run and the starting state.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// One object wires every Phase 03 system over a world (population and
// migration, eras and the chronicle, languages and names, religions and their
// listener, disasters) and one call generates the world, seeds the first
// cultures and runs N years at the world LOD. The result is the starting
// state Phase 04 inherits: regions with peoples, faiths and names, a
// chronicle of records with causes, eras with names. Everything the run
// produces is components and events, so the state can be snapshotted at any
// tick and continued identically.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <memory>
#include <string>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	struct PreHistoryRules
	{
		PopulationRules Population;
		EraRules Eras;
		LanguageRules Languages;
		ReligionRules Religion;
		DisasterRules Disasters;
		uint32 Years = 500; ///< default length of the run
	};

	struct PreHistoryTypes
	{
		WorldGen::WorldSetup World;
		PopulationTypes Population;
		HistoryTypes History;
		LanguageTypes Languages;
		ReligionTypes Religion;
		DisasterTypes Disasters;
	};

	/// Owns the Phase 03 systems and listeners of one world. Construct before
	/// World::Build (it declares the types and adds the systems), then call
	/// Generate once on the fresh world, or LoadSnapshot into the world and
	/// call Run.
	class VAELEN_SIM_API PreHistory
	{
	public:
		PreHistory(World& InWorld, const PreHistoryRules& InRules);
		PreHistory(const PreHistory&) = delete;
		PreHistory& operator=(const PreHistory&) = delete;
		~PreHistory();

		const PreHistoryTypes& Types() const noexcept { return Types_; }
		const PreHistoryRules& Rules() const noexcept { return Rules_; }

		/// Generates the world from the config, initialises history, faith and
		/// disasters, seeds the first cultures and runs Years years (Rules.Years
		/// when 0 is passed and RunYears is false). False and nothing changed
		/// when the world is not fresh (history already initialised or the
		/// clock past its start), when generation fails or when no culture
		/// could be seeded; the world is then left as generated.
		bool Generate(const WorldGenConfig& Config, uint32 Years, bool RunYears = true);
		/// Ticks Years more years.
		void Run(uint32 Years);
		/// True once Generate succeeded or a snapshot with history was loaded.
		bool HasHistory() const noexcept;

		EraSystem& Eras() noexcept { return *EraSystem_; }
		ReligionSystem& Religions() noexcept { return *ReligionSystem_; }
		DisasterSystem& Disasters() noexcept { return *DisasterSystem_; }
		LanguageSystem& Languages() noexcept { return *LanguageSystem_; }
		Chronicle& Records() noexcept { return *Chronicle_; }

	private:
		World* Owner;
		PreHistoryRules Rules_;
		PreHistoryTypes Types_;
		std::unique_ptr<PopulationSystem> PopulationSystem_;
		std::unique_ptr<MigrationSystem> MigrationSystem_;
		std::unique_ptr<EraSystem> EraSystem_;
		std::unique_ptr<LanguageSystem> LanguageSystem_;
		std::unique_ptr<ReligionSystem> ReligionSystem_;
		std::unique_ptr<FaithListener> FaithListener_;
		std::unique_ptr<DisasterSystem> DisasterSystem_;
		std::unique_ptr<Chronicle> Chronicle_;
	};

	/// Ticks per year of the default calendar.
	inline constexpr uint64 TicksPerYear = 8640;

	struct PreHistoryReport
	{
		uint64 Tick = 0;
		uint32 Years = 0;
		uint32 Regions = 0;
		PopulationStats Population;
		NamingStats Names;
		FaithStats Faith;
		DisasterStats Disasters;
		uint32 Eras = 0;
		uint32 Records = 0;
		uint64 Events = 0;
		uint32 Entities = 0;
		Hash64 StateDigest = 0;
		Hash64 LogDigest = 0;
	};
	VAELEN_SIM_API PreHistoryReport ReportPreHistory(const World& W, const PreHistoryTypes& Types);
	/// Human-readable lines of a report (deterministic text).
	VAELEN_SIM_API void ExportPreHistoryText(const PreHistoryReport& R, std::string& Out);
} // namespace Vaelen::History
