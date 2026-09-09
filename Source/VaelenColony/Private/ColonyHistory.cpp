// VAELEN - VaelenColony
// Phase 11 task 11.07: the colony in the chronicle.
//
// STATUS: PROTOTYPE (Phase 11) - text/deterministic tests in Tests/Colony
#include "Vaelen/Colony/ColonyHistory.h"

#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <cstdio>

namespace Vaelen::Colony
{
	namespace
	{
		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		void AppendPeople(std::string& Out, uint64 Count)
		{
			AppendNumber(Out, Count);
			Out += Count == 1 ? " person" : " people";
		}

		void AppendRegion(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			std::string Name;
			History::NameRegion(W, Types, Region, Name);
			Out += Name;
		}
	} // namespace

	ColonyChronicleTypes ColonyChronicleTypes::Declare(World& W)
	{
		ColonyChronicleTypes T;
		T.State = W.Types().Register<ColonyChronicleState>("ColonyChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void NameColony(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
	{
		Out += "the colony";
		if (Region != 0)
		{
			Out += " of ";
			AppendRegion(W, Types, Region, Out);
		}
	}

	bool DescribeColonyEvent(const World& W, const History::PreHistoryTypes& Types, const ColonyContext& Context,
							 const Event& E, std::string& Out)
	{
		(void)Context;
		if (E.Is(ColonyFoundedEvent))
		{
			const Economy::StockPayload& P = E.Get<Economy::StockPayload>();
			Out += "the ground of ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " was given over to what lay under it";
			return true;
		}
		if (E.Is(OreLiftedEvent))
		{
			const Economy::StockPayload& P = E.Get<Economy::StockPayload>();
			AppendNumber(Out, P.Amount);
			Out += P.Amount == 1 ? " measure of ore came out of " : " measures of ore came out of ";
			NameColony(W, Types, P.Region, Out);
			return true;
		}
		if (E.Is(SeamWorkedOutEvent))
		{
			// House is the deposit index for this event, not a family.
			const Economy::StockPayload& P = E.Get<Economy::StockPayload>();
			Out += "a seam under ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " gave up the last of itself";
			return true;
		}
		return false;
	}

	/// One line for a year's binding, which is one act however many people it
	/// took: a thousand records saying the same thing on the same day is not a
	/// chronicle, it is a ledger.
	void DescribeBinding(const World& W, const History::PreHistoryTypes& Types, uint32 Region, uint32 People,
						 std::string& Out)
	{
		AppendPeople(Out, People);
		Out += People == 1 ? " was bound to " : " were bound to ";
		NameColony(W, Types, Region, Out);
	}
} // namespace Vaelen::Colony
