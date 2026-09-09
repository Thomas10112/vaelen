// VAELEN - VaelenPlayer
// Phase 10.04: intent as commands.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge/replay tests in Tests/Player

#include "Vaelen/Player/Commands.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Player
{
	namespace
	{
		/// The handle the mark sits on, null when nobody is played.
		EntityHandle MarkedHandle(const World& W, const PlayerTypes& Player)
		{
			EntityHandle Out;
			W.Components()
				.GetPool(Player.Mark)
				.ForEach(
					[&](EntityHandle H, const PlayerMark&)
					{
						if (Out.IsNull())
						{
							Out = H;
						}
					});
			return Out;
		}

		/// The handle the queue sits on, null when there is none.
		EntityHandle QueueHandle(const World& W, const OrderTypes& Orders)
		{
			EntityHandle Out;
			W.Components()
				.GetPool(Orders.Orders)
				.ForEach(
					[&](EntityHandle H, const PlayerOrders&)
					{
						if (Out.IsNull())
						{
							Out = H;
						}
					});
			return Out;
		}

		bool Known(uint8 Kind) noexcept
		{
			return Kind != static_cast<uint8>(Intent::None) && Kind < static_cast<uint8>(Intent::Count);
		}

		uint32 CostOf(const OrderRules& Rules, const PlayerCommand& C) noexcept
		{
			if (C.Hours != 0)
			{
				return C.Hours;
			}
			return C.Kind < 16 ? Rules.HoursOf[C.Kind] : 0u;
		}
	} // namespace

	const char* IntentName(Intent Kind)
	{
		switch (Kind)
		{
		case Intent::None:
			return "none";
		case Intent::Wait:
			return "wait";
		case Intent::Work:
			return "work";
		case Intent::Rest:
			return "rest";
		case Intent::Eat:
			return "eat";
		case Intent::Move:
			return "move";
		case Intent::Speak:
			return "speak";
		case Intent::Give:
			return "give";
		case Intent::Take:
			return "take";
		case Intent::Count:
			break;
		}
		return "unknown";
	}

	const char* RefusalName(Refusal Why)
	{
		switch (Why)
		{
		case Refusal::None:
			return "none";
		case Refusal::NoPlayer:
			return "nobody is played";
		case Refusal::Dead:
			return "the person is dead";
		case Refusal::Unknown:
			return "an intent with no name";
		case Refusal::Costly:
			return "longer than a day";
		case Refusal::Full:
			return "the queue is full";
		case Refusal::Stale:
			return "no longer meant";
		case Refusal::Nothing:
			return "nothing to do it with";
		case Refusal::TooFar:
			return "too far to walk";
		case Refusal::NoOne:
			return "nobody there";
		case Refusal::Count:
			break;
		}
		return "unknown";
	}

	OrderTypes OrderTypes::Declare(World& W)
	{
		OrderTypes T;
		T.Orders = W.Types().Register<PlayerOrders>("PlayerOrders");
		W.Components().CreatePool(T.Orders);
		return T;
	}

	bool BeginOrders(World& W, const PlayerTypes& Player, const OrderTypes& Orders, SimTick Now)
	{
		const EntityHandle H = MarkedHandle(W, Player);
		if (H.IsNull() || !QueueHandle(W, Orders).IsNull())
		{
			return false; // nobody to queue for, or a queue is already open
		}
		PlayerOrders Fresh;
		Fresh.Since = Now;
		W.Components().GetPool(Orders.Orders).Add(H, Fresh);
		return true;
	}

	bool EndOrders(World& W, const OrderTypes& Orders)
	{
		const EntityHandle H = QueueHandle(W, Orders);
		if (H.IsNull())
		{
			return false;
		}
		W.Components().GetPool(Orders.Orders).Remove(H);
		return true;
	}

	Refusal Submit(World& W, const PlayerTypes& Player, const OrderTypes& Orders, const OrderRules& Rules,
				   const PlayerCommand& Command)
	{
		const EntityHandle H = MarkedHandle(W, Player);
		if (H.IsNull())
		{
			return Refusal::NoPlayer;
		}
		PlayerOrders* Queue = W.Components().GetPool(Orders.Orders).TryGet(H);
		if (Queue == nullptr)
		{
			return Refusal::NoPlayer; // the queue was never opened
		}
		if (!Known(Command.Kind))
		{
			return Refusal::Unknown;
		}
		const uint32 Room = std::min<uint32>(Rules.MostHeld, static_cast<uint32>(MostOrders));
		if (Queue->Held >= Room)
		{
			++Queue->Dropped;
			Queue->Last = static_cast<uint32>(Refusal::Full);
			return Refusal::Full;
		}
		// Queued, and NOTHING else: no hour is spent, no store moves, no person
		// changes. The system inside the simulation is the only thing that acts.
		const usize Slot = (usize{Queue->First} + Queue->Held) % MostOrders;
		Queue->Ring[Slot] = Command;
		Queue->Ring[Slot].Why = static_cast<uint8>(Refusal::None);
		++Queue->Held;
		return Refusal::None;
	}

	Refusal Order(World& W, const PlayerTypes& Player, const OrderTypes& Orders, const OrderRules& Rules, Intent Kind,
				  uint32 Target, uint32 Amount, SimTick Now)
	{
		PlayerCommand C;
		C.Kind = static_cast<uint8>(Kind);
		C.Target = Target;
		C.Amount = Amount;
		C.Issued = Now;
		return Submit(W, Player, Orders, Rules, C);
	}

	void PlayerOrderSystem::Tick(TickContext& Context)
	{
		World& W = *Owner;
		const EntityHandle H = MarkedHandle(W, Player);
		if (H.IsNull())
		{
			return; // nobody is played
		}
		PlayerOrders* Queue = W.Components().GetPool(Orders.Orders).TryGet(H);
		if (Queue == nullptr || Queue->Held == 0)
		{
			return; // nothing is meant
		}
		const Population::PersonInfo* Who = W.Components().GetPool(Persons.Person).TryGet(H);
		const bool Living = Who != nullptr && Who->State == static_cast<uint8>(Population::LifeState::Alive);
		const uint32 Person = Who != nullptr ? Who->Index : 0u;

		while (Queue->Held != 0)
		{
			PlayerCommand& C = Queue->Ring[Queue->First];
			Refusal Why = Refusal::None;
			if (!Living)
			{
				Why = Refusal::Dead;
			}
			else if (!Known(C.Kind))
			{
				Why = Refusal::Unknown;
			}
			else if (Rules.StaleAfter != 0 && C.Issued + Rules.StaleAfter < Context.Tick)
			{
				// It waited through more days than it was meant for. A person
				// who wanted to eat a month ago does not want to eat now.
				Why = Refusal::Stale;
			}
			const uint32 Cost = CostOf(Rules, C);
			const PlayerHours* Day = HoursOf(W, Hours);
			const uint32 Whole = Day != nullptr ? Day->Awake : 0u;
			if (Why == Refusal::None && Cost > Whole)
			{
				// Nothing can pay for it, so leaving it in would block every
				// intent behind it forever.
				Why = Refusal::Costly;
			}
			if (Why == Refusal::None && Cost > HoursLeft(W, Hours))
			{
				break; // the day is gone; what is left waits for tomorrow
			}
			if (Why == Refusal::None && Doing != nullptr)
			{
				// Asked before a single hour is spent, so that something the
				// world will not allow costs the person nothing.
				Why = Doing->Allows(W, Person, C);
			}
			const PlayerCommand Done = C;
			C.Why = static_cast<uint8>(Why);
			Queue->First = static_cast<uint32>((usize{Queue->First} + 1) % MostOrders);
			--Queue->Held;
			if (Why != Refusal::None)
			{
				++Queue->Refused;
				Queue->Last = static_cast<uint32>(Why);
				Context.Events->Publish(Context.Tick, PlayerRefusedEvent,
										ActPayload{Person, Done.Kind, Done.Target, static_cast<uint32>(Why)},
										W.Entities().GetId(H));
				continue;
			}
			const uint32 Paid = SpendHours(W, Hours, Cost);
			++Queue->Taken;
			const PersistentId Act =
				Context.Events->Publish(Context.Tick, PlayerActedEvent,
										ActPayload{Person, Done.Kind, Done.Target, Paid}, W.Entities().GetId(H));
			if (Doing != nullptr)
			{
				// The effect of each kind, through the system that already owns
				// that part of the world, with the act itself as the cause.
				Doing->Do(W, Person, Done, Context.Tick, Act);
			}
		}
	}

	const PlayerOrders* OrdersOf(const World& W, const OrderTypes& Orders)
	{
		const PlayerOrders* Found = nullptr;
		W.Components()
			.GetPool(Orders.Orders)
			.ForEach(
				[&](EntityHandle, const PlayerOrders& Q)
				{
					if (Found == nullptr)
					{
						Found = &Q;
					}
				});
		return Found;
	}

	uint32 OrdersHeld(const World& W, const OrderTypes& Orders)
	{
		const PlayerOrders* Queue = OrdersOf(W, Orders);
		return Queue != nullptr ? Queue->Held : 0u;
	}

	OrderStats MeasureOrders(const World& W, const Population::PersonTypes& Persons, const PlayerTypes& Player,
							 const OrderTypes& Orders, const OrderRules& Rules)
	{
		OrderStats S;
		const uint32 Played = PlayerPerson(W, Player);
		const uint32 Room = std::min<uint32>(Rules.MostHeld, static_cast<uint32>(MostOrders));
		Hash64 D = HashString("PlayerOrders");
		W.Components()
			.GetPool(Orders.Orders)
			.ForEach(
				[&](EntityHandle H, const PlayerOrders& Q)
				{
					++S.Queues;
					S.Held += Q.Held;
					S.Taken += Q.Taken;
					S.Refused += Q.Refused;
					S.Dropped += Q.Dropped;
					const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (Played != 0 && (P == nullptr || P->Index != Played))
					{
						++S.Bad; // a queue on somebody who is not being played
					}
					if (Q.Held > Room || Q.First >= MostOrders)
					{
						++S.Bad; // more waiting than the ring can hold
					}
					for (usize i = 0; i < Q.Held && i < MostOrders; ++i)
					{
						const PlayerCommand& C = Q.Ring[(usize{Q.First} + i) % MostOrders];
						if (!Known(C.Kind))
						{
							++S.Bad; // a waiting intent this build has no name for
						}
					}
					// The whole record, ring included: a replayed life must hold
					// the same intents in the same slots, not merely the same
					// count of them.
					D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Q), sizeof(PlayerOrders)));
				});
		if (S.Queues > 1)
		{
			S.Bad += S.Queues - 1u; // one queue to a world
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Player
