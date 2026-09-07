// VAELEN - VaelenPolitics
// Phase 07.06: diplomacy - what two polities are to each other, and what that
// changes on the ground.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics
//
// Two polities know each other when their ground touches: contact is a fact of
// the region graph, not a decision. From there a relation warms and cools by
// things the world already has - a shared culture, roads carrying goods across
// the border, the length of that border, and the shadow a much larger
// neighbour casts - and the stance follows the warmth with hysteresis, so a
// pact is not lost to one bad year.
//
// A relation that is only a label changes nothing, so this one has teeth. At
// war, the weaker side's border regions are marked contested, and the reach
// system - which until now could only take unruled ground - may take a
// contested region at a war price. Everything else in the model still applies:
// the taken region is held from the seat like any other, costs upkeep like any
// other, and can be neglected into a faction like any other.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Politics/PoliticsApi.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
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
	/// What two polities are to each other. Warmer to colder.
	enum class Stance : uint32
	{
		Pact = 0,	 ///< sworn: a settled border
		Peace = 1,	 ///< nothing owed either way
		Rivalry = 2, ///< a grudge with no blows
		War = 3,	 ///< ground may change hands
	};
	VAELEN_POLITICS_API const char* StanceName(Stance S) noexcept;

	/// Component of a relation entity (ids of kind Treaty). Always A < B, so a
	/// pair has exactly one relation whichever side is asked.
	struct Relation
	{
		uint32 Index = 0;
		uint32 A = 0;		 ///< polity index, the lower
		uint32 B = 0;		 ///< polity index, the higher
		uint32 Stance_ = 0;	 ///< Stance
		uint32 Warmth = 0;	 ///< per mille, 0 = deadly, 1000 = sworn
		uint32 Border = 0;	 ///< adjacent region pairs, last counted
		uint64 Met = 0;		 ///< tick their ground first touched
		uint64 Turned = 0;	 ///< tick the stance last changed
		Hash64 Identity = 0; ///< from the world seed, for names
	};
	static_assert(sizeof(Relation) == 48, "Relation must stay padding free");

	struct DiplomacyTypes
	{
		ComponentType<Relation> Relation_;
		ComponentType<RegionInPlay> Contested; ///< declared by Reach, written here
		static VAELEN_POLITICS_API DiplomacyTypes Declare(World& W);
	};

	struct DiplomacyRules
	{
		uint32 WarmthAtContact = 500;  ///< per mille when two polities first touch
		uint32 WarmthSameCulture = 40; ///< gained a year while they share a culture
		uint32 WarmthPerRoute = 15;	   ///< gained a year per road across the border
		uint32 ChillPerBorder = 8;	   ///< lost a year per adjacent region pair
		uint32 ChillPerSizeStep = 20;  ///< lost a year per region the larger holds over the smaller
		uint32 WarmthCeiling = 1000;
		uint32 PactAt = 750;	///< warmth at or above which a pact is sworn
		uint32 PeaceAt = 450;	///< at or above which there is peace
		uint32 RivalryAt = 200; ///< at or above which it is only a grudge
		uint32 Hysteresis = 60; ///< a stance holds until the warmth passes its edge by this much
	};

	/// Two polities' ground touched for the first time (A, 0, B, warmth).
	inline constexpr EventType<PolityPayload> ContactMadeEvent = MakeEventType<PolityPayload>("ContactMade");
	/// A relation turned (A, 0, B, the new Stance).
	inline constexpr EventType<PolityPayload> StanceChangedEvent = MakeEventType<PolityPayload>("StanceChanged");
	/// A region was put in play (the polity that may take it, the region, its holder, 0).
	inline constexpr EventType<PolityPayload> RegionContestedEvent = MakeEventType<PolityPayload>("RegionContested");

	/// Yearly, after Factions: find who touches whom, warm and cool every
	/// relation, turn the stances, and mark what a war puts in play.
	class VAELEN_POLITICS_API DiplomacySystem final : public ISystem
	{
	public:
		DiplomacySystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::TradeTypes InTrade,
						PolityTypes InPolities, DiplomacyTypes InRelations, DiplomacyRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Trade(InTrade), Polities(InPolities), Relations(InRelations),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Diplomacy"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Factions"};
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
		Economy::TradeTypes Trade;
		PolityTypes Polities;
		DiplomacyTypes Relations;
		DiplomacyRules Rules;
		WorldGen::RegionGraph Graph;
		uint32 GraphRegions = 0;
	};

	/// The relation between two polities, either way round (nullptr when their
	/// ground has never touched).
	VAELEN_POLITICS_API const Relation* RelationBetween(const World& W, const DiplomacyTypes& Relations, uint32 A,
														uint32 B);
	/// Every relation a polity has, by index of the other side, in order.
	VAELEN_POLITICS_API void NeighboursOf(const World& W, const DiplomacyTypes& Relations, uint32 Polity,
										  std::vector<uint32>& Out);

	struct DiplomacyStats
	{
		uint32 Relations_ = 0; ///< pairs that have ever touched
		uint32 Pacts = 0;
		uint32 Peaces = 0;
		uint32 Rivalries = 0;
		uint32 Wars = 0;
		uint32 InPlay = 0;	 ///< regions standing contested now
		uint32 Contacts = 0; ///< events, from the log
		uint32 Turns = 0;
		uint32 Contests = 0;
		uint32 Bad = 0;	   ///< a relation with a polity that is gone, A not below B, a warmth or
						   ///< stance out of range, a contested region its holder does not hold
		Hash64 Digest = 0; ///< every relation in index order, then every contest in region order
	};
	VAELEN_POLITICS_API DiplomacyStats MeasureDiplomacy(const World& W, const History::PreHistoryTypes& Types,
														const PolityTypes& Polities, const DiplomacyTypes& Relations,
														const DiplomacyRules& Rules);
} // namespace Vaelen::Politics
