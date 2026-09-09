// VAELEN - VaelenPlayer
// Phase 10.05: what the player can do.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player

#include "Vaelen/Player/Doings.h"

#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Player
{
	uint32 Doings::RegionOf(const World& W, uint32 Person) const
	{
		const Population::PersonInfo* P = Population::FindPerson(W, Persons, Person);
		if (P == nullptr || P->State != static_cast<uint8>(Population::LifeState::Alive))
		{
			return 0;
		}
		return P->Region;
	}

	uint32 Doings::HouseOf(const World& W, uint32 Person) const
	{
		const Population::PersonInfo* P = Population::FindPerson(W, Persons, Person);
		return P != nullptr ? P->Family : 0u;
	}

	uint32 Doings::HasGoods(const World& W, uint32 Person) const
	{
		const uint32 Region = RegionOf(W, Person);
		if (Region == 0 || Rules.WorkGood >= Economy::GoodCount)
		{
			return 0;
		}
		const uint32 House = HouseOf(W, Person);
		if (House != 0)
		{
			const Economy::HouseStock* S = Economy::HouseStockOf(W, Families, Economy, House);
			if (S != nullptr)
			{
				return S->Amount[Rules.WorkGood];
			}
		}
		// No house, or a house with no stock of its own: what the region holds
		// in common is what they can lay hands on.
		const Economy::RegionStock* R = Economy::StockOf(W, Types, Economy, Region);
		return R != nullptr ? R->Amount[Rules.WorkGood] : 0u;
	}

	Refusal Doings::Allows(const World& W, uint32 Person, const PlayerCommand& Command) const
	{
		const uint32 Region = RegionOf(W, Person);
		if (Region == 0)
		{
			return Refusal::Dead;
		}
		switch (static_cast<Intent>(Command.Kind))
		{
		case Intent::Wait:
		case Intent::Work:
			return Refusal::None; // a day of work always brings in something

		case Intent::Rest:
		case Intent::Eat:
		{
			// Both need the person to be one the fine grain knows: a person of a
			// coarse region is a number and has no needs to fill.
			const Population::PersonInfo* P = Population::FindPerson(W, Persons, Person);
			if (P == nullptr)
			{
				return Refusal::Dead;
			}
			if (static_cast<Intent>(Command.Kind) == Intent::Eat && HasGoods(W, Person) < Rules.EatGrain)
			{
				return Refusal::Nothing; // no grain anywhere they can reach
			}
			return Refusal::None;
		}

		case Intent::Move:
		{
			// Only somewhere a person could walk to: a region next to this one,
			// and one the world is simulating person by person.
			const WorldGen::RegionGraph& Graph = Ways.Of(W.Map(), Types.World.Regions);
			if (Command.Target == 0 || Command.Target == Region || Region >= Graph.Neighbours.size())
			{
				return Refusal::TooFar;
			}
			bool Next = false;
			for (const uint16 N : Graph.Neighbours[Region])
			{
				Next = Next || uint32{N} == Command.Target;
			}
			if (!Next || !Population::IsDetailed(W, Types, Persons, Command.Target))
			{
				return Refusal::TooFar;
			}
			return Refusal::None;
		}

		case Intent::Speak:
		case Intent::Give:
		case Intent::Take:
		{
			// All three are aimed at somebody, and that somebody has to be alive
			// and standing in the same region.
			if (Command.Target == 0 || Command.Target == Person || RegionOf(W, Command.Target) != Region)
			{
				return Refusal::NoOne;
			}
			if (static_cast<Intent>(Command.Kind) == Intent::Give && HasGoods(W, Person) == 0)
			{
				return Refusal::Nothing;
			}
			if (static_cast<Intent>(Command.Kind) == Intent::Take && HasGoods(W, Command.Target) == 0)
			{
				return Refusal::Nothing;
			}
			return Refusal::None;
		}

		case Intent::None:
		case Intent::Count:
		default:
			return Refusal::Unknown;
		}
	}

	void Doings::Do(World& W, uint32 Person, const PlayerCommand& Command, SimTick Now, PersistentId Cause)
	{
		const uint32 Region = RegionOf(W, Person);
		if (Region == 0)
		{
			return;
		}
		const Intent Kind = static_cast<Intent>(Command.Kind);
		const Economy::Good Good = static_cast<Economy::Good>(
			Rules.WorkGood < Economy::GoodCount ? Rules.WorkGood : static_cast<uint32>(Economy::Good::Grain));
		const uint32 House = HouseOf(W, Person);
		switch (Kind)
		{
		case Intent::Wait:
			break; // the hours were the whole of it

		case Intent::Work:
			// 06.01 owns units of a good; this is the one call that moves them.
			Economy::AddStock(W, Types, Families, Economy, Region, House, Good, static_cast<int32>(Rules.WorkYield),
							  Now, Cause);
			// And a day of it costs the body what eating and resting give back.
			// The yearly ration of 04.04 still tops everyone up once a year, so
			// a hungry day inside a fed year is levelled out at the year's turn -
			// which is the yearly system doing its job, not this one being undone.
			Population::HungerPerson(W, Persons, Needs, Person, Rules.WorkHunger);
			Population::TirePerson(W, Persons, Needs, Person, Rules.WorkTire);
			break;

		case Intent::Rest:
			Population::RestPerson(W, Persons, Needs, Person, Rules.RestGain);
			break;

		case Intent::Eat:
			Economy::AddStock(W, Types, Families, Economy, Region, House, Good, -static_cast<int32>(Rules.EatGrain),
							  Now, Cause);
			Population::FeedPerson(W, Persons, Needs, Person, Rules.EatFood);
			break;

		case Intent::Move:
			// 04.06 owns a person's region, and reconciles both grains.
			Population::MovePerson(W, Types, Persons, Person, Command.Target, Now, Cause);
			break;

		case Intent::Speak:
			// Nothing moves. The act is in the log with who it was aimed at, and
			// 10.06 builds what the people around the player make of them out of
			// exactly that - never out of a dialogue tree.
			break;

		case Intent::Give:
		case Intent::Take:
		{
			const uint32 Most = Kind == Intent::Give ? Rules.GiveMost : Rules.TakeMost;
			const uint32 From = Kind == Intent::Give ? Person : Command.Target;
			const uint32 To = Kind == Intent::Give ? Command.Target : Person;
			const uint32 Wanted = Command.Amount == 0 ? 1u : std::min(Command.Amount, Most);
			const uint32 Moved = std::min(Wanted, HasGoods(W, From));
			if (Moved == 0)
			{
				break;
			}
			const uint32 OutOf = HouseOf(W, From);
			const uint32 Into = HouseOf(W, To);
			const uint32 Left = Economy::AddStock(W, Types, Families, Economy, Region, OutOf, Good,
												  -static_cast<int32>(Moved), Now, Cause);
			if (Left != 0)
			{
				Economy::AddStock(W, Types, Families, Economy, Region, Into, Good, static_cast<int32>(Left), Now,
								  Cause);
			}
			break;
		}

		case Intent::None:
		case Intent::Count:
		default:
			return;
		}
		if (static_cast<usize>(Kind) < IntentCount)
		{
			++Tally[static_cast<usize>(Kind)];
		}
	}

	DoingStats MeasureDoings(const World& W)
	{
		DoingStats S;
		std::vector<PersistentId> Acts;
		for (const Event& E : W.Log().All())
		{
			if (E.Is(PlayerActedEvent))
			{
				++S.Acts;
				Acts.push_back(E.Id);
				const ActPayload A = E.Get<ActPayload>();
				switch (static_cast<Intent>(A.Kind))
				{
				case Intent::Work:
					++S.Worked;
					break;
				case Intent::Eat:
					++S.Ate;
					break;
				case Intent::Rest:
					++S.Rested;
					break;
				case Intent::Give:
					++S.Gave;
					break;
				case Intent::Take:
					++S.Took;
					break;
				default:
					break;
				}
			}
			S.Refused += E.Is(PlayerRefusedEvent) ? 1u : 0u;
			S.Moved += E.Is(Population::PersonMovedEvent) ? 1u : 0u;
		}
		// What the world moved because of an act: the causal edge of 01.05, which
		// is what 10.07 will walk back through every layer under the player.
		for (const Event& E : W.Log().All())
		{
			if (!E.Cause.IsValid())
			{
				continue;
			}
			for (const PersistentId& Act : Acts)
			{
				if (E.Cause == Act)
				{
					++S.Caused;
					break;
				}
			}
		}
		return S;
	}
} // namespace Vaelen::Player
