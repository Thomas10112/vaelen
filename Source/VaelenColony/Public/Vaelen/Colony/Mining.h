// VAELEN - VaelenColony
// Phase 11 task 11.03: the work of a colony. Hands on the deposits of 02.07,
// the ore credited through the stocks of 06.01, and the seam running out.
//
// What this is not. 06.02 already lifts ore: it sums the richness of a region's
// iron and copper deposits and adds five per mille of it to the common stock
// every year. This task does not replace that and does not add a second way to
// make ore. It gives ONE region - the colony's - the same lift at the colony's
// grain instead of at the year, by named hands rather than by a headcount, and
// makes what is taken run out.
//
// The seam running out is the only genuinely new fact in the task, and it is
// kept off every world that has no colony: the count of what has been lifted is
// a component of this module, on the deposit entity, and a world that never
// declares it behaves exactly as it did before. That is the rule 11.01 paid for
// - a hook is an observed type declared by the module that needs it, never a
// field pushed down into a lower module's Declare.
//
// STATUS: INCOMPLETE (Phase 11) - written, not yet compiled or tested
#pragma once

#include "Vaelen/Colony/ColonyApi.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Colony
{
	/// Component on a deposit entity (02.07): what this module has lifted out of
	/// it, against DepositInfo::Richness. Declared here and nowhere else, so a
	/// world with no colony has no pool of it and no digest of it.
	struct DepositTaken
	{
		uint32 Taken = 0;	 ///< units lifted so far, never above Richness
		uint32 Reserved = 0; ///< keeps the struct eight bytes, as every component is
	};
	static_assert(sizeof(DepositTaken) == 8, "DepositTaken must stay padding free");

	/// Component on the colony's region entity: who works the rock.
	struct ColonyInfo
	{
		uint32 Region = 0; ///< the region it is; 0 = not a colony
		uint32 Hands = 0;  ///< people put on the rock, recounted each tick
		uint32 Lifted = 0; ///< units of ore lifted over the colony's life
		uint32 Reserved = 0;
	};
	static_assert(sizeof(ColonyInfo) == 16, "ColonyInfo must stay padding free");

	struct ColonyTypes
	{
		ComponentType<ColonyInfo> Colony;
		ComponentType<DepositTaken> Taken;
		/// 06.02's mark for ground worked for what is under it. Declared through
		/// Economy::DeclareMined by this module and by nobody below it, so that a
		/// world with no colony carries no trace of one.
		ComponentType<Economy::RegionMined> Mined;

		static VAELEN_COLONY_API ColonyTypes Declare(World& W);
	};

	struct MiningRules
	{
		// No Region here. A colony is founded at a tick and a rule is fixed when the
		// system is built, so a rule cannot name one - ADR-0090, and this is the third
		// place in Phase 11 to learn it. The system mines whatever the colony pool
		// holds, which is the world's own answer to "where is it".
		uint32 DaysPerYear = 360;		 ///< the calendar of 01.04 at SimLod::Aggregate
		uint32 WorkFromAge = 12;		 ///< a person of the colony goes on the rock at this age
		uint32 PerHandPerYear = 2;		 ///< units of ore a pair of hands lifts in a year, at ordinary skill
		uint32 SkillFloorPerMille = 850; ///< a hand's yield: floor plus span by their skill
		uint32 SkillSpanPerMille = 300;
		uint32 GrainPerPersonPerYear = 4; ///< what the colony eats and does not grow (matches 06.02)
	};

	/// A colony founded on a region (Region, 0, Ore, 0). Nothing in the world
	/// happens without an event that says so, or no chronicle can tell it -
	/// FoundColony was silent until 11.07 needed it spoken.
	inline constexpr EventType<Economy::StockPayload> ColonyFoundedEvent =
		MakeEventType<Economy::StockPayload>("ColonyFounded");
	/// A colony's lift (Region, 0, Ore, units), with the colony as subject.
	inline constexpr EventType<Economy::StockPayload> OreLiftedEvent =
		MakeEventType<Economy::StockPayload>("OreLifted");
	/// A seam that has given everything it held (Region, deposit index, Ore, 0).
	inline constexpr EventType<Economy::StockPayload> SeamWorkedOutEvent =
		MakeEventType<Economy::StockPayload>("SeamWorkedOut");

	/// Daily, after ColonyDay: the colony's hands lift ore out of the seams of
	/// its region and eat grain they did not grow.
	///
	/// The ore reaches the world through Economy::AddStock and through nothing
	/// else, so the lift is in the event log with a cause and the chronicle of
	/// 11.07 can read it - which is the one thing 06.02's yearly extraction
	/// cannot give, because it writes the stock directly.
	///
	/// The yearly pass must not also extract on this ground or the ore is counted
	/// twice; Economy::RegionMined is what stops it, which FoundColony puts on
	/// the region at the tick the colony begins (ADR-0090).
	class VAELEN_COLONY_API MiningSystem final : public ISystem
	{
	public:
		MiningSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 Population::FamilyTypes InFamilies, Economy::EconomyTypes InEconomy, ColonyTypes InColony,
					 MiningRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Colony(InColony), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "ColonyMining"; }
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out;
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// The colony's day (11.02) runs first where it runs at all; a world that
		/// has no ColonyDay system still mines, because the two touch nothing in
		/// common - one spends food, the other lifts rock.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: the persons' skills (04.05) shape what a hand lifts. Without
		/// it every hand lifts the ordinary amount.
		void ObserveTraits(ComponentType<Population::PersonTraits> InTraits) noexcept
		{
			Traits = InTraits;
			HasTraits = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		Economy::EconomyTypes Economy;
		ColonyTypes Colony;
		MiningRules Rules;
		ComponentType<Population::PersonTraits> Traits;
		bool HasTraits = false;
	};

	/// Marks a region as the colony: the counters, and the mined mark 06.02 reads
	/// to stop reaping there. Returns false for an unknown region or one that is
	/// already a colony.
	///
	/// A colony BEGINS here, which is the whole reason the mark is a component:
	/// the world before this call is a world with no colony in it, pre-history
	/// included.
	VAELEN_COLONY_API bool FoundColony(World& W, const History::PreHistoryTypes& Types, const ColonyTypes& Colony,
									   uint32 Region);

	/// What is left in a region's ore seams: the summed richness less what has
	/// been lifted. The same arithmetic the system uses, for a caller that wants
	/// to ask rather than to walk the deposits.
	VAELEN_COLONY_API uint32 SeamLeft(const World& W, const History::PreHistoryTypes& Types, const ColonyTypes& Colony,
									  uint32 Region);

	struct MiningStats
	{
		uint32 Colonies = 0;
		uint32 Hands = 0;
		uint32 Seams = 0;	  ///< ore deposits in the colony's region
		uint32 WorkedOut = 0; ///< of them, given everything they held
		uint32 Richness = 0;  ///< summed richness of those seams
		uint32 Taken = 0;	  ///< summed lifted out of them
		uint32 Lifts = 0;	  ///< OreLifted events in the log
		Hash64 Digest = 0;	  ///< every colony then every taken count, in deposit index order
	};
	VAELEN_COLONY_API MiningStats MeasureMining(const World& W, const History::PreHistoryTypes& Types,
												const ColonyTypes& Colony, uint32 Region);
} // namespace Vaelen::Colony
