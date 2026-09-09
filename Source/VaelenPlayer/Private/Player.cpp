// VAELEN - VaelenPlayer
// Phase 10.01: the player as a marker on one person the world already had.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player

#include "Vaelen/Player/Player.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Player
{
	namespace
	{
		constexpr uint64 PlayerSalt = 0x504c415945ull; // "PLAYE"

		/// The handle of a living person by index, null when there is none. A
		/// person of a coarse region does not exist as a person at all, so this
		/// answers for the second half of the rule as well as the first.
		EntityHandle LivingPerson(const World& W, const Population::PersonTypes& Persons, uint32 Person)
		{
			EntityHandle Out;
			if (Person == 0)
			{
				return Out;
			}
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const Population::PersonInfo& P)
					{
						if (P.Index == Person && Out.IsNull() &&
							P.State == static_cast<uint8>(Population::LifeState::Alive))
						{
							Out = H;
						}
					});
			return Out;
		}
	} // namespace

	PlayerTypes PlayerTypes::Declare(World& W)
	{
		PlayerTypes T;
		T.Mark = W.Types().Register<PlayerMark>("PlayerMark");
		W.Components().CreatePool(T.Mark);
		return T;
	}

	bool TakePlayer(World& W, const Population::PersonTypes& Persons, const PlayerTypes& Player, uint32 Person,
					SimTick Now, const Population::LodTypes* Lod)
	{
		if (PlayerPerson(W, Player) != 0)
		{
			return false; // a world holds one player at a time
		}
		const EntityHandle H = LivingPerson(W, Persons, Person);
		if (H.IsNull())
		{
			return false;
		}
		PlayerMark Fresh;
		Fresh.Person = Person;
		Fresh.Since = Now;
		Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ PlayerSalt, static_cast<int32>(Person), 0);
		W.Components().GetPool(Player.Mark).Add(H, Fresh);
		if (Lod != nullptr)
		{
			// And hold them where they are: the crossings of 04.06 would
			// otherwise send them to a neighbour as a count, which ends a
			// played life without anybody dying.
			Population::HoldPerson(W, Persons, *Lod, Person, Person);
		}
		return true;
	}

	bool ReleasePlayer(World& W, const PlayerTypes& Player, const Population::PersonTypes* Persons,
					   const Population::LodTypes* Lod)
	{
		const uint32 Was = PlayerPerson(W, Player);
		std::vector<EntityHandle> Marked;
		W.Components().GetPool(Player.Mark).ForEach([&](EntityHandle H, const PlayerMark&) { Marked.push_back(H); });
		for (const EntityHandle H : Marked)
		{
			W.Components().GetPool(Player.Mark).Remove(H);
		}
		if (Was != 0 && Persons != nullptr && Lod != nullptr)
		{
			Population::FreePerson(W, *Persons, *Lod, Was); // the world may have them back
		}
		return !Marked.empty();
	}

	uint32 PlayerPerson(const World& W, const PlayerTypes& Player)
	{
		const PlayerMark* Found = PlayerOf(W, Player);
		return Found != nullptr ? Found->Person : 0u;
	}

	const PlayerMark* PlayerOf(const World& W, const PlayerTypes& Player)
	{
		const PlayerMark* Found = nullptr;
		W.Components()
			.GetPool(Player.Mark)
			.ForEach(
				[&](EntityHandle, const PlayerMark& M)
				{
					if (Found == nullptr || M.Person < Found->Person)
					{
						Found = &M; // the lowest index, so a broken world answers the same way twice
					}
				});
		return Found;
	}

	PlayerStats MeasurePlayer(const World& W, const Population::PersonTypes& Persons, const PlayerTypes& Player)
	{
		PlayerStats S;
		std::vector<PlayerMark> Marks;
		std::vector<uint32> OnPerson; // the index of the person the mark actually sits on
		W.Components()
			.GetPool(Player.Mark)
			.ForEach(
				[&](EntityHandle H, const PlayerMark& M)
				{
					Marks.push_back(M);
					const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					OnPerson.push_back(P != nullptr ? P->Index : 0u);
					if (P != nullptr && P->State == static_cast<uint8>(Population::LifeState::Alive))
					{
						++S.Alive;
						S.Region = P->Region;
					}
				});
		S.Marks = static_cast<uint32>(Marks.size());
		if (S.Marks > 1)
		{
			S.Bad += S.Marks - 1u; // a world holds one player at a time
		}
		Hash64 D = HashString("Player");
		for (usize i = 0; i < Marks.size(); ++i)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Marks[i]), sizeof(PlayerMark)));
			if (Marks[i].Person == 0 || OnPerson[i] == 0)
			{
				++S.Bad; // a mark on nothing, or on something that is not a person
				continue;
			}
			if (Marks[i].Person != OnPerson[i])
			{
				++S.Bad; // the mark and the person it sits on disagree on who it is
			}
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Player
