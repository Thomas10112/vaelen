// VAELEN - VaelenSociety
// Phase 05.07: society in history - the records that matter, one line for
// every society event, the why of a decision.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic tests in Tests/Society
//
// The society events that matter (an organisation founded or disbanded, a
// head seated, grain laid in, a raid planned, a custom changed, a person
// enslaved or freed) become the same Record entities as the Phase 03 and
// 04 chronicles, under a yearly cap per region; every society event gets a
// sentence, every other event goes through the person and history text;
// the unified chronicle and the why of a decision follow.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Decisions.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/SocietyApi.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Society
{
	struct SocietyChronicleRules
	{
		uint32 RecordFoundings = 1;	   ///< OrganizationFounded, OrganizationDisbanded
		uint32 RecordHeads = 1;		   ///< HeadSeated
		uint32 RecordGrain = 1;		   ///< DecisionMade of kind StoreGrain
		uint32 RecordRaids = 1;		   ///< RaidPlanned
		uint32 RecordSermons = 0;	   ///< DecisionMade of kind Preach
		uint32 RecordCustoms = 1;	   ///< NormChanged
		uint32 RecordEnslavements = 1; ///< BondEntered of kind Enslaved
		uint32 RecordManumissions = 1; ///< BondLeft by manumission
		uint32 MaxRecordsPerYear = 32; ///< per region
	};

	/// Singleton component: the listener's tallies.
	struct SocietyChronicleState
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Year = 0;
		uint32 Region = 0;
		uint32 InYear = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(SocietyChronicleState) == 24, "SocietyChronicleState must stay padding free");

	struct SocietyChronicleTypes
	{
		ComponentType<SocietyChronicleState> State;
		static SocietyChronicleTypes Declare(World& W);
	};

	/// Everything the society text needs to name things.
	struct SocietyContext
	{
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		OrganizationTypes Organizations;
	};

	/// Listener: the society events that matter become chronicle records.
	class VAELEN_SOCIETY_API SocietyChronicle final : public IEventListener
	{
	public:
		SocietyChronicle(World& InWorld, const History::PreHistoryTypes& InTypes, SocietyContext InContext,
						 SocietyChronicleTypes InState, SocietyChronicleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Context(InContext), State(InState), Rules(InRules)
		{
		}
		const char* GetListenerName() const noexcept override { return "SocietyChronicle"; }
		void OnEvent(const Event& E) override;
		void Attach();

	private:
		bool Matters(const Event& E, uint32& Region) const;
		World* Owner;
		History::PreHistoryTypes Types;
		SocietyContext Context;
		SocietyChronicleTypes State;
		SocietyChronicleRules Rules;
	};

	/// "the council of Edavaken", "the temple of Oldiss in Edavaken", "organisation 7".
	VAELEN_SOCIETY_API void NameOrganization(const World& W, const History::PreHistoryTypes& Types,
											 const SocietyContext& Context, uint32 Organization, std::string& Out);
	/// The name of a culture by index ("the Thuthanyo", or "culture 3").
	VAELEN_SOCIETY_API void NameCulture(const World& W, const History::PreHistoryTypes& Types, uint32 Culture,
										std::string& Out);
	/// One line for any event: society events get their own sentence, the rest
	/// go through the person and history text.
	VAELEN_SOCIETY_API void DescribeSocietyEvent(const World& W, const History::PreHistoryTypes& Types,
												 const SocietyContext& Context, const Event& E, std::string& Out,
												 const Population::PersonIndex* Index = nullptr);
	/// The whole chronicle as text, in tick order, one line each.
	VAELEN_SOCIETY_API uint32 ExportChronicleWithSociety(const World& W, const History::PreHistoryTypes& Types,
														 const SocietyContext& Context, std::string& Out,
														 uint32 MaxLines = 0);
	/// The why of an event id as text: the event, then "because ..." lines to the root.
	VAELEN_SOCIETY_API uint32 ExportWhyWithSociety(const World& W, const History::PreHistoryTypes& Types,
												   const SocietyContext& Context, PersistentId Id, std::string& Out);

	struct SocietyChronicleStats
	{
		uint32 Records = 0;
		uint32 Dropped = 0;
		uint32 Described = 0;
		uint32 WithRegion = 0;
		uint32 EraConsistent = 0;
		uint32 ByType[8] = {}; ///< founded, disbanded, head, grain, raid, custom, enslaved, freed
	};
	VAELEN_SOCIETY_API SocietyChronicleStats CheckSocietyChronicle(const World& W,
																   const History::PreHistoryTypes& Types,
																   const SocietyContext& Context,
																   const SocietyChronicleTypes& State);
} // namespace Vaelen::Society
