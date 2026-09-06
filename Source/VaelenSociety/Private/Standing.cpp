// VAELEN - VaelenSociety
// Phase 05.02: standing.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic tests in Tests/Society

#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"

#include <algorithm>

namespace Vaelen::Society
{
	using Population::LifeState;
	using Population::PersonInfo;

	namespace
	{
		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
			uint32 Score = 0;
			uint8 Offices = 0;
		};

		bool IsAlive(const PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(LifeState::Alive);
		}
	} // namespace

	StandingTypes StandingTypes::Declare(World& W)
	{
		StandingTypes T;
		T.Standing = W.Types().Register<PersonStanding>("PersonStanding");
		W.Components().CreatePool(T.Standing);
		return T;
	}

	uint32 StandingScore(const PersonInfo& P, const Population::PersonTraits* T, uint32 HouseMembers, uint8 Offices,
						 uint64 Tick, const StandingRules& Rules) noexcept
	{
		uint32 Score = std::min(HouseMembers, Rules.HouseMembersCap) * Rules.HousePointsPerMember;
		Score += (Offices & static_cast<uint8>(Office::HeadOfHouse)) != 0 ? Rules.HeadOfHousePoints : 0u;
		Score += (Offices & static_cast<uint8>(Office::Seat)) != 0 ? Rules.SeatPoints : 0u;
		Score += (Offices & static_cast<uint8>(Office::HeadOfSeat)) != 0 ? Rules.HeadOfSeatPoints : 0u;
		const uint32 Age = Population::AgeYears(P, Tick);
		const uint32 Band = Age < 30 ? 0u : (Age < 45 ? 1u : (Age < 60 ? 2u : 3u));
		Score += Rules.AgeBandPoints[Band];
		if (T != nullptr)
		{
			const uint32 Charm = T->Traits[static_cast<uint32>(Population::Trait::Charm)];
			const uint32 Will = T->Traits[static_cast<uint32>(Population::Trait::Will)];
			Score += (Charm + Will) / 2u * Rules.TraitPointsPerMille / 1000u;
			uint32 Best = 0;
			for (uint32 S = 0; S < static_cast<uint32>(Population::Skill::Count); ++S)
			{
				Best = std::max(Best, uint32{T->Skills[S]});
			}
			Score += Best * Rules.SkillPointsPerMille / 1000u;
		}
		return Score;
	}

	void StandingSystem::Tick(TickContext& Context)
	{
		World& W = *Owner;
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const Population::RegionDetail& D) { Regions.push_back(D.Region); });
		std::sort(Regions.begin(), Regions.end());
		if (Regions.empty())
		{
			return;
		}
		// Standings of the dead, the gone, the young and the coarse are dropped.
		std::vector<EntityHandle> Stale;
		W.Components()
			.GetPool(Standing.Standing)
			.ForEach(
				[&](EntityHandle H, const PersonStanding&)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (P == nullptr || !IsAlive(*P) || Population::AgeYears(*P, Context.Tick) < Rules.AdultFrom ||
						std::find(Regions.begin(), Regions.end(), P->Region) == Regions.end() ||
						(HasBonds && W.Components().GetPool(Bonds).TryGet(H) != nullptr))
					{
						Stale.push_back(H);
					}
				});
		std::sort(Stale.begin(), Stale.end(), [](EntityHandle A, EntityHandle B) { return A.Index() < B.Index(); });
		for (const EntityHandle H : Stale)
		{
			W.Components().GetPool(Standing.Standing).Remove(H);
		}
		// Heads of houses and house sizes, once.
		std::vector<uint32> HeadOf; // family index -> head person index
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle, const Population::FamilyInfo& F)
				{
					if (F.Index >= HeadOf.size())
					{
						HeadOf.resize(usize{F.Index} + 1u, 0u);
					}
					HeadOf[F.Index] = F.Head;
				});
		// Organisation heads, once.
		std::vector<uint32> OrgHead;
		W.Components()
			.GetPool(Organizations.Organization)
			.ForEach(
				[&](EntityHandle, const OrganizationInfo& O)
				{
					if (O.Index >= OrgHead.size())
					{
						OrgHead.resize(usize{O.Index} + 1u, 0u);
					}
					OrgHead[O.Index] = O.Disbanded == 0 ? O.Head : 0u;
				});
		for (const uint32 Region : Regions)
		{
			std::vector<Ref> Adults;
			std::vector<uint32> HouseSize;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region != Region || !IsAlive(P))
						{
							return;
						}
						if (P.Family != 0)
						{
							if (P.Family >= HouseSize.size())
							{
								HouseSize.resize(usize{P.Family} + 1u, 0u);
							}
							++HouseSize[P.Family];
						}
						if (Population::AgeYears(P, Context.Tick) >= Rules.AdultFrom &&
							!(HasBonds && W.Components().GetPool(Bonds).TryGet(H) != nullptr))
						{
							Adults.push_back(Ref{H, P, 0, 0});
						}
					});
			std::sort(Adults.begin(), Adults.end(),
					  [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
			for (Ref& R : Adults)
			{
				uint8 Offices = 0;
				if (R.Info.Family != 0 && R.Info.Family < HeadOf.size() && HeadOf[R.Info.Family] == R.Info.Index)
				{
					Offices |= static_cast<uint8>(Office::HeadOfHouse);
				}
				const Membership* M = W.Components().GetPool(Organizations.Member).TryGet(R.Handle);
				if (M != nullptr)
				{
					Offices |= static_cast<uint8>(Office::Seat);
					if (M->Organization < OrgHead.size() && OrgHead[M->Organization] == R.Info.Index)
					{
						Offices |= static_cast<uint8>(Office::HeadOfSeat);
					}
				}
				const Population::PersonTraits* T = W.Components().GetPool(Traits.Traits).TryGet(R.Handle);
				const uint32 Members =
					R.Info.Family != 0 && R.Info.Family < HouseSize.size() ? HouseSize[R.Info.Family] : 0u;
				R.Offices = Offices;
				R.Score = StandingScore(R.Info, T, Members, Offices, Context.Tick, Rules);
			}
			// Rank: position in the score order (ties by index), scaled to 0..255; tiers by share.
			std::vector<usize> Order(Adults.size());
			for (usize i = 0; i < Order.size(); ++i)
			{
				Order[i] = i;
			}
			std::sort(Order.begin(), Order.end(),
					  [&](usize A, usize B)
					  {
						  return Adults[A].Score != Adults[B].Score ? Adults[A].Score > Adults[B].Score
																	: Adults[A].Info.Index < Adults[B].Info.Index;
					  });
			const usize N = Order.size();
			const usize EliteCount = N * Rules.ElitePerMille / 1000u;
			const usize NotableCount = N * Rules.NotablePerMille / 1000u;
			for (usize Position = 0; Position < N; ++Position)
			{
				const Ref& R = Adults[Order[Position]];
				PersonStanding S;
				S.Score = R.Score;
				S.Rank = static_cast<uint8>(N > 1 ? 255u - (Position * 255u) / (N - 1) : 255u);
				S.Tier_ = static_cast<uint8>(
					Position < EliteCount ? Tier::Elite
										  : (Position < EliteCount + NotableCount ? Tier::Notable : Tier::Common));
				S.Offices = R.Offices;
				PersonStanding* Existing = W.Components().GetPool(Standing.Standing).TryGet(R.Handle);
				if (Existing != nullptr)
				{
					*Existing = S;
				}
				else
				{
					W.Components().GetPool(Standing.Standing).Add(R.Handle, S);
				}
			}
		}
	}

	void EliteOf(const World& W, const Population::PersonTypes& Persons, const StandingTypes& Types, uint32 Region,
				 std::vector<uint32>& Out, uint32 MaxCount)
	{
		Out.clear();
		std::vector<std::pair<uint8, uint32>> Ranked;
		W.Components()
			.GetPool(Types.Standing)
			.ForEach(
				[&](EntityHandle H, const PersonStanding& S)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (P != nullptr && P->Region == Region && IsAlive(*P) &&
						(MaxCount != 0 || S.Tier_ == static_cast<uint8>(Tier::Elite)))
					{
						Ranked.push_back({S.Rank, P->Index});
					}
				});
		std::sort(Ranked.begin(), Ranked.end(), [](const auto& A, const auto& B)
				  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
		for (const auto& [Rank, Index] : Ranked)
		{
			if (MaxCount != 0 && Out.size() >= MaxCount)
			{
				break;
			}
			Out.push_back(Index);
		}
	}

	const PersonStanding* StandingOf(const World& W, const Population::PersonTypes& Persons, const StandingTypes& Types,
									 uint32 Person)
	{
		const PersonStanding* Found = nullptr;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == Person && Found == nullptr)
					{
						Found = W.Components().GetPool(Types.Standing).TryGet(H);
					}
				});
		return Found;
	}

	StandingStats MeasureStanding(const World& W, const Population::PersonTypes& Persons, const StandingTypes& Types,
								  uint32 Region)
	{
		StandingStats S;
		struct Row
		{
			uint32 Index;
			PersonStanding Standing;
		};
		std::vector<Row> Rows;
		const StandingRules Rules;
		W.Components()
			.GetPool(Types.Standing)
			.ForEach(
				[&](EntityHandle H, const PersonStanding& St)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (P == nullptr)
					{
						++S.Stale;
						return;
					}
					Rows.push_back(Row{P->Index, St});
					if (!IsAlive(*P) || Population::AgeYears(*P, W.Now()) < Rules.AdultFrom)
					{
						++S.Stale;
						return;
					}
					if (Region != 0 && P->Region != Region)
					{
						return;
					}
					++S.Ranked;
					const uint32 T = St.Tier_ < 3 ? St.Tier_ : 0u;
					++S.PerTier[T];
					S.ScoreSum[T] += St.Score;
					S.WithOffice += St.Offices != 0 ? 1u : 0u;
				});
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Index < B.Index; });
		Hash64 D = HashString("PersonStanding");
		for (const Row& R : Rows)
		{
			D = HashCombine(D, HashUInt64(R.Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.Standing), sizeof(R.Standing)));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Society
