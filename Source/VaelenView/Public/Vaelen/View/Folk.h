// VAELEN - VaelenView
// Phase 13 task 13.08b's missing half: the PEOPLE, as a renderer needs them.
//
// 13.01 gave the view regions, 13.07a the ground, 13.08a the roads and the
// colony. 13.08b asks for "a person, a colony and a road drawn from the view of
// 13.01" - and the view had no person in it. Regions carry a HEAD COUNT, which
// draws a number over a province and cannot put one figure anywhere. The task
// could not be started, not because the engine was missing anything, but
// because the surface it was to draw from was short of the thing it names.
//
// WHY THIS IS NOT IN `WorldView`, for the same two reasons NetView is not.
//
// First the diff: 13.02's `Delta` compares a `WorldView` region by region. A
// vector of people added to that struct would be carried, ignored by the diff,
// and silently stale on every screen rebuilt from a delta.
//
// Second the weight, which is new here and larger. A `WorldView` of AELVOR at
// 128 is 5600 bytes - small enough to copy every frame without thinking, which
// is 13.01's whole promise. One detailed region holds thousands of people. Put
// them in the frame and every renderer that wants a map pays for a crowd it
// never draws. So people are taken separately, by anything that means to draw
// one.
//
// ONLY MATERIALISED PEOPLE ARE HERE, and that is the honest answer rather than
// a limitation to apologise for. The simulation thinks about most of the world
// in aggregate (SimLod, 01.03): a region at Statistic detail has a population
// and no persons, because no person was ever made. This view reports who
// EXISTS, not an estimate of who would exist if somebody looked. A renderer
// that finds no people in a region is being told the truth about that region.
//
// STATUS: PROTOTYPE (Phase 13) - tests in Tests/View/Test_Folk.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/ViewApi.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::View
{
	/// One person, as something drawing a figure needs them.
	///
	/// No pointer, no handle, no tick of birth: an AGE, because a renderer draws
	/// a child or an elder and would otherwise have to know what a tick is and
	/// how many go in a year. Parents are deliberately absent - 13.01 says the
	/// state a renderer NEEDS, and a family tree is Phase 14's business, not a
	/// precaution taken here in case somebody wants one.
	struct PersonView
	{
		uint32 Index = 0;	 ///< 1-based, stable for the life of the world
		uint32 Region = 0;	 ///< where they live, or died
		uint32 Family = 0;	 ///< family index, 0 = none
		uint32 Culture = 0;	 ///< never 0
		uint32 Religion = 0; ///< 0 = none
		uint32 Years = 0;	 ///< age at this frame's tick, whole years
		uint32 Spouse = 0;	 ///< person index, 0 = unmarried or widowed
		uint8 Sex = 0;		 ///< Population::Sex
		uint8 State = 0;	 ///< Population::LifeState
		uint8 Reserved[2] = {};
		Hash64 Identity = 0; ///< seed of their traits and their name
	};
	static_assert(sizeof(PersonView) == 7 * sizeof(uint32) + 4 + sizeof(Hash64),
				  "PersonView must have no padding: MeasurePeopleView hashes it");

	/// What PersonView::State holds for somebody alive.
	///
	/// The enum it comes from is Population::LifeState, and a renderer is not
	/// allowed to include the module that declares it - that prohibition is the
	/// point of this whole layer. Which left the one consumer this view was
	/// built for unable to ask the one question it has: is this person alive.
	/// It could count them (PeopleView::Living) and not identify them.
	///
	/// So the value is named here and checked against the enum in Folk.cpp,
	/// which is the one place allowed to see both. The same arrangement as
	/// BiomeKinds in Land.h, and found the same way: by writing the thing that
	/// was supposed to read the view and watching it come up short.
	inline constexpr uint8 AliveState = 0;

	/// Whether this person is alive at the frame the view was taken.
	///
	/// Not "not dead": Population::LifeState also has Gone, somebody who left
	/// the detailed grain of 04.06 and is kept for history. They are not dead
	/// and they are nowhere, so a renderer that draws everyone who is not dead
	/// draws people who are not there.
	VAELEN_VIEW_API bool IsAlive(const PersonView& P);

	/// The people of one frame, in index order - always, so a renderer can keep
	/// its own array in step with this one without sorting it first.
	struct PeopleView
	{
		uint64 Tick = 0;
		uint32 Year = 0;
		uint32 Living = 0; ///< of the people below, how many are alive
		std::vector<PersonView> People;
	};

	/// Takes the people. Const world in, numbers out - the same signature and
	/// the same promise as TakeView and TakeNetView. Out is left empty when no
	/// region is detailed, which is a world nobody is looking at closely and not
	/// an error.
	VAELEN_VIEW_API void TakePeopleView(const World& W, const ViewSources& From, PeopleView& Out);

	/// The person with this index, or nullptr. Binary search: the vector is
	/// ordered and this is the lookup that order exists for.
	VAELEN_VIEW_API const PersonView* PersonIn(const PeopleView& V, uint32 Index);

	/// The people of one region, appended to Out in index order.
	VAELEN_VIEW_API void PeopleOfRegion(const PeopleView& V, uint32 Region, std::vector<PersonView>& Out);

	struct PeopleStats
	{
		uint32 People = 0;	///< in the view
		uint32 Living = 0;	///< of them, alive
		uint32 Regions = 0; ///< distinct regions the people live in
		uint32 Oldest = 0;	///< years of the oldest living person
		uint32 Bytes = 0;	///< what the people weigh
		uint32 Reserved = 0;
		Hash64 Digest = 0; ///< every person in index order
	};
	VAELEN_VIEW_API PeopleStats MeasurePeopleView(const PeopleView& V);
} // namespace Vaelen::View
