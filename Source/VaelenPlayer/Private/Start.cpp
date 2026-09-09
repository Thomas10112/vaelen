// VAELEN - VaelenPlayer
// Phase 10.02: the enslaved start.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player

#include "Vaelen/Player/Start.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Player
{
	namespace
	{
		bool IsOre(uint32 Kind) noexcept
		{
			return Kind == static_cast<uint32>(WorldGen::ResourceKind::IronOre) ||
				   Kind == static_cast<uint32>(WorldGen::ResourceKind::CopperOre);
		}

		/// The regions with ore under them, sorted. Empty when the world was
		/// generated without deposits at all, which the caller reads as "every
		/// region will do" rather than "no region will".
		void RegionsWithOre(const World& W, const History::PreHistoryTypes& Types, std::vector<uint32>& Out)
		{
			Out.clear();
			W.Components()
				.GetPool(Types.World.DepositTypes_.Deposit)
				.ForEach(
					[&](EntityHandle, const WorldGen::DepositInfo& D)
					{
						if (D.Region != 0 && IsOre(D.Kind))
						{
							Out.push_back(D.Region);
						}
					});
			std::sort(Out.begin(), Out.end());
			Out.erase(std::unique(Out.begin(), Out.end()), Out.end());
		}
	} // namespace

	StartTypes StartTypes::Declare(World& W)
	{
		StartTypes T;
		T.Start = W.Types().Register<PlayerStart>("PlayerStart");
		W.Components().CreatePool(T.Start);
		return T;
	}

	uint32 BeginEnslaved(World& W, const History::PreHistoryTypes& Types, const Population::PersonTypes& Persons,
						 const Society::BondageTypes& Bondage, const Society::StandingTypes& Standing,
						 const PlayerTypes& Player, const StartTypes& Start, const StartRules& Rules, SimTick Now,
						 const Population::LodTypes* Lod)
	{
		if (PlayerPerson(W, Player) != 0 || StartOf(W, Start) != nullptr)
		{
			return 0; // a life is taken up once
		}
		std::vector<uint32> Ore;
		RegionsWithOre(W, Types, Ore);
		const bool WantsOre = Rules.PreferOre != 0 && !Ore.empty();

		// Everybody the world offers, in person order so that the same world
		// always hands over the same life.
		struct Candidate
		{
			uint32 Person = 0;
			EntityHandle Handle;
			uint32 Region = 0;
			uint32 Family = 0;
			uint32 Age = 0;
			uint8 OnOre = 0;
		};
		std::vector<Candidate> Offered;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.State != static_cast<uint8>(Population::LifeState::Alive) || P.Born > Now)
					{
						return;
					}
					const uint32 Age = static_cast<uint32>((Now - P.Born) / History::TicksPerYear);
					if (Age < Rules.FromAge || Age > Rules.ToAge)
					{
						return;
					}
					const bool OnOre = WantsOre && std::binary_search(Ore.begin(), Ore.end(), P.Region);
					if (Rules.WantBound != 0)
					{
						const Society::BondState* Bond = W.Components().GetPool(Bondage.Bond).TryGet(H);
						if (Bond == nullptr || Bond->Kind == static_cast<uint8>(Society::BondKind::Free))
						{
							return; // a free life is not the life this start is
						}
					}
					Offered.push_back(Candidate{P.Index, H, P.Region, P.Family, Age, OnOre ? uint8{1} : uint8{0}});
				});
		if (Offered.empty())
		{
			return 0; // the world offers nobody; nothing is invented and nothing is written
		}
		// Ore ground first where the world offers a life on it, then the lowest
		// person index, so that the same world always hands over the same life.
		std::sort(Offered.begin(), Offered.end(), [](const Candidate& A, const Candidate& B)
				  { return A.OnOre != B.OnOre ? A.OnOre > B.OnOre : A.Person < B.Person; });
		const Candidate& Taken = Offered.front();
		if (!TakePlayer(W, Persons, Player, Taken.Person, Now, Lod))
		{
			return 0;
		}

		PlayerStart Record;
		Record.Person = Taken.Person;
		Record.Region = Taken.Region;
		Record.Family = Taken.Family;
		Record.Age = Taken.Age;
		Record.OnOre = Taken.OnOre;
		Record.Began = Now;
		const Society::BondState* Bond = W.Components().GetPool(Bondage.Bond).TryGet(Taken.Handle);
		Record.Bond = Bond != nullptr ? Bond->Kind : static_cast<uint32>(Society::BondKind::Free);
		Record.Holder = Bond != nullptr ? Bond->Holder : 0u;
		const Society::PersonStanding* Rank = W.Components().GetPool(Standing.Standing).TryGet(Taken.Handle);
		Record.Standing = Rank != nullptr ? Rank->Score : 0u;
		W.Components().GetPool(Start.Start).Add(Taken.Handle, Record);
		return Taken.Person;
	}

	bool EndStart(World& W, const StartTypes& Start)
	{
		std::vector<EntityHandle> Held;
		W.Components().GetPool(Start.Start).ForEach([&](EntityHandle H, const PlayerStart&) { Held.push_back(H); });
		for (const EntityHandle H : Held)
		{
			W.Components().GetPool(Start.Start).Remove(H);
		}
		return !Held.empty();
	}

	const PlayerStart* StartOf(const World& W, const StartTypes& Start)
	{
		const PlayerStart* Found = nullptr;
		W.Components()
			.GetPool(Start.Start)
			.ForEach(
				[&](EntityHandle, const PlayerStart& S)
				{
					if (Found == nullptr || S.Person < Found->Person)
					{
						Found = &S;
					}
				});
		return Found;
	}

	StartStats MeasureStart(const World& W, const Population::PersonTypes& Persons,
							const Society::BondageTypes& Bondage, const PlayerTypes& Player, const StartTypes& Start)
	{
		StartStats S;
		const uint32 Played = PlayerPerson(W, Player);
		std::vector<PlayerStart> All;
		std::vector<uint32> OnPerson;
		std::vector<uint8> StillBound;
		W.Components()
			.GetPool(Start.Start)
			.ForEach(
				[&](EntityHandle H, const PlayerStart& Record)
				{
					All.push_back(Record);
					const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					OnPerson.push_back(P != nullptr ? P->Index : 0u);
					const Society::BondState* Bond = W.Components().GetPool(Bondage.Bond).TryGet(H);
					StillBound.push_back(Bond != nullptr && Bond->Kind != static_cast<uint8>(Society::BondKind::Free)
											 ? uint8{1}
											 : uint8{0});
				});
		S.Started = static_cast<uint32>(All.size());
		if (S.Started > 1)
		{
			S.Bad += S.Started - 1u; // a life is taken up once
		}
		Hash64 D = HashString("PlayerStart");
		for (usize i = 0; i < All.size(); ++i)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&All[i]), sizeof(PlayerStart)));
			if (All[i].Person == 0 || All[i].Person != OnPerson[i])
			{
				++S.Bad; // the record and the person it sits on disagree on who it is
				continue;
			}
			if (Played != 0 && All[i].Person != Played)
			{
				++S.Bad; // a start on somebody who is not the one being played
			}
			S.StillBound += StillBound[i];
			S.WithFamily += All[i].Family != 0 ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Player
