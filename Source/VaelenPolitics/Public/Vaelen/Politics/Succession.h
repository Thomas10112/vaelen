// VAELEN - VaelenPolitics
// Phase 07.04: succession - what a polity's line remembers, and what it costs
// when the seat passes to someone the custom did not name.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics
//
// A polity does not choose its ruler: 07.01 seats the council's head, and this
// system does not overrule that. What it does is remember and judge. Every
// year it names the claimant the culture's descent custom points at - the
// eldest living child of age of whoever sits - and when the seat changes hands
// it compares who took it against who was named.
//
// A seat that falls empty, and a seat taken by someone the custom did not
// name, both leave unrest: a number on the polity that fades year by year and
// that the reach system subtracts from the hold of every region while it
// lasts. A disputed succession is not an event the polity survives untouched;
// it is a year in which the far edge is likelier to slip.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/PoliticsApi.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/Norms.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Politics
{
	/// Component on a polity entity: its line of rulers and what the last
	/// passing of the seat cost it.
	struct PolityLine
	{
		uint32 Polity = 0;
		uint32 Sitting = 0;	   ///< the person on the seat as this system last saw it
		uint32 Rulers = 0;	   ///< how many have sat since the founding
		uint32 Interregna = 0; ///< times the seat has stood empty
		uint32 Disputed = 0;   ///< successions the custom did not name
		uint32 Unrest = 0;	   ///< per mille, fading; the reach system subtracts it from every hold
		uint32 Claimant = 0;   ///< whom the custom names for the seat next, 0 = nobody
		uint32 Reserved = 0;
		uint64 Seated = 0; ///< tick the sitting ruler took the seat
		uint64 Vacant = 0; ///< tick the seat fell empty, 0 while it is filled
	};
	static_assert(sizeof(PolityLine) == 48, "PolityLine must stay padding free");

	struct SuccessionTypes
	{
		ComponentType<PolityLine> Line;
		static VAELEN_POLITICS_API SuccessionTypes Declare(World& W);
	};

	struct SuccessionRules
	{
		uint32 HeirFromAge = 20;	   ///< a claimant is of age
		uint32 UnrestOnDisputed = 250; ///< per mille added when the custom is passed over
		uint32 UnrestOnVacancy = 150;  ///< per mille added when the seat falls empty
		uint32 UnrestFadePerYear = 50; ///< per mille the world forgets a year
		uint32 UnrestCeiling = 600;	   ///< no shock ever costs more than this
	};

	/// The seat fell empty (Polity, Seat, the ruler who is gone, 0).
	inline constexpr EventType<PolityPayload> SeatFellVacantEvent = MakeEventType<PolityPayload>("SeatFellVacant");
	/// The seat passed to whom the custom named (Polity, Seat, the new ruler, 1).
	inline constexpr EventType<PolityPayload> SuccessionSettledEvent =
		MakeEventType<PolityPayload>("SuccessionSettled");
	/// The seat passed to someone else (Polity, Seat, the new ruler, the claimant passed over).
	inline constexpr EventType<PolityPayload> SuccessionDisputedEvent =
		MakeEventType<PolityPayload>("SuccessionDisputed");

	/// Yearly, after Polities: name the claimant the custom points at, judge the
	/// passing of the seat against it, and let the unrest fade.
	class VAELEN_POLITICS_API SuccessionSystem final : public ISystem
	{
	public:
		SuccessionSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
						 Society::NormTypes InNorms, PolityTypes InPolities, SuccessionTypes InLines,
						 SuccessionRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Norms(InNorms), Polities(InPolities), Lines(InLines),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Succession"; }
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
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Society::NormTypes Norms;
		PolityTypes Polities;
		SuccessionTypes Lines;
		SuccessionRules Rules;
	};

	/// The line of a polity (nullptr before its first full year or for an unknown one).
	VAELEN_POLITICS_API const PolityLine* LineOf(const World& W, const PolityTypes& Polities,
												 const SuccessionTypes& Lines, uint32 Polity);

	struct SuccessionStats
	{
		uint32 Lines = 0;	 ///< polities with a line, standing or not
		uint32 Empty = 0;	 ///< standing polities whose seat is vacant now
		uint32 Troubled = 0; ///< standing polities carrying unrest
		uint32 Unrest = 0;	 ///< the worst unrest anywhere, per mille
		uint32 Rulers = 0;	 ///< rulers seated, over every line
		uint32 Interregna = 0;
		uint32 Disputes = 0;
		uint32 Vacancies = 0; ///< events, from the log
		uint32 Settled = 0;
		uint32 Contested = 0;
		uint32 Bad = 0;	   ///< a line disagreeing with the polity's ruler, unrest past its ceiling,
						   ///< a claimant who is not alive and of age
		Hash64 Digest = 0; ///< every line in polity order
	};
	VAELEN_POLITICS_API SuccessionStats MeasureSuccession(const World& W, const PolityTypes& Polities,
														  const Population::PersonTypes& Persons,
														  const SuccessionTypes& Lines, const SuccessionRules& Rules);
} // namespace Vaelen::Politics
