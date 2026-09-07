// VAELEN - VaelenPolitics
// Phase 07.01: polities - who rules where, seated in a council, holding
// regions that remember whose they are.
//
// STATUS: VALIDATED (Phase 07) - unit/deterministic/edge tests in Tests/Politics
//
// A polity is an entity of kind Polity with a seat region, a culture, a
// ruler and the council it rules through. It is founded where a detailed
// region holds a council of enough people and belongs to nobody, and its
// ruler is that council's head: no new kind of person, no parallel hierarchy
// - authority is exercised through the organisations of Phase 05. Belonging
// is a component on the region, not a list on the polity, so a region knows
// whose it is whether it is simulated person by person or kept as counts, and
// a demotion never loses it. A polity whose seat loses its council, or whose
// last region is gone, is dissolved and stays in the world as history.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Politics/PoliticsApi.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/Organizations.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Politics
{
	/// Component of a polity entity (ids of kind Polity).
	struct PolityInfo
	{
		uint32 Index = 0;	  ///< 1-based, in order of founding
		uint32 Seat = 0;	  ///< region the polity rules from
		uint32 Culture = 0;	  ///< culture of its founders
		uint32 Ruler = 0;	  ///< person index, 0 while no head sits
		uint32 Council = 0;	  ///< organisation index it rules through
		uint32 Regions = 0;	  ///< regions belonging to it (last counted)
		uint64 Founded = 0;	  ///< tick
		uint64 Dissolved = 0; ///< tick, 0 while it stands
		Hash64 Identity = 0;  ///< from the world seed, for names
	};
	static_assert(sizeof(PolityInfo) == 48, "PolityInfo must stay padding free");

	/// Component on a region entity: whose it is. Written by the polity system,
	/// kept through every change of detail, never removed while the polity stands.
	struct RegionRule
	{
		uint32 Polity = 0; ///< polity index, 0 = ruled by nobody
		uint32 Reserved = 0;
		uint64 Since = 0; ///< tick the region came under this rule
	};
	static_assert(sizeof(RegionRule) == 16, "RegionRule must stay padding free");

	struct PolityTypes
	{
		ComponentType<PolityInfo> Polity;
		ComponentType<RegionRule> Rule;
		static PolityTypes Declare(World& W);
	};

	struct PolityRules
	{
		uint32 FoundFromPeople = 400;  ///< a seat must hold this many living
		uint32 FoundFromSeats = 4;	   ///< and its council this many members
		uint32 DissolveAfterYears = 3; ///< a polity without a council or a region this long ends
		uint32 RulerFromAge = 20;	   ///< a ruler is of age
	};

	struct PolityPayload
	{
		uint32 Polity = 0;
		uint32 Region = 0;
		uint32 Person = 0; ///< the ruler, where one is meant
		uint32 Value = 0;  ///< regions held, or the council
	};
	inline constexpr EventType<PolityPayload> PolityFoundedEvent = MakeEventType<PolityPayload>("PolityFounded");
	inline constexpr EventType<PolityPayload> PolityDissolvedEvent = MakeEventType<PolityPayload>("PolityDissolved");
	inline constexpr EventType<PolityPayload> RulerSeatedEvent = MakeEventType<PolityPayload>("RulerSeated");
	inline constexpr EventType<PolityPayload> RegionClaimedEvent = MakeEventType<PolityPayload>("RegionClaimed");
	inline constexpr EventType<PolityPayload> RegionLostEvent = MakeEventType<PolityPayload>("RegionLost");

	/// Yearly, after Organizations: found what the councils warrant, seat the
	/// rulers, count the regions, dissolve what no longer rules anything.
	class VAELEN_POLITICS_API PolitySystem final : public ISystem
	{
	public:
		PolitySystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 Society::OrganizationTypes InOrganizations, PolityTypes InPolities, PolityRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Organizations(InOrganizations), Polities(InPolities),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Polities"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Organizations"};
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
		Society::OrganizationTypes Organizations;
		PolityTypes Polities;
		PolityRules Rules;
	};

	/// The polity of a given index (nullptr when unknown), standing or dissolved.
	VAELEN_POLITICS_API const PolityInfo* PolityOf(const World& W, const PolityTypes& Types, uint32 Polity);
	/// Whose a region is (nullptr when it never came under any rule).
	VAELEN_POLITICS_API const RegionRule* RuleOf(const World& W, const History::PreHistoryTypes& Types,
												 const PolityTypes& Polities, uint32 Region);
	/// The regions a standing polity holds, in index order.
	VAELEN_POLITICS_API void RegionsOf(const World& W, const History::PreHistoryTypes& Types,
									   const PolityTypes& Polities, uint32 Polity, std::vector<uint32>& Out);

	struct PolityStats
	{
		uint32 Standing = 0; ///< polities not dissolved
		uint32 Dissolved = 0;
		uint32 Ruled = 0;	 ///< regions belonging to a standing polity
		uint32 Unruled = 0;	 ///< regions carrying a rule of a dissolved polity
		uint32 Headless = 0; ///< standing polities without a living ruler
		uint32 Bad = 0;		 ///< a seat outside its polity, a ruler who is not the council's head, a double claim
		uint32 Founded = 0;	 ///< events, from the log
		uint32 Ended = 0;
		uint32 Seatings = 0;
		Hash64 Digest = 0; ///< every polity in index order, then every rule in region order
	};
	VAELEN_POLITICS_API PolityStats MeasurePolities(const World& W, const History::PreHistoryTypes& Types,
													const Population::PersonTypes& Persons,
													const Society::OrganizationTypes& Organizations,
													const PolityTypes& Polities);
} // namespace Vaelen::Politics
