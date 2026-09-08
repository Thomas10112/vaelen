// VAELEN - VaelenMilitary
// Phase 08.01: levies and armies - who a polity can call up, what calling them
// up costs, and what happens to men it cannot feed.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military
//
// An army is not a thing a polity owns; it is people taken out of regions. A
// levy is raised only where there is a reason - a polity at war (07.06) - and
// only from ground it actually holds: a region held loosely gives few men and
// one held under the floor gives none, because a levy is obedience and 07.03
// already measures obedience. The men are recorded on the regions they came
// from, so nothing is invented and nothing is lost.
//
// Standing armies eat. Every year an army costs grain out of the treasury
// 07.02 fills, and an army the polity cannot feed melts away - it is not
// disbanded by an order, it simply stops being there. When the war it was
// raised for ends, it goes home and the men return to their regions.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Sim/Event.h"
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
	/// Component of an army entity (ids of kind Army).
	struct ArmyInfo
	{
		uint32 Index = 0;	  ///< 1-based, in order of raising
		uint32 Polity = 0;	  ///< whose it is
		uint32 Region = 0;	  ///< where it stands; at the seat while it is only raised
		uint32 Strength = 0;  ///< men under arms
		uint32 Fed = 0;		  ///< grain it was given last year
		uint32 Hungry = 0;	  ///< consecutive years it went short
		uint64 Raised = 0;	  ///< tick
		uint64 Disbanded = 0; ///< tick, 0 while it stands
		Hash64 Identity = 0;  ///< from the world seed, for names
	};
	static_assert(sizeof(ArmyInfo) == 48, "ArmyInfo must stay padding free");

	/// Component on a region entity: the men it has given, and to whom. A levy
	/// is a debt of people, and the region remembers it until they come home.
	struct RegionLevy
	{
		uint32 Polity = 0; ///< who called them up, 0 = nobody
		uint32 Men = 0;	   ///< men away from this region
	};
	static_assert(sizeof(RegionLevy) == 8, "RegionLevy must stay padding free");

	struct ArmyTypes
	{
		ComponentType<ArmyInfo> Army;
		ComponentType<RegionLevy> Levy;
		static VAELEN_MILITARY_API ArmyTypes Declare(World& W);
	};

	struct ArmyRules
	{
		uint32 MenPerThousand = 20;	   ///< men a region gives per thousand living, at a full hold
		uint32 LevyFloorHold = 300;	   ///< per mille; under this a region gives nobody
		uint32 GrainPerManPerYear = 3; ///< what a man under arms eats out of the treasury
		uint32 RaiseAtStrength = 60;   ///< a levy under this many men is not worth calling
		uint32 MeltPerMille = 400;	   ///< of an army's men lost in a year it is not fed
		uint32 HungryBeforeGone = 3;   ///< years short before what is left goes home
		uint32 ArmiesPerPolity = 1;	   ///< 08.01 raises one host; 08.02 marches it
	};

	/// A polity called up a levy (Polity, the seat, the army, men).
	inline constexpr EventType<Politics::PolityPayload> ArmyRaisedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyRaised");
	/// An army went home (Polity, where it stood, the army, men returned).
	inline constexpr EventType<Politics::PolityPayload> ArmyDisbandedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyDisbanded");
	/// An army was not fed and lost men (Polity, where it stood, the army, men lost).
	inline constexpr EventType<Politics::PolityPayload> ArmyStarvedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyStarved");

	/// Yearly, after Diplomacy: raise a host where there is a war and none
	/// stands, feed what stands out of the treasury, let what cannot be fed melt
	/// away, and send home what has no war left to fight.
	class VAELEN_MILITARY_API ArmySystem final : public ISystem
	{
	public:
		ArmySystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::EconomyTypes InEconomy,
				   Politics::PolityTypes InPolities, Politics::LawTypes InLaws, Politics::ReachTypes InReaches,
				   Politics::DiplomacyTypes InRelations, ArmyTypes InArmies, ArmyRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Economy(InEconomy), Polities(InPolities), Laws(InLaws),
			  Reaches(InReaches), Relations(InRelations), Armies(InArmies), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Armies"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Diplomacy"};
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
		Economy::EconomyTypes Economy;
		Politics::PolityTypes Polities;
		Politics::LawTypes Laws;
		Politics::ReachTypes Reaches;
		Politics::DiplomacyTypes Relations;
		ArmyTypes Armies;
		ArmyRules Rules;
	};

	/// An army by index (nullptr when unknown), standing or gone.
	VAELEN_MILITARY_API const ArmyInfo* ArmyOf(const World& W, const ArmyTypes& Armies, uint32 Army);
	/// The armies a polity has standing, in index order.
	VAELEN_MILITARY_API void ArmiesOf(const World& W, const ArmyTypes& Armies, uint32 Polity, std::vector<uint32>& Out);
	/// What a region has given (nullptr when it has never been called upon).
	VAELEN_MILITARY_API const RegionLevy* LevyOf(const World& W, const History::PreHistoryTypes& Types,
												 const ArmyTypes& Armies, uint32 Region);

	struct ArmyStats
	{
		uint32 Standing = 0; ///< armies not disbanded
		uint32 Gone = 0;
		uint32 Men = 0;		 ///< men under arms, over every standing army
		uint32 Away = 0;	 ///< men the regions say are away
		uint32 Levied = 0;	 ///< regions that have given men
		uint64 Fed = 0;		 ///< grain spent feeding armies, over every standing one, last year
		uint32 Raisings = 0; ///< events, from the log
		uint32 Disbandings = 0;
		uint32 Starvings = 0;
		uint32 Bad = 0;	   ///< an army of a polity that is gone, standing in ground it does not hold,
						   ///< with no men, or men away that no army accounts for
		Hash64 Digest = 0; ///< every army in index order, then every levy in region order
	};
	VAELEN_MILITARY_API ArmyStats MeasureArmies(const World& W, const History::PreHistoryTypes& Types,
												const Politics::PolityTypes& Polities, const ArmyTypes& Armies,
												const ArmyRules& Rules);
} // namespace Vaelen::Military
