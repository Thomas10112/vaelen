// VAELEN - VaelenPopulation
// Phase 04.07: persons in history - the records that matter, one line for
// every person event, a person's story and the why of a death.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic tests in Tests/Population
//
// The Phase 03 chronicle records the events of peoples, faiths and disasters.
// This module adds the persons: a listener that turns the person events that
// matter (a house founded or died out, a death with a cause, a head of house
// gone, the chronicle's focus moving) into the same Record entities, capped
// per year so a crowded region does not drown the chronicle; text for every
// person event; the timeline of a person and the why-chain of a death,
// reaching the disaster and its omen through the Phase 03 queries.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/PreHistory.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	struct PersonChronicleRules
	{
		uint32 RecordFoundings = 1;		///< FamilyFounded
		uint32 RecordExtinctions = 1;	///< FamilyExtinct
		uint32 RecordCausedDeaths = 1;	///< PersonDied with a cause id (famine, plague)
		uint32 RecordHeadDeaths = 1;	///< PersonDied of a head of house
		uint32 RecordHeadMarriages = 1; ///< PersonMarried of a head of house
		uint32 RecordCrossings = 0;		///< PersonLeft / PersonArrived
		uint32 RecordFocus = 1;			///< RegionPromoted / RegionDemoted
		uint32 MaxRecordsPerYear = 64;	///< per region, then the rest of the year is silent
	};

	/// Singleton component: the listener's tallies (state, snapshot-safe).
	struct PersonChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0; ///< events that mattered but fell over the yearly cap
		uint32 Year = 0;	///< year of the running count
		uint32 Region = 0;	///< region of the running count (one region at a time is enough:
							///< events of a tick arrive region by region)
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(PersonChronicleState) == 24, "PersonChronicleState must stay padding free");

	struct PersonChronicleTypes
	{
		ComponentType<PersonChronicleState> State;
		static PersonChronicleTypes Declare(World& W);
	};

	/// Listener: the person events that matter become chronicle records.
	/// Subscribe with Attach after the systems exist (before Build).
	class VAELEN_POPULATION_API PersonChronicle final : public IEventListener
	{
	public:
		PersonChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons,
						FamilyTypes InFamilies, PersonChronicleTypes InState, PersonChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "PersonChronicle"; }
		void OnEvent(const Event& E) override;
		/// Subscribes to every person event type it may record.
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Region) const;
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		FamilyTypes Families;
		PersonChronicleTypes State;
		PersonChronicleRules Rules;
	};

	/// The name of a person for text ("Umamissar", or "person 12" when unnamed or unknown).
	VAELEN_POPULATION_API void NamePerson(const World& W, const History::PreHistoryTypes& Types,
										  const PersonTypes& Persons, uint32 Person, std::string& Out);
	/// The name of a house ("the house of Ukro", by its founder).
	VAELEN_POPULATION_API void NameFamily(const World& W, const History::PreHistoryTypes& Types,
										  const PersonTypes& Persons, const FamilyTypes& Families, uint32 Family,
										  std::string& Out);

	/// One line for any event: the person events get their own sentence, every
	/// other event goes through History::DescribeEvent.
	VAELEN_POPULATION_API void DescribePersonEvent(const World& W, const History::PreHistoryTypes& Types,
												   const PersonTypes& Persons, const FamilyTypes& Families,
												   const Event& E, std::string& Out);
	/// The whole chronicle (Phase 03 records and person records) as text, in
	/// tick order, one line each, at most MaxLines (0 = all).
	VAELEN_POPULATION_API uint32 ExportChronicleWithPersons(const World& W, const History::PreHistoryTypes& Types,
															const PersonTypes& Persons, const FamilyTypes& Families,
															std::string& Out, uint32 MaxLines = 0);

	/// Every event about a person, in log order: born, married (as groom or
	/// bride), left, arrived, died, and the founding of a house by them.
	VAELEN_POPULATION_API void PersonTimeline(const World& W, const PersonTypes& Persons, uint32 Person,
											  std::vector<const Event*>& Out);
	/// The death event of a person, or nullptr while alive or unknown.
	VAELEN_POPULATION_API const Event* DeathOf(const World& W, const PersonTypes& Persons, uint32 Person);
	/// The story of a person as text: the timeline, then, when the death has a
	/// cause, the why-chain from the death to the root cause. Returns the lines.
	VAELEN_POPULATION_API uint32 ExportPersonStory(const World& W, const History::PreHistoryTypes& Types,
												   const PersonTypes& Persons, const FamilyTypes& Families,
												   uint32 Person, std::string& Out);

	struct PersonChronicleStats
	{
		uint32 Records = 0; ///< person records (types of this module)
		uint32 Dropped = 0;
		uint32 Described = 0; ///< records with a specific line
		uint32 WithRegion = 0;
		uint32 EraConsistent = 0;
		uint32 ByType[8] = {}; ///< founded, extinct, died, married, left, arrived, promoted, demoted
	};
	VAELEN_POPULATION_API PersonChronicleStats CheckPersonChronicle(const World& W,
																	const History::PreHistoryTypes& Types,
																	const PersonTypes& Persons,
																	const FamilyTypes& Families,
																	const PersonChronicleTypes& State);
} // namespace Vaelen::Population
