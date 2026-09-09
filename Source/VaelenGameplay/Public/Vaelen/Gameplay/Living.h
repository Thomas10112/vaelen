// VAELEN - VaelenGameplay
// Phase 12 task 12.01: a person nobody is playing.
//
// This module invents no verb. 10.05 owns the seven, and `Doings::Do` takes a
// PERSON INDEX rather than "the player", so the verbs have always been
// anybody's - what was the player's is the plumbing around them. What this
// module decides is INTENT, and nothing else.
//
// Which verbs, and why it is not a matter of taste. Reading what each one does
// splits them exactly, and the line is conservation:
//
//   Wait       nothing at all
//   Speak      nothing moves; the act is in the log and 10.06 reads it
//   Give/Take  move goods between two houses - conservative
//   Move       MovePerson, which reconciles both grains - conservative
//   Work       AddStock(+WorkYield) - CREATES goods
//   Eat        AddStock(-EatGrain) and FeedPerson - DESTROYS and fills
//   Rest       RestPerson - fills
//
// 06.02 already harvests for everybody and 04.04 already rations everybody, in
// aggregate. So Work, Eat and Rest done person by person count a life's labour
// and food TWICE. For one played person that is negligible and 10.03 accepted
// it knowingly; for a region of unplayed people the economy would roughly
// double. **The verbs that create or destroy are already done in aggregate; the
// verbs that only move or say are done by nobody.** That hole is this task.
//
// STATUS: PROTOTYPE (Phase 12) - unit/deterministic/edge tests in Tests/Gameplay/Test_Living.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Gameplay
{
	/// Component on a region entity: its people act, one at a time, as people
	/// rather than as counts.
	///
	/// A component and not a rule, for the reason ADR-0090 gives and Phase 11
	/// paid for three times: ground becomes lively at a tick, and a rule fixed
	/// when the system is built has no date.
	struct RegionLively
	{
		uint32 Why = 0; ///< free for whoever marks the ground
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RegionLively) == 8, "RegionLively must stay padding free");

	struct LivingTypes
	{
		ComponentType<RegionLively> Lively;

		static VAELEN_GAMEPLAY_API LivingTypes Declare(World& W);
	};

	struct LivingRules
	{
		uint32 FromAge = 12;			 ///< a person acts for themselves from this age
		uint32 ActPerMille = 200;		 ///< of them do something on a given day
		uint32 GiveMost = 2;			 ///< units a person parts with at once
		uint32 SpeakSharePerMille = 700; ///< of the acts that are only words

		/// A person bolder than this takes what they want instead of asking.
		///
		/// 12.01 shipped with Speak and Give and nothing else, and ADR-0096 gave
		/// the reason: the verbs that CREATE or DESTROY are already done in
		/// aggregate, and the ones that only MOVE or SAY were done by nobody. But
		/// Take only moves, and leaving it out cost the phase its whole negative
		/// half - 12.02's `worst` was 0 because nobody had ever wronged anybody.
		///
		/// Why character and not want, which was tried first and measured. Want
		/// in AELVOR is not individual: 06.02 works out ONE ration for a whole
		/// region and 04.04 moves every person's Food by it, so everybody in a
		/// place is exactly as fed as everybody else. Measured, all 1428 people
		/// of the world's best-fed region sit at 255 of 255; a land lean enough
		/// to push them under the hunger line at all leaves 5 alive out of 1428.
		/// There is no window in which some people are hungry and others are not,
		/// so hunger cannot decide who steals.
		///
		/// Boldness can. It is 04.05's, it is 0..255 with 128 ordinary, and it is
		/// heritable at half - so a bold line stays bold, and who is thought ill
		/// of in this world is a thing that runs in families rather than a thing
		/// the dice decide each morning.
		uint32 TakeWhenBolderThan = 200;
		/// Of a bold person's acts that are a taking. Character raises the odds;
		/// it does not replace the person. A first version had them take on EVERY
		/// act, which turned most acts into two stock movements and an event
		/// each, and cost the acting about nine times what it had.
		uint32 TakePerMille = 250;
		uint32 TakeMost = 2; ///< units taken at once
	};

	/// Daily: the people of a lively region do something conservative - they
	/// speak to somebody they share ground with, or hand them something they can
	/// spare. Nothing is created and nothing is destroyed, so a region of people
	/// living can never inflate the world that 06.02 and 04.04 already feed.
	///
	/// What decides the intent here is deliberately thin - somebody nearby, and
	/// something to spare - because 12.02 is the answer to that question: a
	/// person acts on what they BELIEVE, and nobody believes anything yet.
	/// This is the plumbing that 12.02 will steer, and it says so in place.
	class VAELEN_GAMEPLAY_API LivingSystem final : public ISystem
	{
	public:
		LivingSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 LivingTypes InLiving, LivingRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Living(InLiving), Rules(InRules)
		{
		}
		/// Optional: what 04.05 knows about a person's character. Without it
		/// nobody is ever bold enough to take, and this system is exactly what
		/// it was when 12.01 shipped.
		void ObserveTraits(ComponentType<Population::PersonTraits> InTraits) noexcept
		{
			Traits = InTraits;
			HasTraits = true;
		}
		const char* GetName() const noexcept override { return "Living"; }
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out;
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Who carries out an act. The same IDoing 10.04 hands to the player's
		/// own system, so an unplayed person and a played one do the same thing
		/// by the same call - which is the whole claim of this task.
		void ObserveDoing(Player::IDoing* InDoing) noexcept { Doing = InDoing; }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		LivingTypes Living;
		LivingRules Rules;
		Player::IDoing* Doing = nullptr;
		ComponentType<Population::PersonTraits> Traits;
		bool HasTraits = false;
	};

	/// Marks a region's people as living their own lives. False for an unknown
	/// region or one already marked.
	VAELEN_GAMEPLAY_API bool MakeLively(World& W, const History::PreHistoryTypes& Types, const LivingTypes& Living,
										uint32 Region);

	struct LivingStats
	{
		uint32 Regions = 0; ///< lively ground
		uint32 Acts = 0;	///< acts carried out, from the log
		uint32 Spoke = 0;
		uint32 Gave = 0;
		uint32 Refused = 0; ///< intents the world would not have
		Hash64 Digest = 0;	///< every act in order: who, to whom, which verb
	};
	VAELEN_GAMEPLAY_API LivingStats MeasureLiving(const World& W, const LivingTypes& Living, uint32 Region);
} // namespace Vaelen::Gameplay
