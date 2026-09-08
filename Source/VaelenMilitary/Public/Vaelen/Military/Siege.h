// VAELEN - VaelenMilitary
// Phase 08.04: siege - a host before a seat, and the only way a capital ever
// changes hands.
//
// STATUS: PROTOTYPE (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military
//
// Everything else in the phase moves men about and kills them. Nothing so far
// takes anything. A province can slip to a neighbour when its ruler's grip
// fails (07.03), and a faction can carry one off (07.05), but a seat is what a
// polity is: 07.01 dissolves a polity that has lost its seat, whatever else it
// still holds. So a seat must be hard to take, and it must be taken by sitting
// in front of it.
//
// A host that stands on an enemy seat invests it. Nothing comes down in the
// year it arrives - a seat does not fall in a season - and after that the wall
// comes down at a rate set by how many men are sitting before it. A seat left
// alone rebuilds, slowly, so a besieger that is beaten off or marches away has
// to begin again. When the wall is gone the seat changes hands, and the polity
// that held it finds out next year, from 07.01, that it no longer exists.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/Battle.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Military
{
	/// Component on a region entity that is, or has been, a seat under siege.
	/// It outlives the siege: a wall that has been breached stays breached
	/// until it is rebuilt, and the next besieger inherits the work.
	struct SiegeInfo
	{
		uint32 Besieger = 0; ///< polity sitting before it, 0 = nobody
		uint32 Army = 0;	 ///< its host
		uint32 Held = 0;	 ///< polity whose seat this is, as of the last year it was besieged
		uint32 Years = 0;	 ///< consecutive years this besieger has sat before it
		uint32 Wall = 0;	 ///< per mille still standing
		uint32 Taken = 0;	 ///< times this seat has been stormed
		uint64 Laid = 0;	 ///< tick the present siege was laid, 0 when none is
	};
	static_assert(sizeof(SiegeInfo) == 32, "SiegeInfo must stay padding free");

	struct SiegeTypes
	{
		ComponentType<SiegeInfo> Siege;
		static VAELEN_MILITARY_API SiegeTypes Declare(World& W);
	};

	struct SiegeRules
	{
		uint32 WallAtFull = 1000;		 ///< per mille a seat stands at
		uint32 MenToInvest = 60;		 ///< a host under this cannot shut a seat in
		uint32 WallPerHundredMen = 250;	 ///< per mille a hundred men bring down in a year
		uint32 YearsBeforeBreaching = 1; ///< years before anything comes down at all
		uint32 MendPerYear = 120;		 ///< per mille a seat rebuilds in a year nobody sits before it
	};

	/// A host shut a seat in (the besieger, the seat, the army, the wall standing).
	inline constexpr EventType<Politics::PolityPayload> SiegeLaidEvent =
		MakeEventType<Politics::PolityPayload>("SiegeLaid");
	/// The host before a seat is gone (the besieger, the seat, the army, the wall standing).
	inline constexpr EventType<Politics::PolityPayload> SiegeLiftedEvent =
		MakeEventType<Politics::PolityPayload>("SiegeLifted");
	/// A seat was stormed and changed hands (the taker, the seat, the army, the polity that lost it).
	inline constexpr EventType<Politics::PolityPayload> SeatTakenEvent =
		MakeEventType<Politics::PolityPayload>("SeatTaken");

	/// Yearly, after Battles: press every siege a host is sitting on, mend
	/// every seat nobody is sitting on, and hand over the seats whose walls
	/// have gone.
	class VAELEN_MILITARY_API SiegeSystem final : public ISystem
	{
	public:
		SiegeSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Politics::PolityTypes InPolities,
					Politics::ReachTypes InReaches, Politics::DiplomacyTypes InRelations, ArmyTypes InArmies,
					SiegeTypes InSieges, SiegeRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Polities(InPolities), Reaches(InReaches), Relations(InRelations),
			  Armies(InArmies), Sieges(InSieges), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Sieges"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Battles"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Politics::PolityTypes Polities;
		Politics::ReachTypes Reaches;
		Politics::DiplomacyTypes Relations;
		ArmyTypes Armies;
		SiegeTypes Sieges;
		SiegeRules Rules;
	};

	/// What a region's walls know (nullptr when it has never been besieged).
	VAELEN_MILITARY_API const SiegeInfo* SiegeOf(const World& W, const History::PreHistoryTypes& Types,
												 const SiegeTypes& Sieges, uint32 Region);

	struct SiegeStats
	{
		uint32 Pressed = 0;	 ///< seats with a host sitting before them
		uint32 Breached = 0; ///< seats standing at less than a full wall
		uint32 Stormed = 0;	 ///< seats that have been taken, over the world's history
		uint32 Laid = 0;	 ///< events, from the log
		uint32 Lifted = 0;
		uint32 Taken = 0;
		uint32 Bad = 0;	   ///< a polity besieging itself, a wall over full, a siege pressed by nobody,
						   ///< or a besieger with no army named
		Hash64 Digest = 0; ///< every siege in region order
	};
	VAELEN_MILITARY_API SiegeStats MeasureSieges(const World& W, const History::PreHistoryTypes& Types,
												 const SiegeTypes& Sieges, const SiegeRules& Rules);
} // namespace Vaelen::Military
