// VAELEN - VaelenGameplay
// Phase 12 task 12.02: an opinion between any two people, and hearsay.
//
// What this is not, and the correction is worth keeping. The Phase 12 breakdown
// first said that AELVOR was full of omniscience - that Regard.cpp reads the
// world's whole log to decide what somebody thinks. It does not. It walks back
// only to the current tick and stops, takes only acts AIMED at somebody, and
// records the opinion on the person who was acted upon. That is already exactly
// what reached them, and carefully so.
//
// The gap is the other way round, and narrower. 10.06 gives an opinion only
// about THE PLAYED PERSON - `PlayerRegard` lives on the played person - and
// 12.01 has just given unplayed people acts of their own with nobody to think
// anything of them. And nothing TRAVELS: an opinion moves only between the two
// people involved, and nobody in this world has ever heard anything second
// hand.
//
// A reputation is what a world gets when what happened to one person reaches a
// third. That is the whole of this task: the same opinion 10.06 already models,
// for everybody rather than for one, and a way for it to be told.
//
// STATUS: PROTOTYPE (Phase 12) - unit/deterministic tests in Tests/Gameplay/Test_Repute.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Population/Persons.h"
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
	/// How many people one person is thought about by at once. The same handful
	/// 10.06 chose, and for the same reason: a person is known to the people
	/// they have dealt with, not to a region.
	inline constexpr usize MostThoughtOf = Player::MostKnown;

	/// Component on a person: what other people think of THEM. The mirror of
	/// 10.06's PlayerRegard, which holds exactly this for the one played person.
	struct PersonRepute
	{
		uint32 Known = 0;	   ///< how many of Who are in use
		uint32 Kindnesses = 0; ///< things done for them
		uint32 Wrongs = 0;	   ///< things taken from them
		uint32 Heard = 0;	   ///< opinions that reached somebody second hand
		int32 Repute = 0;	   ///< the opinions together
		uint32 Reserved = 0;   //
		uint64 Since = 0;	   ///< tick the first of them was formed
		Player::Opinion Who[MostThoughtOf];
	};
	static_assert(sizeof(PersonRepute) == 224, "PersonRepute must stay padding free");

	struct ReputeTypes
	{
		ComponentType<PersonRepute> Repute;

		static VAELEN_GAMEPLAY_API ReputeTypes Declare(World& W);
	};

	struct ReputeRules
	{
		int32 ForSpeaking = 15;	  ///< the same weights 10.06 gives, so one world, one scale
		int32 ForGiving = 60;	  //
		int32 PerUnitGiven = 30;  //
		int32 ForTaking = -120;	  //
		int32 PerUnitTaken = -40; //
		int32 Most = 1000;
		int32 Least = -1000;
		/// What a thing HEARD is worth against a thing suffered, per mille. A
		/// story is worth less than a scar, and it is worth something.
		uint32 HeardPerMille = 300;
		/// A speaker tells what they think of one person they think about. This
		/// many per mille of the speakings carry a story at all; the rest are
		/// only words.
		uint32 TellsPerMille = 400;
	};

	/// One person's opinion reaching a third (Person = who is spoken of,
	/// Target = who heard it, Amount = what it was worth to them).
	inline constexpr EventType<Player::ActPayload> HeardOfEvent = MakeEventType<Player::ActPayload>("HeardOf");

	/// Daily, after Living: what people make of each other out of what was done
	/// to them, and out of what they were told.
	///
	/// Two things happen and they are deliberately separate. An act aimed at
	/// somebody moves THAT person's opinion of whoever did it - first hand, the
	/// same arithmetic 10.06 uses. And a speaking may carry the speaker's
	/// opinion of a third person to whoever they spoke to, worth less than
	/// having been there. Nothing else in twelve phases lets anybody learn
	/// anything they did not suffer.
	class VAELEN_GAMEPLAY_API ReputeSystem final : public ISystem
	{
	public:
		ReputeSystem(World& InWorld, Population::PersonTypes InPersons, ReputeTypes InRepute,
					 ReputeRules InRules) noexcept
			: Owner(&InWorld), Persons(InPersons), Repute(InRepute), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Repute"; }
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
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		Population::PersonTypes Persons;
		ReputeTypes Repute;
		ReputeRules Rules;
	};

	/// The slot holding what one person thinks of another, made if there is room
	/// and taken from the oldest when there is not. Public because 12.03 needs
	/// exactly this and a second copy of it got the eviction wrong: a page could
	/// teach nobody anything about a person already thought of by the full
	/// handful, which is every interesting person.
	VAELEN_GAMEPLAY_API Player::Opinion* SlotFor(PersonRepute& About, uint32 Holder, SimTick Now);

	/// What the world thinks of somebody (0 when nobody thinks anything).
	VAELEN_GAMEPLAY_API int32 ReputeOf(const World& W, const Population::PersonTypes& Persons, const ReputeTypes& Types,
									   uint32 Person);
	/// The opinion one person holds of another, or nullptr when they hold none.
	VAELEN_GAMEPLAY_API const Player::Opinion* OpinionOf(const World& W, const Population::PersonTypes& Persons,
														 const ReputeTypes& Types, uint32 About, uint32 Holder);

	struct ReputeStats
	{
		uint32 ThoughtOf = 0; ///< people somebody has an opinion about
		uint32 Opinions = 0;  ///< opinions in all
		uint32 Heard = 0;	  ///< of them, ones that reached somebody second hand
		uint32 Tellings = 0;  ///< HeardOf events in the log
		int32 Best = 0;
		int32 Worst = 0;
		Hash64 Digest = 0;
	};
	VAELEN_GAMEPLAY_API ReputeStats MeasureRepute(const World& W, const Population::PersonTypes& Persons,
												  const ReputeTypes& Types);
} // namespace Vaelen::Gameplay
