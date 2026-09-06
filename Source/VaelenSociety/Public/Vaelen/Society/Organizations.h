// VAELEN - VaelenSociety
// Phase 05.01: organisations - councils and temples founded from a region's
// persons, membership in detailed regions, member counts in coarse ones.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic/edge tests in Tests/Society
//
// An organisation is an entity of kind Organization with a seat region, a
// kind, a culture (and a faith for a temple), a head and a member count.
// In a detailed region the yearly system founds a council once the region
// is peopled enough (its seats go to the heads of the largest houses) and a
// temple once the majority faith has believers enough (its members are the
// most pious of that faith), keeps the seats filled as members die or leave,
// and disbands what has nobody left. In a coarse region the organisation
// keeps its last member count; a promotion fills it again from persons.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/SocietyApi.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Society
{
	enum class OrganizationKind : uint32
	{
		Council = 0,
		Temple,
		Guild,	 ///< Phase 05.05
		Warband, ///< Phase 05.05
		Clan,	 ///< Phase 05.05
		Count
	};
	VAELEN_SOCIETY_API const char* OrganizationKindName(OrganizationKind Kind) noexcept;

	/// Component of an organisation entity (ids of kind Organization).
	struct OrganizationInfo
	{
		uint32 Index = 0;	  ///< 1-based, in order of founding
		uint32 Kind = 0;	  ///< OrganizationKind
		uint32 Region = 0;	  ///< seat
		uint32 Culture = 0;	  ///< culture of the founders
		uint32 Religion = 0;  ///< faith served (temples), 0 otherwise
		uint32 Head = 0;	  ///< person index, 0 while the seat is coarse or the organisation empty
		uint32 Members = 0;	  ///< living members (last known when the seat is coarse)
		uint32 Seats = 0;	  ///< members wanted
		uint64 Founded = 0;	  ///< tick
		uint64 Disbanded = 0; ///< tick, 0 while alive
		Hash64 Identity = 0;  ///< from the world seed, for names and later traits
		uint32 Reserved[2] = {};
	};
	static_assert(sizeof(OrganizationInfo) == 64, "OrganizationInfo must stay padding free");

	/// Component on a person entity while it belongs to an organisation.
	struct Membership
	{
		uint32 Organization = 0; ///< organisation index
		uint32 Role = 0;		 ///< 0 member, 1 head
		uint64 Since = 0;		 ///< tick
	};
	static_assert(sizeof(Membership) == 16, "Membership must stay padding free");

	struct OrganizationTypes
	{
		ComponentType<OrganizationInfo> Organization;
		ComponentType<Membership> Member;
		static OrganizationTypes Declare(World& W);
	};

	struct OrganizationRules
	{
		uint32 CouncilFromPeople = 300;	  ///< a council once the region holds this many
		uint32 CouncilSeats = 7;		  ///< heads of the largest houses
		uint32 TempleFromBelievers = 200; ///< a temple once the majority faith holds this many
		uint32 TempleSeatsPerMille = 50;  ///< members among the believers, the most pious first
		uint32 TempleMaxSeats = 64;
		uint32 MemberFromAge = 20;
		uint32 DisbandAfterYears = 3; ///< empty this long in a detailed region: disbanded
	};

	struct OrganizationPayload
	{
		uint32 Organization = 0;
		uint32 Kind = 0;
		uint32 Region = 0;
		uint32 Person = 0; ///< the member (joined, left, head) or 0
	};
	inline constexpr EventType<OrganizationPayload> OrganizationFoundedEvent =
		MakeEventType<OrganizationPayload>("OrganizationFounded");
	inline constexpr EventType<OrganizationPayload> OrganizationDisbandedEvent =
		MakeEventType<OrganizationPayload>("OrganizationDisbanded");
	inline constexpr EventType<OrganizationPayload> MemberJoinedEvent =
		MakeEventType<OrganizationPayload>("MemberJoined");
	inline constexpr EventType<OrganizationPayload> MemberLeftEvent = MakeEventType<OrganizationPayload>("MemberLeft");
	inline constexpr EventType<OrganizationPayload> HeadSeatedEvent = MakeEventType<OrganizationPayload>("HeadSeated");

	/// Yearly, for every detailed region: foundings, seats filled, heads seated,
	/// the empty disbanded; coarse seats keep their last count.
	class VAELEN_SOCIETY_API OrganizationSystem final : public ISystem
	{
	public:
		OrganizationSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
						   Population::FamilyTypes InFamilies, Population::TraitTypes InTraits,
						   OrganizationTypes InOrganizations, OrganizationRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Traits(InTraits),
			  Organizations(InOrganizations), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Organizations"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Families"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after another yearly system too (Needs, Lod). The system must exist.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		Population::TraitTypes Traits;
		OrganizationTypes Organizations;
		OrganizationRules Rules;
	};

	/// Organisations seated in a region (index order), alive or disbanded.
	VAELEN_SOCIETY_API void OrganizationsOf(const World& W, const OrganizationTypes& Types, uint32 Region,
											std::vector<OrganizationInfo>& Out);
	/// Living members of an organisation, by person index, ascending.
	VAELEN_SOCIETY_API void MembersOf(const World& W, const Population::PersonTypes& Persons,
									  const OrganizationTypes& Types, uint32 Organization, std::vector<uint32>& Out);
	/// The organisation a person belongs to (0 for none).
	VAELEN_SOCIETY_API uint32 OrganizationOf(const World& W, const Population::PersonTypes& Persons,
											 const OrganizationTypes& Types, uint32 Person);

	struct OrganizationStats
	{
		uint32 Total = 0;
		uint32 Alive = 0;
		uint32 PerKind[static_cast<uint32>(OrganizationKind::Count)] = {};
		uint32 Members = 0;		  ///< memberships on living persons
		uint32 Astray = 0;		  ///< memberships pointing at a disbanded or missing organisation, or a dead member
		uint32 HeadsAlive = 0;	  ///< living organisations whose head is a living member
		uint32 CountMismatch = 0; ///< living organisations of detailed regions whose Members differs from the persons
		Hash64 Digest = 0;		  ///< every organisation in index order
	};
	VAELEN_SOCIETY_API OrganizationStats MeasureOrganizations(const World& W, const History::PreHistoryTypes& Types,
															  const Population::PersonTypes& Persons,
															  const OrganizationTypes& Types_);
} // namespace Vaelen::Society
