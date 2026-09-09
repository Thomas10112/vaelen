// VAELEN - VaelenGameplay
// Phase 12 task 12.07: gameplay in the chronicle.
//
// Every layer of this project ends the same way: the events that matter get a
// sentence, in one hand, built purely from the state and the log. This is that
// for Phase 12 - what was known, by whom, and when.
//
// But the phase's own rule makes this more than a twelfth set of sentences.
// **A person acts on what they believe, and the world acts on what it has
// heard.** So the interesting question is not "what happened" - 03.07 has
// answered that since Phase 03 - it is WHY somebody did something, walked back
// not to what was true but to what its maker had been told. A man bound in
// Kratfa was not bound because he stole; he was bound because a name reached
// Kratfa, and that name reached Kratfa because it reached somewhere else first.
// 12.05 and 12.06 now publish those tellings as the CAUSE of what is done about
// them, so History::Why walks the belief and not the fact.
//
// STATUS: PROTOTYPE (Phase 12) - text/deterministic tests in Tests/Gameplay/Test_Chronicle.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Gameplay/Documents.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Gameplay/Judgement.h"
#include "Vaelen/Gameplay/Maps.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Gameplay
{
	/// Everything the gameplay text needs to name things.
	struct GameplayContext
	{
		Population::PersonTypes Persons;
		ReputeTypes Repute;
		DocumentTypes Documents;
		MapTypes Maps;
		FameTypes Fame;
	};

	/// "Aelric", or "person 412" when nothing named them. The chronicle of a
	/// world where most people are never named still has to say who.
	VAELEN_GAMEPLAY_API void NamePerson(const World& W, const History::PreHistoryTypes& Types,
										const Population::PersonTypes& Persons, uint32 Person, std::string& Out);

	/// A sentence for one gameplay event, in the same hand as every layer below.
	/// False when the event is not one of this layer's.
	VAELEN_GAMEPLAY_API bool DescribeGameplayEvent(const World& W, const History::PreHistoryTypes& Types,
												   const GameplayContext& Context, const Event& E, std::string& Out);

	/// One step of somebody's belief: an event, and the sentence for it.
	struct BeliefStep
	{
		PersistentId Event_;
		uint64 Tick = 0;
		uint32 Region = 0; ///< the place that came to believe it, when there is one
		uint32 Hops = 0;   ///< roads it had crossed by then
		std::string Text;
	};

	/// Why somebody was condemned, pardoned, or had a name at all: the chain of
	/// TELLINGS behind it, newest first, ending where the name was earned.
	///
	/// This is 03.07's Why with one difference that is the whole of Phase 12:
	/// what it walks back through is what a place was told, so the last step is
	/// where somebody actually did something and every step before it is
	/// somebody repeating it. A chain of length one is a place acting on what it
	/// saw; a chain of length four is a place acting on a rumour four valleys
	/// old.
	VAELEN_GAMEPLAY_API void WhyBelieved(const World& W, const History::PreHistoryTypes& Types,
										 const GameplayContext& Context, PersistentId Id, std::vector<BeliefStep>& Out,
										 uint32 MaxDepth = 32);

	/// What a place could say about somebody at a tick, and since when - the
	/// "what was known, by whom, and when" of this task, answered for a place.
	/// False when the place had never heard of them.
	VAELEN_GAMEPLAY_API bool WhatWasKnown(const World& W, const History::PreHistoryTypes& Types,
										  const GameplayContext& Context, uint32 Region, uint32 Person,
										  std::string& Out);

	struct ChronicleStats
	{
		uint32 Events = 0;	  ///< gameplay events in the log
		uint32 Described = 0; ///< of them, ones with a sentence
		uint32 Believed = 0;  ///< judgements whose cause chain reaches a telling
		uint32 Longest = 0;	  ///< the longest chain of tellings behind any of them
		Hash64 Digest = 0;	  ///< every sentence, in log order
	};
	VAELEN_GAMEPLAY_API ChronicleStats MeasureChronicle(const World& W, const History::PreHistoryTypes& Types,
														const GameplayContext& Context);
} // namespace Vaelen::Gameplay
