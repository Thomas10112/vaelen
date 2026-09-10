// VAELEN - VaelenGameplay
// Phase 12 task 12.07: gameplay in the chronicle.
//
// STATUS: PROTOTYPE (Phase 12) - text/deterministic tests in Tests/Gameplay/Test_Chronicle.cpp
#include "Vaelen/Gameplay/GameplayHistory.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <cstdio>

namespace Vaelen::Gameplay
{
	namespace
	{
		void AppendNumber(std::string& Out, int64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%lld", static_cast<long long>(Value));
			Out += Buffer;
		}

		void AppendRegion(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			std::string Name;
			History::NameRegion(W, Types, Region, Name);
			Out += Name;
		}

		void AppendRoads(std::string& Out, uint32 Hops)
		{
			if (Hops == 0)
			{
				Out += " where it was earned";
				return;
			}
			Out += ", ";
			AppendNumber(Out, static_cast<int64>(Hops));
			Out += Hops == 1 ? " road away" : " roads away";
		}

		/// Good or ill, said in words rather than in a number, because a
		/// chronicle is read and not queried.
		void AppendSaid(std::string& Out, int32 Said)
		{
			if (Said < 0)
			{
				Out += "ill";
			}
			else
			{
				Out += "well";
			}
			Out += " of (";
			AppendNumber(Out, Said);
			Out += ")";
		}
	} // namespace

	void NamePerson(const World& W, const History::PreHistoryTypes& Types, const Population::PersonTypes& Persons,
					uint32 Person, std::string& Out)
	{
		if (Person == 0)
		{
			Out += "nobody";
			return;
		}
		const History::NameText Name = Population::PersonName(W, Types.Languages, Persons, Person);
		if (Name.Chars[0] != '\0')
		{
			Out += Name.Chars;
			return;
		}
		Out += "person ";
		AppendNumber(Out, static_cast<int64>(Person));
	}

	bool DescribeGameplayEvent(const World& W, const History::PreHistoryTypes& Types, const GameplayContext& Context,
							   const Event& E, std::string& Out)
	{
		if (E.Is(NameTravelledEvent) || E.Is(NameForgottenEvent) || E.Is(CondemnedEvent) || E.Is(PardonedEvent))
		{
			const FamePayload& P = E.Get<FamePayload>();
			if (E.Is(NameTravelledEvent))
			{
				AppendRegion(W, Types, P.Region, Out);
				Out += " came to speak ";
				AppendSaid(Out, P.Said);
				Out += " ";
				NamePerson(W, Types, Context.Persons, P.Person, Out);
				AppendRoads(Out, P.Hops);
				return true;
			}
			if (E.Is(NameForgottenEvent))
			{
				AppendRegion(W, Types, P.Region, Out);
				Out += " stopped speaking of ";
				NamePerson(W, Types, Context.Persons, P.Person, Out);
				Out += " at all";
				return true;
			}
			if (E.Is(CondemnedEvent))
			{
				NamePerson(W, Types, Context.Persons, P.Person, Out);
				Out += " was bound by ";
				AppendRegion(W, Types, P.Region, Out);
				Out += " for a name it spoke ";
				AppendSaid(Out, P.Said);
				AppendRoads(Out, P.Hops);
				return true;
			}
			NamePerson(W, Types, Context.Persons, P.Person, Out);
			Out += " was let go by ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " for a name it spoke ";
			AppendSaid(Out, P.Said);
			return true;
		}
		if (E.Is(HeardOfEvent))
		{
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			NamePerson(W, Types, Context.Persons, P.Target, Out);
			Out += " was told something of ";
			NamePerson(W, Types, Context.Persons, P.Person, Out);
			Out += " by somebody who had been there";
			return true;
		}
		// The documents and the maps all carry 10.05's ActPayload, and each one
		// packs it differently: a document says {writer, document, about, worth}
		// but a copy says {holder, new, original, 0}, and a map says
		// {person, map, claims, forged}. Read from the publisher, not guessed.
		if (E.Is(DocumentWrittenEvent))
		{
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			NamePerson(W, Types, Context.Persons, P.Person, Out);
			Out += " wrote down what they thought of ";
			NamePerson(W, Types, Context.Persons, P.Target, Out);
			return true;
		}
		if (E.Is(DocumentReadEvent))
		{
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			NamePerson(W, Types, Context.Persons, P.Person, Out);
			Out += " read a page about ";
			NamePerson(W, Types, Context.Persons, P.Target, Out);
			Out += " and believed it, though the writer may have changed their mind since";
			return true;
		}
		if (E.Is(DocumentCopiedEvent))
		{
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			NamePerson(W, Types, Context.Persons, P.Person, Out);
			Out += " copied a page, and the copy does not know it has aged";
			return true;
		}
		if (E.Is(DocumentLostEvent))
		{
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			Out += "a page about ";
			NamePerson(W, Types, Context.Persons, P.Target, Out);
			Out += " was lost, and what it said is gone while what it caused is not";
			return true;
		}
		if (E.Is(MapWrittenEvent) || E.Is(MapReadEvent))
		{
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			NamePerson(W, Types, Context.Persons, P.Person, Out);
			if (E.Is(MapWrittenEvent))
			{
				Out += " drew a map making ";
				AppendNumber(Out, static_cast<int64>(P.Target));
				Out += P.Target == 1 ? " claim about the ground" : " claims about the ground";
				return true;
			}
			Out += " read a map of ";
			AppendNumber(Out, static_cast<int64>(P.Target));
			Out += P.Target == 1 ? " claim" : " claims";
			if (P.Amount != 0)
			{
				Out += ", ";
				AppendNumber(Out, static_cast<int64>(P.Amount));
				Out += " of them false, and cannot tell which";
			}
			return true;
		}
		return false;
	}

	void WhyBelieved(const World& W, const History::PreHistoryTypes& Types, const GameplayContext& Context,
					 PersistentId Id, std::vector<BeliefStep>& Out, uint32 MaxDepth)
	{
		Out.clear();
		std::vector<const Event*> Chain;
		History::CauseChain(W.Log(), Id, Chain, MaxDepth);
		for (const Event* E : Chain)
		{
			BeliefStep Step;
			Step.Event_ = E->Id;
			Step.Tick = static_cast<uint64>(E->Tick);
			if (E->Is(NameTravelledEvent) || E->Is(NameForgottenEvent) || E->Is(CondemnedEvent) || E->Is(PardonedEvent))
			{
				const FamePayload& P = E->Get<FamePayload>();
				Step.Region = P.Region;
				Step.Hops = P.Hops;
			}
			if (!DescribeGameplayEvent(W, Types, Context, *E, Step.Text))
			{
				History::DescribeEvent(W, Types, *E, Step.Text);
			}
			Out.push_back(Step);
		}
	}

	bool WhatWasKnown(const World& W, const History::PreHistoryTypes& Types, const GameplayContext& Context,
					  uint32 Region, uint32 Person, std::string& Out)
	{
		const Fame* F = FameIn(W, Types, Context.Fame, Region, Person);
		if (F == nullptr)
		{
			return false;
		}
		AppendRegion(W, Types, Region, Out);
		Out += " speaks ";
		AppendSaid(Out, F->Said);
		Out += " ";
		NamePerson(W, Types, Context.Persons, Person, Out);
		AppendRoads(Out, F->Hops);
		Out += ", and has since year ";
		AppendNumber(Out, static_cast<int64>(F->Since / History::TicksPerYear));
		return true;
	}

	BeliefStats MeasureBelief(const World& W, const History::PreHistoryTypes& Types, const GameplayContext& Context)
	{
		BeliefStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const Event& E : W.Log().All())
		{
			std::string Line;
			const bool Mine = DescribeGameplayEvent(W, Types, Context, E, Line);
			if (!Mine && !E.Is(HeardOfEvent))
			{
				continue;
			}
			++Out.Events;
			if (Mine && !Line.empty())
			{
				++Out.Described;
				Digest = HashCombine(Digest, HashString(Line));
			}
			if (!E.Is(CondemnedEvent) && !E.Is(PardonedEvent))
			{
				continue;
			}
			std::vector<BeliefStep> Chain;
			WhyBelieved(W, Types, Context, E.Id, Chain);
			// One step is the judgement itself; anything beyond it is a telling
			// the place acted on rather than something it saw.
			if (Chain.size() > 1)
			{
				++Out.Believed;
			}
			const uint32 Depth = static_cast<uint32>(Chain.size());
			Out.Longest = Depth > Out.Longest ? Depth : Out.Longest;
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
