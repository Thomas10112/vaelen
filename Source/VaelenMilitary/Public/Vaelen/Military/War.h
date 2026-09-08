// VAELEN - VaelenMilitary
// Phase 08.05: war as a thing with a beginning and an end.
//
// STATUS: PROTOTYPE (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military
//
// Until now a war was a stance: 07.06 cooled a relation past a threshold and
// everything downstream read "at war" off it. That is enough to start a war and
// no use at all for ending one, because the thing that would end it - two
// exhausted powers agreeing to stop - has nowhere to live. A stance has no
// memory of what it has cost.
//
// So the war becomes the thing, and the stance follows it. A relation that
// cools into war opens a war; while the war is open the stance stays at war
// however the warmth drifts, because a war is not called off by a good harvest;
// and when the war ends the stance is written back to peace, at a warmth that
// will hold for a while and then not.
//
// What ends a war is exhaustion. Every year a war runs costs both sides a
// little, every hundred men lost costs more, and a capital lost costs a great
// deal. A side worn past forfeiting takes any terms; two sides worn past
// willing make a white peace. Terms are what has already happened: whoever
// holds ground at the end keeps it, and what a war put in play (07.06) stops
// being in play.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/Battle.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Military/Siege.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Polities.h"
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
	/// Component of a war entity (ids of kind War). It outlives the fighting:
	/// a war that has ended is a thing the world remembers.
	struct WarInfo
	{
		uint32 Index = 0;	 ///< 1-based, in order of breaking out
		uint32 A = 0;		 ///< the lower polity index
		uint32 B = 0;		 ///< the higher
		uint32 Years = 0;	 ///< years it has run
		uint32 FallenA = 0;	 ///< men A has lost in it
		uint32 FallenB = 0;	 ///< men B has lost in it
		uint32 WearA = 0;	 ///< per mille; at the forfeiting mark A takes any terms
		uint32 WearB = 0;	 ///< per mille
		uint32 Winner = 0;	 ///< polity; 0 while it runs, and 0 for a white peace
		uint32 Seats = 0;	 ///< capitals that changed hands in it
		uint64 Began = 0;	 ///< tick
		uint64 Ended = 0;	 ///< tick, 0 while it runs
		Hash64 Identity = 0; ///< from the world seed, for names
	};
	static_assert(sizeof(WarInfo) == 64, "WarInfo must stay padding free");

	struct WarTypes
	{
		ComponentType<WarInfo> War;
		static VAELEN_MILITARY_API WarTypes Declare(World& W);
	};

	struct WarRules
	{
		uint32 WearPerHundredFallen = 60; ///< per mille a hundred men lost cost the side that lost them
		uint32 WearPerYear = 20;		  ///< per mille both sides pay for the war simply going on
		uint32 WearPerSeatLost = 400;	  ///< per mille a capital lost costs
		uint32 ForfeitAt = 700;			  ///< a side this worn will take any terms
		uint32 WillingAt = 450;			  ///< two sides this worn make a white peace
		uint32 LeastYears = 3;			  ///< a war does not end in the year it began
		uint32 PeaceWarmth = 450;		  ///< warmth a peace is written at
	};

	/// A war broke out (A, 0, the war, B).
	inline constexpr EventType<Politics::PolityPayload> WarBeganEvent =
		MakeEventType<Politics::PolityPayload>("WarBegan");
	/// A war ended (the winner or 0 for a white peace, 0, the war, the years it ran).
	inline constexpr EventType<Politics::PolityPayload> WarEndedEvent =
		MakeEventType<Politics::PolityPayload>("WarEnded");

	/// Yearly, after Sieges: open a war for every relation that has turned to
	/// one, hold the stance at war while it runs, wear both sides down with
	/// what it costs them, and write the peace when one of them has had enough.
	class VAELEN_MILITARY_API WarSystem final : public ISystem
	{
	public:
		WarSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Politics::PolityTypes InPolities,
				  Politics::DiplomacyTypes InRelations, BattleTypes InBattles, WarTypes InWars,
				  WarRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Polities(InPolities), Relations(InRelations), Battles(InBattles),
			  Wars(InWars), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Wars"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Sieges"};
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
		Politics::PolityTypes Polities;
		Politics::DiplomacyTypes Relations;
		BattleTypes Battles;
		WarTypes Wars;
		WarRules Rules;
	};

	/// A war by index (nullptr when unknown), running or over.
	VAELEN_MILITARY_API const WarInfo* WarOf(const World& W, const WarTypes& Wars, uint32 War);
	/// The war two polities are fighting, if they are (nullptr otherwise).
	VAELEN_MILITARY_API const WarInfo* WarBetween(const World& W, const WarTypes& Wars, uint32 A, uint32 B);

	struct WarStats
	{
		uint32 Running = 0; ///< wars not yet ended
		uint32 Over = 0;	///< wars that have
		uint32 Decided = 0; ///< of those, ones somebody won
		uint32 White = 0;	///< of those, ones nobody did
		uint64 Fallen = 0;	///< men lost, both sides, over every war
		uint32 LongestYears = 0;
		uint32 Began = 0; ///< events, from the log
		uint32 Ended = 0;
		uint32 Bad = 0;	   ///< a polity at war with itself, a war ended before it began, a winner
						   ///< that fought on neither side, or a running war with an end written
		Hash64 Digest = 0; ///< every war in index order
	};
	VAELEN_MILITARY_API WarStats MeasureWars(const World& W, const WarTypes& Wars, const WarRules& Rules);
} // namespace Vaelen::Military
