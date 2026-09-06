// VAELEN - VaelenSim
// Phase 03.06: the pre-history run and the starting state.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim

#include "Vaelen/Sim/PreHistory.h"

#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <cstdio>

namespace Vaelen::History
{
	PreHistory::PreHistory(World& InWorld, const PreHistoryRules& InRules) : Owner(&InWorld), Rules_(InRules)
	{
		World& W = *Owner;
		Types_.World = WorldGen::WorldSetup::Declare(W);
		Types_.Population = PopulationTypes::Declare(W);
		Types_.History = HistoryTypes::Declare(W);
		Types_.Languages = LanguageTypes::Declare(W);
		Types_.Religion = ReligionTypes::Declare(W);
		Types_.Disasters = DisasterTypes::Declare(W);

		PopulationSystem_ = std::make_unique<PopulationSystem>(W, Types_.World, Types_.Population, Rules_.Population);
		MigrationSystem_ = std::make_unique<MigrationSystem>(W, Types_.World, Types_.Population, Rules_.Population);
		EraSystem_ = std::make_unique<EraSystem>(W, Types_.History, Rules_.Eras);
		LanguageSystem_ =
			std::make_unique<LanguageSystem>(W, Types_.World, Types_.Population, Types_.Languages, Rules_.Languages);
		LanguageSystem_->NameEras(Types_.History.Era);
		ReligionSystem_ =
			std::make_unique<ReligionSystem>(W, Types_.World, Types_.Population, Types_.Religion, Rules_.Religion);
		ReligionSystem_->NameWith(Types_.Languages);
		FaithListener_ = std::make_unique<FaithListener>(W, Types_.World, Types_.Population, Types_.Religion,
														 Rules_.Religion, ReligionSystem_.get());
		DisasterSystem_ =
			std::make_unique<DisasterSystem>(W, Types_.World, Types_.Population, Types_.Disasters, Rules_.Disasters);
		DisasterSystem_->ShakeFaith(Types_.Religion, ReligionSystem_.get());
		DisasterSystem_->OpenEras(EraSystem_.get());
		Chronicle_ = std::make_unique<Chronicle>(W, Types_.History, Types_.World.RegionTypes_);

		W.Systems().Add(PopulationSystem_.get());
		W.Systems().Add(MigrationSystem_.get());
		W.Systems().Add(EraSystem_.get());
		W.Systems().Add(LanguageSystem_.get());
		W.Systems().Add(ReligionSystem_.get());
		W.Systems().Add(DisasterSystem_.get());
		FaithListener_->Listen(W.Events());
		// The chronicle keeps what history is made of; omens, waves and
		// conversions stay in the event log only.
		Chronicle_->Chronicle_(EraOpenedEvent.TypeHash);
		Chronicle_->Chronicle_(EraClosedEvent.TypeHash);
		Chronicle_->Chronicle_(CultureFoundedEvent.TypeHash);
		Chronicle_->Chronicle_(CultureSplitEvent.TypeHash);
		Chronicle_->Chronicle_(RegionSettledEvent.TypeHash);
		Chronicle_->Chronicle_(RegionAbandonedEvent.TypeHash);
		Chronicle_->Chronicle_(LanguageFoundedEvent.TypeHash);
		Chronicle_->Chronicle_(ReligionFoundedEvent.TypeHash);
		Chronicle_->Chronicle_(SchismEvent.TypeHash);
		Chronicle_->Chronicle_(DisasterStruckEvent.TypeHash);
	}

	PreHistory::~PreHistory() = default;

	bool PreHistory::HasHistory() const noexcept
	{
		return Owner->Components().GetPool(Types_.History.State).Size() > 0;
	}

	bool PreHistory::Generate(const WorldGenConfig& Config, uint32 Years, bool RunYears)
	{
		World& W = *Owner;
		if (HasHistory() || W.Now() != W.Config().StartTick)
		{
			return false; // not a fresh world
		}
		if (!WorldGen::GenerateWorld(W, Types_.World, Config))
		{
			return false;
		}
		if (SeedCultures(W, Types_.World, Types_.Population, Rules_.Population, W.Now()) == 0)
		{
			return false; // nowhere to live: the world stays generated but empty
		}
		InitializeHistory(W, Types_.History);
		InitializeFaith(W, Types_.Religion);
		InitializeDisasters(W, Types_.Disasters);
		if (RunYears)
		{
			Run(Years == 0 ? Rules_.Years : Years);
		}
		return true;
	}

	void PreHistory::Run(uint32 Years)
	{
		Owner->TickMany(TicksPerYear * Years);
	}

	PreHistoryReport ReportPreHistory(const World& W, const PreHistoryTypes& Types)
	{
		PreHistoryReport R;
		R.Tick = W.Now();
		R.Years = static_cast<uint32>(W.Now() / TicksPerYear);
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach([&](EntityHandle, const WorldGen::RegionInfo&) { ++R.Regions; });
		R.Population = MeasurePopulation(W, Types.Population);
		R.Names = MeasureNames(W, Types.Languages);
		R.Faith = MeasureFaith(W, Types.Population, Types.Religion);
		R.Disasters = MeasureDisasters(W, Types.Disasters);
		W.Components().GetPool(Types.History.Era).ForEach([&](EntityHandle, const EraInfo&) { ++R.Eras; });
		W.Components().GetPool(Types.History.Record).ForEach([&](EntityHandle, const RecordInfo&) { ++R.Records; });
		R.Events = W.Log().Count();
		R.Entities = W.Entities().GetAliveCount();
		R.StateDigest = ComputeStateDigest(W);
		R.LogDigest = W.Log().Digest();
		return R;
	}

	void ExportPreHistoryText(const PreHistoryReport& R, std::string& Out)
	{
		char Line[256];
		Out.clear();
		auto Add = [&](const char* Text)
		{
			Out += Text;
			Out += '\n';
		};
		std::snprintf(Line, sizeof(Line), "pre-history: %u years, tick %llu, %u regions, %u entities, %llu events",
					  R.Years, static_cast<unsigned long long>(R.Tick), R.Regions, R.Entities,
					  static_cast<unsigned long long>(R.Events));
		Add(Line);
		std::snprintf(Line, sizeof(Line), "peoples: %llu people of %llu capacity, %u cultures, %u/%u regions settled",
					  static_cast<unsigned long long>(R.Population.People),
					  static_cast<unsigned long long>(R.Population.Capacity), R.Population.Cultures,
					  R.Population.SettledRegions, R.Population.Regions);
		Add(Line);
		std::snprintf(Line, sizeof(Line), "names: %u languages, %u names (%u regions, %u rivers, %u lakes, %u eras)",
					  R.Names.Languages, R.Names.Names, R.Names.PerScope[static_cast<uint32>(NameScope::Region)],
					  R.Names.PerScope[static_cast<uint32>(NameScope::River)],
					  R.Names.PerScope[static_cast<uint32>(NameScope::Lake)],
					  R.Names.PerScope[static_cast<uint32>(NameScope::Era)]);
		Add(Line);
		std::snprintf(Line, sizeof(Line), "faiths: %u religions (%u schisms), %llu believers, %u/%u regions converted",
					  R.Faith.Religions, R.Faith.Schisms, static_cast<unsigned long long>(R.Faith.Adherents),
					  R.Faith.ConvertedRegions, R.Faith.Regions);
		Add(Line);
		std::snprintf(Line, sizeof(Line),
					  "disasters: %u (drought %u, flood %u, eruption %u, plague %u), %llu deaths, %u omens",
					  R.Disasters.Total, R.Disasters.PerKind[0], R.Disasters.PerKind[1], R.Disasters.PerKind[2],
					  R.Disasters.PerKind[3], static_cast<unsigned long long>(R.Disasters.Deaths), R.Disasters.Omens);
		Add(Line);
		std::snprintf(Line, sizeof(Line), "chronicle: %u eras, %u records", R.Eras, R.Records);
		Add(Line);
		std::snprintf(Line, sizeof(Line), "digests: state %016llx, log %016llx",
					  static_cast<unsigned long long>(R.StateDigest), static_cast<unsigned long long>(R.LogDigest));
		Add(Line);
	}
} // namespace Vaelen::History
