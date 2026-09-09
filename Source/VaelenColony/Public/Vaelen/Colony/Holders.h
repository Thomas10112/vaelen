// VAELEN - VaelenColony
// Phase 11 task 11.04: who holds a colony.
//
// What this is not. 05.04 already binds people, frees them, hardens bondage
// into slavery and lets people run. None of that is rewritten here. What Phase
// 05 never had to answer is what happens when nearly EVERYBODY in one region is
// bound at once, and measuring it (Tests/Colony/Test_Holders.cpp) gave two
// answers worth the task:
//
//  - At 05.04's own rates a region settles near a twelfth of its people bound:
//    debt at fifteen per mille a year against manumission at twenty-five and
//    flight at eight. A colony where everybody is bound cannot GROW out of
//    that. It has to be founded that way, which is what the fiction always
//    said: the people are sent there.
//  - And the holders are not the limit at those rates - forty-eight elites hold
//    a hundred and fourteen people between them, a fifth of what they could.
//    They become the limit only once a colony is founded, because seven hundred
//    hands want fifty-nine holders at twelve each and the region has forty-eight.
//
// So a colony is held by the colony. `BondState::Holder` has meant "the region
// itself" since 05.04 and this is the first thing that needed it in earnest.
//
// STATUS: INCOMPLETE (Phase 11) - written, not yet tested
#pragma once

#include "Vaelen/Colony/ColonyApi.h"
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

namespace Vaelen
{
	class World;
}

namespace Vaelen::Colony
{
	struct BondingRules
	{
		uint32 FromAge = 12;			///< a person of the colony is put to the rock at this age, as 11.03 mines
		uint32 EnslavedPerMille = 1000; ///< of those bound, this many are enslaved rather than merely bonded
		uint32 SpareTheElite = 1;		///< the region's elite are not bound with the rest
	};

	/// Binds the people of a colony to the colony itself, at a tick. Returns how
	/// many were bound; 0 for a region that is not a colony.
	///
	/// Every bond goes through Society::BindPerson, so 05.04 owns every bond in
	/// the world as it always has, and the colony owns none of them.
	VAELEN_COLONY_API uint32 BindColony(World& W, const History::PreHistoryTypes& Types,
										const Population::PersonTypes& Persons, const Society::BondageTypes& Bondage,
										const Society::StandingTypes& Standing, const ColonyTypes& Colony,
										uint32 Region, SimTick Tick, BondingRules Rules = BondingRules{});

	/// Raises the overseers of a colony: the organisation of 05.01 whose members
	/// hold it for it. Returns its index, 0 for a region that is not a colony or
	/// already has one.
	///
	/// The yearly system seats them like any other organisation, from the region's
	/// unbound adults, and re-seats them as they die - so the overseers outlive
	/// the people in them, which is what an institution is.
	VAELEN_COLONY_API uint32 RaiseOverseers(World& W, const History::PreHistoryTypes& Types,
											const Society::OrganizationTypes& Organizations, const ColonyTypes& Colony,
											uint32 Region, uint32 Seats, SimTick Tick);

	struct HoldingStats
	{
		uint32 People = 0;	 ///< living in the region
		uint32 Bound = 0;	 ///< of them, not free
		uint32 ByPerson = 0; ///< held by somebody
		uint32 ByRegion = 0; ///< held by the colony itself
		uint32 Enslaved = 0;
		uint32 Elites = 0;	 ///< unbound elites, who are the only people who may hold anybody
		uint32 Capacity = 0; ///< Elites x MaxHeldPerHolder: what person-holding could carry
		uint32 Fullest = 0;	 ///< the most anybody holds
	};
	VAELEN_COLONY_API HoldingStats MeasureHolding(const World& W, const Population::PersonTypes& Persons,
												  const Society::BondageTypes& Bondage,
												  const Society::StandingTypes& Standing,
												  const Society::BondageRules& Rules, uint32 Region);
} // namespace Vaelen::Colony
