// VAELEN - VaelenPopulation
// Phase 04.04: needs and body - food, health, famine and disease.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population
//
// Every person of a detailed region carries needs: food and health as small
// integers (rest is kept as a slot for Phase 10, where it matters). Each year
// the region's ration - its capacity over its living - refills food, a drought
// that struck the region cuts the ration, hunger wears health down by a draw
// that grows with the deficit, a plague that struck the region strikes a share
// of the persons for a draw, and whoever reaches zero health dies with the
// disaster's event as the cause. The counts follow
// the persons (reconciliation), so famine and disease reach the coarse world
// through the same door as every other death.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	/// Component on a person entity while it lives in a detailed region.
	struct PersonNeeds
	{
		uint8 Food = 200;	///< 0 starving .. 255 sated
		uint8 Health = 200; ///< 0 dead .. 255 whole
		uint8 Rest = 200;	///< reserved for Phase 10
		uint8 Hungry = 0;	///< years in a row under the hunger line (saturates)
		uint32 Reserved = 0;
	};
	static_assert(sizeof(PersonNeeds) == 8, "PersonNeeds must stay padding free");

	struct NeedTypes
	{
		ComponentType<PersonNeeds> Needs;
		static NeedTypes Declare(World& W);
	};

	struct NeedRules
	{
		uint32 FoodBurn = 200;		///< food a year costs
		uint32 FoodRefillMax = 300; ///< food a year of full ration brings (good years rebuild the stores)
		uint32 HungerLine = 200;	///< hunger when the stores cannot cover a year
		uint32 HungerDamage = 40;	///< health lost in a hungry year, plus a draw below the deficit
		uint32 HungerDeficitFactor = 1;
		uint32 HealthRecovery = 40;						 ///< health regained in a fed year
		uint32 DroughtCutPerMille[3] = {300, 600, 900};	 ///< ration lost by severity 1..3
		uint32 PlagueSharePerMille[3] = {150, 300, 500}; ///< persons struck by severity
		uint32 PlagueDamage[3] = {300, 360, 420};		 ///< a struck person loses a draw below this
		uint32 ElderFrom = 60;							 ///< elders and infants take an extra share of every blow
		uint32 FrailExtraPerMille = 300;
		uint32 FamineMemoryYears = 3; ///< hunger this long after a drought is still that drought's famine
	};

	/// Cause codes carried in PersonPayload::Other of a PersonDied event.
	enum class DeathCause : uint32
	{
		Natural = 0,
		Famine = 1,		///< hunger after a drought
		Starvation = 2, ///< hunger without a drought (a region past its capacity)
		Plague = 3,
	};

	/// Yearly, for every detailed region: rations, famine, disease, deaths with
	/// their cause, reconciliation.
	class VAELEN_POPULATION_API NeedSystem final : public ISystem
	{
	public:
		NeedSystem(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons, NeedTypes InNeeds,
				   NeedRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Needs(InNeeds), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Needs"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Lives"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		NeedTypes Needs;
		NeedRules Rules;
	};

	struct NeedStats
	{
		uint32 WithNeeds = 0; ///< living persons carrying needs
		uint32 Hungry = 0;	  ///< under the hunger line
		uint32 Weak = 0;	  ///< health under half
		uint64 FoodSum = 0;
		uint64 HealthSum = 0;
		uint32 FamineDeaths = 0; ///< from the log
		uint32 StarvationDeaths = 0;
		uint32 PlagueDeaths = 0;
		uint32 NaturalDeaths = 0;
		uint32 CausedDeaths = 0; ///< deaths whose event carries a cause
	};
	VAELEN_POPULATION_API NeedStats MeasureNeeds(const World& W, const PersonTypes& Persons, const NeedTypes& Needs,
												 uint32 Region);
} // namespace Vaelen::Population
