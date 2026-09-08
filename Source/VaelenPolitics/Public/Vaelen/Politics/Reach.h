// VAELEN - VaelenPolitics
// Phase 07.03: authority and reach - how far a polity's word carries, what it
// costs to carry it there, and what slips free when it is not paid.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics
//
// Authority is not a number a polity owns; it is a number written on each of
// its regions. It falls with every hop of the region graph away from the seat,
// so a distant region is always held less firmly than a near one, and a region
// the polity can no longer walk to from its own seat is held not at all.
//
// Carrying a word costs grain: every region owes an upkeep that grows with its
// distance, paid out of the treasury 07.02 fills. A polity that cannot pay
// loses its grip on the far edge first, and a region whose hold falls under
// the floor slips free - the polity does not decide to let it go.
//
// Reach is what the treasury buys: the number of hops a polity can take a new
// region at. Claiming costs grain, and a polity claims in region order so that
// two worlds of one seed always take the same ground in the same year.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/PoliticsApi.h"
#include "Vaelen/Politics/Succession.h"
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
	/// Component on a region entity: whose authority runs here, from how far,
	/// and how firmly. Written every year for every ruled region.
	struct RegionAuthority
	{
		uint32 Polity = 0;	 ///< whose authority this measures, 0 = nobody's
		uint32 Distance = 0; ///< hops from that polity's seat (0 at the seat itself)
		uint32 Hold = 0;	 ///< per mille; under the floor the region slips free
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RegionAuthority) == 16, "RegionAuthority must stay padding free");

	/// Component on a polity entity: how far its word carries, and what it cost.
	struct PolityReach
	{
		uint32 Polity = 0;
		uint32 Reach = 0;	///< hops it can take a new region at, from what it holds
		uint32 Claimed = 0; ///< regions taken since the founding
		uint32 Slipped = 0; ///< regions lost for want of hold
		uint64 Spent = 0;	///< grain spent carrying its word
		uint64 Unpaid = 0;	///< upkeep it could not pay, last year
	};
	static_assert(sizeof(PolityReach) == 32, "PolityReach must stay padding free");

	struct ReachTypes
	{
		ComponentType<RegionAuthority> Authority;
		ComponentType<PolityReach> Reach;
		static VAELEN_POLITICS_API ReachTypes Declare(World& W);
	};

	/// How well a region is served by made roads, on the region entity: written
	/// by a later module (Phase 09 infrastructure), read by whoever is told to
	/// observe it. A road does not extend a polity and does not raise an army:
	/// it means the word a polity already sends carries further and costs less
	/// along that ground, and a host that already marches gets further on it and
	/// eats less beside it. One number, two readers, and a factor of one where
	/// nothing has been built.
	struct RegionWays
	{
		uint32 EasePerMille = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RegionWays) == 8, "RegionWays must stay padding free");

	struct ReachRules
	{
		uint32 HoldAtSeat = 1000;	 ///< per mille, at the seat itself
		uint32 HoldLostPerHop = 250; ///< per mille lost for every hop away from it
		uint32 HoldFloor = 250;		 ///< under this a region slips free
		uint32 UpkeepPerHop = 6;	 ///< grain a year, per region, per hop
		uint32 ClaimCost = 120;		 ///< grain to take a region
		uint32 ReachPerGrain = 400;	 ///< a hop of reach per this much grain held
		uint32 ReachCeiling = 4;	 ///< a word never carries further than this
		uint32 UnpaidHoldLoss = 200; ///< per mille lost where the upkeep went unpaid
		uint32 AnnexCost = 600;		 ///< grain to take a region another polity holds
	};

	/// A polity took a region (Polity, Region, 0, hops from the seat).
	inline constexpr EventType<PolityPayload> RegionTakenEvent = MakeEventType<PolityPayload>("RegionTaken");
	/// A region slipped free for want of hold (Polity, Region, 0, the hold it had).
	inline constexpr EventType<PolityPayload> RegionSlippedEvent = MakeEventType<PolityPayload>("RegionSlipped");
	/// A polity could not pay for carrying its word (Polity, 0, 0, grain short).
	inline constexpr EventType<PolityPayload> UpkeepUnpaidEvent = MakeEventType<PolityPayload>("UpkeepUnpaid");
	/// A polity took a region another one held (Polity, Region, the one it was taken from, hops).
	inline constexpr EventType<PolityPayload> RegionAnnexedEvent = MakeEventType<PolityPayload>("RegionAnnexed");

	/// Written by a higher layer (diplomacy, 07.06) and read where it is
	/// observed: a region another polity may take from the one holding it. The
	/// reach system never learns what a war is; it only learns that this ground
	/// is takeable and dearer than empty ground.
	struct RegionInPlay
	{
		uint32 By = 0; ///< the polity that may take it, 0 = nobody
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RegionInPlay) == 8, "RegionInPlay must stay padding free");

	/// Yearly, after Law: measure the hold of every ruled region from its
	/// distance to the seat, pay for it, let go what cannot be held, then take
	/// what the treasury and the reach allow.
	class VAELEN_POLITICS_API ReachSystem final : public ISystem
	{
	public:
		ReachSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::EconomyTypes InEconomy,
					PolityTypes InPolities, LawTypes InLaws, ReachTypes InReaches, ReachRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Economy(InEconomy), Polities(InPolities), Laws(InLaws),
			  Reaches(InReaches), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Reach"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Law"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: the unrest a polity carries after a vacancy or a succession
		/// the custom did not name (07.04) comes off the hold of every region it
		/// rules, for as long as it lasts.
		void ObserveLine(ComponentType<PolityLine> InLine) noexcept
		{
			Line = InLine;
			HasLine = true;
		}
		/// Optional: ground another polity has been put in a position to take
		/// (07.06). Without this the system only ever takes what nobody rules.
		void ObserveContest(ComponentType<RegionInPlay> InPlay) noexcept
		{
			InPlay_ = InPlay;
			HasPlay = true;
		}
		/// Optional: the roads made on the ground (09.06). A word carries further
		/// and costs less along made ground; without this every region is bare.
		void ObserveWays(ComponentType<RegionWays> InWays) noexcept
		{
			Ways = InWays;
			HasWays = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Economy::EconomyTypes Economy;
		PolityTypes Polities;
		LawTypes Laws;
		ReachTypes Reaches;
		ReachRules Rules;
		ComponentType<PolityLine> Line;
		bool HasLine = false;
		ComponentType<RegionWays> Ways;
		bool HasWays = false;
		ComponentType<RegionInPlay> InPlay_;
		bool HasPlay = false;
		WorldGen::RegionGraphCache Roads; ///< rebuilt when the map it was built from is replaced
	};

	/// The authority running in a region (nullptr while no polity has ever held it).
	VAELEN_POLITICS_API const RegionAuthority* AuthorityOf(const World& W, const History::PreHistoryTypes& Types,
														   const ReachTypes& Reaches, uint32 Region);
	/// How far a polity's word carries (nullptr before its first full year).
	VAELEN_POLITICS_API const PolityReach* ReachOf(const World& W, const PolityTypes& Polities,
												   const ReachTypes& Reaches, uint32 Polity);

	struct ReachStats
	{
		uint32 Held = 0;	///< regions with authority running in them
		uint32 Firm = 0;	///< of those, held at more than half
		uint32 Far = 0;		///< the greatest distance any region is held at
		uint64 Spent = 0;	///< grain spent by every polity carrying its word
		uint64 Unpaid = 0;	///< grain of upkeep unpaid, last year
		uint32 Taken = 0;	///< events, from the log
		uint32 Annexed = 0; ///< of those, ground another polity was holding
		uint32 Slipped = 0;
		uint32 Unfunded = 0;
		uint32 Bad = 0; ///< authority of a polity that is gone, a hold above the seat's, a ruled
						///< region with no authority, an authority disagreeing with the rule
		Hash64 Digest = 0; ///< every reach in polity order, then every authority in region order
	};
	VAELEN_POLITICS_API ReachStats MeasureReach(const World& W, const History::PreHistoryTypes& Types,
												const PolityTypes& Polities, const ReachTypes& Reaches,
												const ReachRules& Rules);
} // namespace Vaelen::Politics
