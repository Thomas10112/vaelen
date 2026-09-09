// VAELEN - VaelenPlayer
// Phase 10.03: the player's grain.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player

#include "Vaelen/Player/Hours.h"

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

		uint32 DayOf(const HourRules& Rules, SimTick Tick) noexcept
		{
			const uint64 PerDay = uint64{Rules.TicksPerHour} * Rules.HoursPerDay;
			return PerDay == 0 ? 0u : static_cast<uint32>(Tick / PerDay);
		}
	} // namespace

	HourTypes HourTypes::Declare(World& W)
	{
		HourTypes T;
		T.Hours = W.Types().Register<PlayerHours>("PlayerHours");
		W.Components().CreatePool(T.Hours);
		return T;
	}

	void PlayerDaySystem::Tick(TickContext& Context)
	{
		World& W = *Owner;
		const EntityHandle H = MarkedHandle(W, Player);
		if (H.IsNull())
		{
			return; // nobody is played; the finer grain has nobody to run for
		}
		const Population::PersonInfo* Who = W.Components().GetPool(Persons.Person).TryGet(H);
		const bool Living = Who != nullptr && Who->State == static_cast<uint8>(Population::LifeState::Alive);
		const uint32 Today = DayOf(Rules, Context.Tick);
		const uint32 Waking = Rules.HoursPerDay > Rules.SleepHours ? Rules.HoursPerDay - Rules.SleepHours : 0u;

		PlayerHours* Day = W.Components().GetPool(Hours.Hours).TryGet(H);
		if (Day == nullptr)
		{
			if (!Living)
			{
				return; // the grain begins with a life, not with a corpse
			}
			PlayerHours Fresh;
			Fresh.Day = Today;
			Fresh.Awake = Waking;
			Fresh.Slept = Rules.SleepHours;
			Fresh.Days = 1;
			Fresh.Since = Context.Tick;
			W.Components().GetPool(Hours.Hours).Add(H, Fresh);
			return;
		}
		if (Day->Day == Today)
		{
			return; // the same day the last tick was in; nothing has turned
		}
		if (!Living)
		{
			// The person is dead, or gone to the coarse grain. The day does not
			// turn for them, and the record says how many did not.
			++Day->Missed;
			Day->Day = Today;
			return;
		}
		Day->Day = Today;
		Day->Awake = Waking;
		Day->Spent = 0;
		Day->Slept = Rules.SleepHours;
		++Day->Days;
	}

	const PlayerHours* HoursOf(const World& W, const HourTypes& Hours)
	{
		const PlayerHours* Found = nullptr;
		W.Components()
			.GetPool(Hours.Hours)
			.ForEach(
				[&](EntityHandle, const PlayerHours& D)
				{
					if (Found == nullptr)
					{
						Found = &D;
					}
				});
		return Found;
	}

	uint32 HoursLeft(const World& W, const HourTypes& Hours)
	{
		const PlayerHours* Day = HoursOf(W, Hours);
		if (Day == nullptr || Day->Spent >= Day->Awake)
		{
			return 0;
		}
		return Day->Awake - Day->Spent;
	}

	uint32 SpendHours(World& W, const HourTypes& Hours, uint32 Wanted)
	{
		PlayerHours* Day = nullptr;
		W.Components()
			.GetPool(Hours.Hours)
			.ForEach(
				[&](EntityHandle, PlayerHours& D)
				{
					if (Day == nullptr)
					{
						Day = &D;
					}
				});
		if (Day == nullptr || Wanted == 0)
		{
			return 0;
		}
		// A day is a budget and never an overdraft.
		const uint32 Left = Day->Spent >= Day->Awake ? 0u : Day->Awake - Day->Spent;
		const uint32 Spent = std::min(Wanted, Left);
		Day->Spent += Spent;
		return Spent;
	}

	HourStats MeasureHours(const World& W, const Population::PersonTypes& Persons, const PlayerTypes& Player,
						   const HourTypes& Hours, const HourRules& Rules)
	{
		HourStats S;
		const uint32 Played = PlayerPerson(W, Player);
		std::vector<PlayerHours> All;
		std::vector<uint32> OnPerson;
		W.Components()
			.GetPool(Hours.Hours)
			.ForEach(
				[&](EntityHandle H, const PlayerHours& D)
				{
					All.push_back(D);
					const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					OnPerson.push_back(P != nullptr ? P->Index : 0u);
				});
		S.Records = static_cast<uint32>(All.size());
		if (S.Records > 1)
		{
			S.Bad += S.Records - 1u; // one day at a time, for one person
		}
		Hash64 D = HashString("PlayerHours");
		for (usize i = 0; i < All.size(); ++i)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&All[i]), sizeof(PlayerHours)));
			if (Played != 0 && OnPerson[i] != Played)
			{
				++S.Bad; // a day being lived by somebody who is not being played
			}
			if (All[i].Spent > All[i].Awake)
			{
				++S.Bad; // more hours spent than the day gave
			}
			const uint32 Waking = Rules.HoursPerDay > Rules.SleepHours ? Rules.HoursPerDay - Rules.SleepHours : 0u;
			if (All[i].Awake > Waking)
			{
				++S.Bad; // a day longer than a day
			}
			if (All[i].Days == 0)
			{
				++S.Bad; // a record of a day that never turned
			}
			S.Days += All[i].Days;
			S.Missed += All[i].Missed;
			S.Left += All[i].Spent >= All[i].Awake ? 0u : All[i].Awake - All[i].Spent;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Player
