// VAELEN - VaelenPolitics
// Phase 07.02: law - what a polity demands, written where the systems under it
// can see it, and the grain it takes for having demanded it.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics
//
// A polity's law is a component on the polity: one number today, the share of
// every harvest owed to it. It is not read by the layers below - it is written
// down onto every region the polity rules as RegionDues, a plain struct the
// economy declares and the production system observes. The economy never
// learns what a polity is: it assesses the share on the harvest it just
// reaped and leaves the grain where it lies. The collector comes after, takes
// what the region can pay into the polity's treasury, and what it cannot
// becomes arrears the region still owes.
//
// A law moves by itself: a polity whose treasury is thinner than what it wants
// for the people it rules raises the share, one that is fat lowers it, and a
// polity that has failed to collect for years relents. Every move is an event,
// so the chronicle of 07.07 can say why the bread got dearer.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/PoliticsApi.h"
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
	/// Component on a polity entity: what it demands of the regions it rules.
	struct PolityLaw
	{
		uint32 Polity = 0;
		uint32 TaxPerMille = 0; ///< of every harvest of every region it rules
		uint32 Changes = 0;		///< times the share has moved since the founding
		uint32 Failures = 0;	///< consecutive years a region could not pay in full
		uint64 Since = 0;		///< tick the share last moved
		uint64 Taken = 0;		///< grain taken since the founding
	};
	static_assert(sizeof(PolityLaw) == 32, "PolityLaw must stay padding free");

	/// Component on a polity entity: what it holds, by Good, as a region's stock.
	struct Treasury
	{
		uint32 Amount[8] = {}; ///< [GoodCount] used, the rest reserved
	};
	static_assert(sizeof(Treasury) == 32, "Treasury must stay padding free");

	struct LawTypes
	{
		ComponentType<PolityLaw> Law;
		ComponentType<Treasury> Hoard;
		ComponentType<Economy::RegionDues> Dues; ///< written here, observed by the production system
		static VAELEN_POLITICS_API LawTypes Declare(World& W);
	};

	struct LawRules
	{
		uint32 TaxAtFounding = 80;		 ///< per mille of the harvest, at the founding
		uint32 TaxFloor = 20;			 ///< a law never demands less
		uint32 TaxCeiling = 250;		 ///< nor more
		uint32 TaxStep = 20;			 ///< how far a law moves in a year
		uint32 WantPerPerson = 2;		 ///< grain a polity wants in store per person it rules
		uint32 FatMultiple = 3;			 ///< holding this many times what it wants, it relents
		uint32 FailuresBeforeRelief = 3; ///< years of short collection before it relents anyway
	};

	/// A polity's share moved (Polity, 0, 0, the new per mille).
	inline constexpr EventType<PolityPayload> LawChangedEvent = MakeEventType<PolityPayload>("LawChanged");
	/// A region paid what it owed (Polity, Region, 0, grain taken).
	inline constexpr EventType<PolityPayload> DuesPaidEvent = MakeEventType<PolityPayload>("DuesPaid");
	/// A region could not pay in full (Polity, Region, 0, grain still owed).
	inline constexpr EventType<PolityPayload> DuesUnpaidEvent = MakeEventType<PolityPayload>("DuesUnpaid");

	/// Yearly, after Polities and the harvest: write the law onto the regions,
	/// collect what they owe, then let the law move.
	class VAELEN_POLITICS_API LawSystem final : public ISystem
	{
	public:
		LawSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Economy::EconomyTypes InEconomy,
				  PolityTypes InPolities, LawTypes InLaws, LawRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Economy(InEconomy), Polities(InPolities), Laws(InLaws), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Law"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Polities"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after the harvest too, so that a year's dues are collected the
		/// year they are assessed. The system must exist.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Economy::EconomyTypes Economy;
		PolityTypes Polities;
		LawTypes Laws;
		LawRules Rules;
	};

	/// The law of a polity (nullptr before its first year or for an unknown one).
	VAELEN_POLITICS_API const PolityLaw* LawOf(const World& W, const PolityTypes& Polities, const LawTypes& Laws,
											   uint32 Polity);
	/// What a polity holds (nullptr before its first year or for an unknown one).
	VAELEN_POLITICS_API const Treasury* TreasuryOf(const World& W, const PolityTypes& Polities, const LawTypes& Laws,
												   uint32 Polity);
	/// What a region owes (nullptr while nobody has ever demanded anything of it).
	VAELEN_POLITICS_API const Economy::RegionDues* DuesOf(const World& W, const History::PreHistoryTypes& Types,
														  const LawTypes& Laws, uint32 Region);

	struct LawStats
	{
		uint32 Laws = 0;	///< polities with a law, standing or not
		uint32 Taxed = 0;	///< regions demanded of this year (a share above nothing)
		uint32 Owing = 0;	///< regions carrying arrears
		uint64 Held = 0;	///< grain in every treasury
		uint64 Arrears = 0; ///< grain assessed and not taken
		uint32 Paid = 0;	///< events, from the log
		uint32 Unpaid = 0;
		uint32 Changes = 0;
		uint32 Bad = 0;	   ///< a share outside its bounds, a dissolved polity still demanding, dues with no master
		Hash64 Digest = 0; ///< every law in polity order, then every dues in region order
	};
	VAELEN_POLITICS_API LawStats MeasureLaws(const World& W, const History::PreHistoryTypes& Types,
											 const PolityTypes& Polities, const LawTypes& Laws, const LawRules& Rules);
} // namespace Vaelen::Politics
