// VAELEN - VaelenPolitics
// Phase 07.05: factions - who inside a polity wants it to be otherwise, how
// strong they get, and what they take when nobody answers them.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics
//
// A faction cannot change who sits: the council seats its head (07.01) and
// succession only judges the passing (07.04). What a faction can do is take
// the ground away. It forms around a grievance with a place and, where there
// is one, a person:
//
//   - a claimant the custom named and the council passed over, in the region
//     they live in;
//   - a region held so loosely, for so long, that its people stop counting
//     themselves as ruled.
//
// While a faction stands it adds to the polity's unrest, which 07.03 takes off
// the hold of every region - so a faction in one province weakens the whole.
// It gathers strength while its grievance stands and loses it when the
// grievance is answered, and at its threshold the region leaves the polity.
// Then the faction is done: it wanted that, and it has it.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Politics/PoliticsApi.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Politics/Succession.h"
#include "Vaelen/Population/Persons.h"
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

namespace Vaelen::Politics
{
	/// Why a faction exists.
	enum class Grievance : uint32
	{
		PassedOver = 0, ///< the custom named a claimant and the seat went elsewhere
		Neglect = 1,	///< the region has been held too loosely for too long
	};
	VAELEN_POLITICS_API const char* GrievanceName(Grievance G) noexcept;

	/// Component of a faction entity (ids of kind Faction).
	struct FactionInfo
	{
		uint32 Index = 0;	 ///< 1-based, in order of forming
		uint32 Polity = 0;	 ///< the polity it is inside
		uint32 Region = 0;	 ///< where its strength sits
		uint32 Claimant = 0; ///< the person it wants on the seat, 0 = it wants no one
		uint32 Strength = 0; ///< per mille; at the threshold it takes the region
		uint32 Cause = 0;	 ///< Grievance
		uint64 Formed = 0;	 ///< tick
		uint64 Ended = 0;	 ///< tick, 0 while it stands
		Hash64 Identity = 0; ///< from the world seed, for names
	};
	static_assert(sizeof(FactionInfo) == 48, "FactionInfo must stay padding free");

	/// Component on a region entity: how many years running it has been held
	/// too loosely, and by whom. The counter of a grievance nobody has voiced yet.
	struct RegionPatience
	{
		uint32 Polity = 0; ///< whose neglect is being counted
		uint32 Years = 0;  ///< consecutive years under the threshold
	};
	static_assert(sizeof(RegionPatience) == 8, "RegionPatience must stay padding free");

	struct FactionTypes
	{
		ComponentType<FactionInfo> Faction;
		ComponentType<RegionPatience> Patience;
		static VAELEN_POLITICS_API FactionTypes Declare(World& W);
	};

	struct FactionRules
	{
		uint32 NeglectUnderHold = 500;		  ///< per mille; a region held under this is neglected
		uint32 NeglectYears = 3;			  ///< years running before its people say so
		uint32 StrengthAtBirth = 100;		  ///< per mille
		uint32 StrengthPerYearAggrieved = 60; ///< gained while the grievance stands
		uint32 StrengthLostPerYear = 80;	  ///< lost once it is answered
		uint32 RevoltAt = 500;				  ///< per mille; at this the region leaves
		uint32 UnrestPerFaction = 40;		  ///< per mille added to the polity for each faction standing
		uint32 UnrestCeiling = 600;			  ///< never past this - the same ceiling succession keeps
		uint32 StrengthCeiling = 1000;
	};

	/// A faction formed (Polity, Region, the claimant or 0, the grievance).
	inline constexpr EventType<PolityPayload> FactionFormedEvent = MakeEventType<PolityPayload>("FactionFormed");
	/// A faction took its region out of the polity (Polity, Region, claimant, strength).
	inline constexpr EventType<PolityPayload> FactionRevoltedEvent = MakeEventType<PolityPayload>("FactionRevolted");
	/// A faction came to nothing (Polity, Region, claimant, the strength it had left).
	inline constexpr EventType<PolityPayload> FactionFadedEvent = MakeEventType<PolityPayload>("FactionFaded");

	/// Yearly, after Reach: judge the standing factions on this year's holds,
	/// let the strong ones take their region, then form what this year has
	/// given cause for. Runs last of the politics systems, so the unrest it
	/// adds is felt the year after - a faction is not news the day it forms.
	class VAELEN_POLITICS_API FactionSystem final : public ISystem
	{
	public:
		FactionSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					  PolityTypes InPolities, ReachTypes InReaches, SuccessionTypes InLines, FactionTypes InFactions,
					  FactionRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Polities(InPolities), Reaches(InReaches),
			  Lines(InLines), Factions(InFactions), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Factions"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Reach"};
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
		PolityTypes Polities;
		ReachTypes Reaches;
		SuccessionTypes Lines;
		FactionTypes Factions;
		FactionRules Rules;
	};

	/// A faction by index (nullptr when unknown), standing or ended.
	VAELEN_POLITICS_API const FactionInfo* FactionOf(const World& W, const FactionTypes& Factions, uint32 Faction);
	/// The factions standing inside a polity, in index order.
	VAELEN_POLITICS_API void FactionsOf(const World& W, const FactionTypes& Factions, uint32 Polity,
										std::vector<uint32>& Out);

	struct FactionStats
	{
		uint32 Standing = 0; ///< factions not ended
		uint32 Ended = 0;
		uint32 Strongest = 0; ///< per mille
		uint32 Aggrieved = 0; ///< regions counting years of neglect
		uint32 Formed = 0;	  ///< events, from the log
		uint32 Revolts = 0;
		uint32 Faded = 0;
		uint32 Bad = 0;	   ///< a faction of a polity that is gone, in a region it does not rule,
						   ///< a strength past its ceiling, a claimant who is not alive
		Hash64 Digest = 0; ///< every faction in index order, then every patience in region order
	};
	VAELEN_POLITICS_API FactionStats MeasureFactions(const World& W, const History::PreHistoryTypes& Types,
													 const Population::PersonTypes& Persons,
													 const PolityTypes& Polities, const FactionTypes& Factions,
													 const FactionRules& Rules);
} // namespace Vaelen::Politics
