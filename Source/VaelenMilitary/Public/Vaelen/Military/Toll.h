// VAELEN - VaelenMilitary
// Phase 08.06: what war costs the living.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military
//
// Everything the phase has built so far moves numbers. A levy is men taken out
// of regions and a battle is men taken out of a levy, and at no point does
// anybody actually die: the count of men away comes back down and the people of
// the region are exactly as many as they were. That is the difference between a
// wargame and a world.
//
// So this is where the numbers land on people. Men who fell are dead where
// they came from - real persons struck out where a region is simulated person
// by person (04.06), and people taken off the count where it is not. Men who
// came home carry it: 05.02 grants standing for what other people know about
// you, and a man who marched and came back is granted something a man who
// stayed is not. And people leave ground an army will not get off, which is how
// a province is emptied without a single battle being fought on it.
//
// Nothing here decides anything. It reads what the war did - the levies
// released, the ground foraged - and writes what it cost.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/March.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Military/War.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/Standing.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Military
{
	/// Component on a region: what the wars have cost the people who live
	/// there, over the whole life of the world. It is a record, not a state:
	/// nothing reads it back to decide anything.
	struct RegionToll
	{
		uint32 Fallen = 0; ///< men of this region who did not come back
		uint32 Home = 0;   ///< men who did
		uint32 Fled = 0;   ///< people who left rather than live under a host
		uint32 Years = 0;  ///< years a war cost this region anything at all
	};
	static_assert(sizeof(RegionToll) == 16, "RegionToll must stay padding free");

	struct TollTypes
	{
		ComponentType<RegionToll> Toll;
		static VAELEN_MILITARY_API TollTypes Declare(World& W);
	};

	struct TollRules
	{
		uint32 FleeAfterYears = 2; ///< years a host must sit on a region before anybody leaves it
		uint32 FleePerMille = 30;  ///< of the region's people, each year after that
		uint32 MinToFlee = 40;	   ///< a region with fewer people than this loses nobody
		uint32 AdultFrom = 16;	   ///< a person younger than this is not a man under arms
	};

	/// Men of a region did not come back (the polity, the region, 0, the dead).
	inline constexpr EventType<Politics::PolityPayload> WarDeadEvent =
		MakeEventType<Politics::PolityPayload>("WarDead");
	/// People left ground an army would not get off (0, the region left, the region gone to, people).
	inline constexpr EventType<Politics::PolityPayload> PeopleFledEvent =
		MakeEventType<Politics::PolityPayload>("PeopleFled");

	/// Yearly, after Wars: read what the year's war did and write what it cost
	/// the living - the dead where they came from, the standing of those who
	/// came back, and the people who would not stay under a host.
	class VAELEN_MILITARY_API TollSystem final : public ISystem
	{
	public:
		TollSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
				   Society::StandingTypes InStanding, Politics::PolityTypes InPolities, MarchTypes InMarches,
				   TollTypes InToll, TollRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Standing(InStanding), Polities(InPolities),
			  Marches(InMarches), Toll(InToll), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Toll"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Wars"};
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
		Society::StandingTypes Standing;
		Politics::PolityTypes Polities;
		MarchTypes Marches;
		TollTypes Toll;
		TollRules Rules;
		WorldGen::RegionGraphCache Roads;
	};

	/// What the wars have cost a region (nullptr when they have cost it nothing).
	VAELEN_MILITARY_API const RegionToll* TollOf(const World& W, const History::PreHistoryTypes& Types,
												 const TollTypes& Toll, uint32 Region);

	struct TollStats
	{
		uint32 Regions = 0; ///< regions a war has cost something
		uint64 Fallen = 0;	///< men who did not come back, over every region
		uint64 Home = 0;	///< men who did
		uint64 Fled = 0;	///< people who left
		uint32 Served = 0;	///< living people carrying a war on their standing
		uint32 Deaths = 0;	///< events, from the log
		uint32 Flights = 0;
		uint32 Bad = 0;	   ///< a toll on a region that is not there, or one that cost nothing in no years
		Hash64 Digest = 0; ///< every toll in region order
	};
	VAELEN_MILITARY_API TollStats MeasureToll(const World& W, const History::PreHistoryTypes& Types,
											  const Society::StandingTypes& Standing, const TollTypes& Toll,
											  const TollRules& Rules);
} // namespace Vaelen::Military
