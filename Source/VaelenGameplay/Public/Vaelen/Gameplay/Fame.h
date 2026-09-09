// VAELEN - VaelenGameplay
// Phase 12 task 12.05: a name that travels.
//
// 12.02 gave people opinions of each other and a way to tell them, and named
// its own limit: `MostThoughtOf` is eight, so a reputation is held by eight
// people at once and the oldest is forgotten. That is a village's reputation.
// This task is where the bound is faced rather than raised, and the answer is
// that it was the wrong shape rather than the wrong number.
//
// A name that has travelled is not held by individuals. Nobody in the next
// valley has an opinion of a man they will never meet; the PLACE has one - "in
// Kratfa they say he is a thief" - and it is one thing about one name, not
// eight hundred private judgements. So fame lives on the region, a handful of
// names to a region, and it moves the way everything else moves in this world:
// along the routes of 06.04 and the roads of 09.04, weaker at every hop, and
// fading when nothing renews it.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic tests in Tests/Gameplay/Test_Fame.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Gameplay
{
	/// How many names one place carries. A region knows a few names, not
	/// everybody's - which is what a name being known MEANS.
	inline constexpr usize MostNames = 8;

	/// What a place says about somebody who is not there.
	struct Fame
	{
		uint64 Since = 0;	 ///< tick this place first heard it, kept while it keeps saying it
		uint32 Person = 0;	 ///< who is spoken of
		int32 Said = 0;		 ///< what is said of them here, on 12.02's scale
		uint32 Hops = 0;	 ///< roads between here and where it started
		uint32 Reserved = 0; //
	};
	static_assert(sizeof(Fame) == 24, "Fame must stay padding free");

	/// Component on a region entity: the names this place carries.
	struct RegionNames
	{
		uint32 Count = 0;
		uint32 Reserved = 0;
		Fame Who[MostNames];
	};
	static_assert(sizeof(RegionNames) == 200, "RegionNames must stay padding free");

	struct FameTypes
	{
		ComponentType<RegionNames> Names;

		static VAELEN_GAMEPLAY_API FameTypes Declare(World& W);
	};

	struct FameRules
	{
		/// What one thing done for somebody is worth to the doer's name, and one
		/// thing taken from them.
		///
		/// This is the answer to the limit 12.02 named, and it is not a bigger
		/// number. A name is built on what a person has DONE, counted over their
		/// whole life and never evicted - not on 12.02's Repute, which is the
		/// average of what the eight people who last dealt with them happen to
		/// think. Measured, that average puts 1163 of a region's 1428 people
		/// between 80 and 93 on a scale to a thousand, and the eight names it
		/// picks out are COMPLETELY DIFFERENT every year. A world built on it has
		/// no famous people, only a yearly lottery. The deeds do persist: the
		/// same two people led the region in four consecutive years.
		int32 PerDeed = 4;
		int32 PerWrong = -25;
		int32 Most = 1000; ///< a life's worth of it, past which nothing counts

		/// A floor rather than a selector: somebody who has done next to nothing
		/// has no name at all. Which of the rest a place carries is decided by
		/// MostNames - a place says the loudest handful it has heard.
		int32 WorthCarrying = 100;
		/// What survives one road, per mille. A name is thinner in the next
		/// valley than it is at home, and thinner again beyond that.
		uint32 PerHopPerMille = 600;
		/// How far a name goes at all. Beyond this nobody has heard of anybody.
		uint32 MostHops = 3;
	};

	/// A name reaching a place that had not heard it. Its own payload rather
	/// than 12.01's, because what is said of a man can be bad and ActPayload's
	/// Amount is unsigned - a thief's name would have arrived as a hero's.
	struct FamePayload
	{
		uint32 Person = 0;
		uint32 Region = 0;
		int32 Said = 0;
		uint32 Hops = 0;
	};
	static_assert(sizeof(FamePayload) == 16, "FamePayload must stay padding free");

	inline constexpr EventType<FamePayload> NameTravelledEvent = MakeEventType<FamePayload>("NameTravelled");
	/// A name a place has stopped saying.
	inline constexpr EventType<FamePayload> NameForgottenEvent = MakeEventType<FamePayload>("NameForgotten");

	/// Yearly, after Trade: what people say of somebody where they live becomes
	/// what the place says, and what the place says travels to the places it
	/// trades with. A name a place is no longer told, it stops saying.
	///
	/// There is no gradual fading here and the omission is deliberate and
	/// measured. A per-year decay was written, and in every case that could be
	/// reached it did nothing: while the people who earned a name are alive it
	/// is re-told across every road every year at a value that only grows, so
	/// no place is ever a year out of date. Shutting all sixty-eight roads by
	/// hand left MORE names abroad two years later, because 06.04 re-opens a
	/// route the year the prices warrant it; killing every person in the source
	/// region did not do it either, because the level-of-detail bridge simply
	/// materialises the region's people again. With the decay in, three
	/// settings of it (0, 120, 400 a year) gave byte-identical loudness and
	/// byte-identical name lists, differing only in a timestamp. A rule that
	/// changes nothing is not shipped: see ADR-0100.
	///
	/// It moves on the ROUTES of 06.04 and not on adjacency, which is the whole
	/// of why it is worth doing here rather than in 12.02: a place with no road
	/// to you has never heard of you, however close it stands.
	class VAELEN_GAMEPLAY_API FameSystem final : public ISystem
	{
	public:
		FameSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
				   ReputeTypes InRepute, Economy::TradeTypes InTrade, FameTypes InFame, FameRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Repute(InRepute), Trade(InTrade), Fame_(InFame),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Fame"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out;
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
		Population::PersonTypes Persons;
		ReputeTypes Repute;
		Economy::TradeTypes Trade;
		FameTypes Fame_;
		FameRules Rules;
	};

	/// What is said of somebody in a place (nullptr when the place has never
	/// heard of them).
	VAELEN_GAMEPLAY_API const Fame* FameIn(const World& W, const History::PreHistoryTypes& Types,
										   const FameTypes& Types_, uint32 Region, uint32 Person);

	struct FameStats
	{
		uint32 Places = 0;	 ///< regions carrying any name at all
		uint32 Names = 0;	 ///< names carried, counting a name once per place
		uint32 Abroad = 0;	 ///< of them, ones that travelled at least one road
		uint32 Furthest = 0; ///< the most roads any name has crossed
		uint64 Loudness = 0; ///< what is said, added up, however good or bad
		Hash64 Digest = 0;
	};
	VAELEN_GAMEPLAY_API FameStats MeasureFame(const World& W, const FameTypes& Types);
} // namespace Vaelen::Gameplay
