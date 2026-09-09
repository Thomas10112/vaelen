// VAELEN - VaelenColony
// Phase 11 task 11.03: the work of a colony.
//
// STATUS: PROTOTYPE (Phase 11) - unit/integration/deterministic/edge tests in Tests/Colony
#include "Vaelen/Colony/Mining.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <vector>

namespace Vaelen::Colony
{
	namespace
	{
		constexpr uint32 G_ORE = static_cast<uint32>(Economy::Good::Ore);
		constexpr uint32 SKILL_CRAFT = static_cast<uint32>(Population::Skill::Craft);

		bool IsOre(uint32 Kind) noexcept
		{
			return Kind == static_cast<uint32>(WorldGen::ResourceKind::IronOre) ||
				   Kind == static_cast<uint32>(WorldGen::ResourceKind::CopperOre);
		}

		/// An ore seam of the colony: the deposit entity, its index, and what it
		/// held when the world was made.
		struct Seam
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Richness = 0;
		};

		/// The ore seams of one region, in deposit-index order. Pools are dense
		/// and walked in insertion order, but the sort makes the order a fact of
		/// the data rather than of the container - which is what lets the taking
		/// be the same in every run of the same seed.
		std::vector<Seam> SeamsOf(const World& W, const History::PreHistoryTypes& Types, uint32 Region)
		{
			std::vector<Seam> Out;
			W.Components()
				.GetPool(Types.World.DepositTypes_.Deposit)
				.ForEach(
					[&](EntityHandle H, const WorldGen::DepositInfo& D)
					{
						if (D.Region == Region && IsOre(D.Kind))
						{
							Out.push_back(Seam{H, D.Index, D.Richness});
						}
					});
			std::sort(Out.begin(), Out.end(), [](const Seam& A, const Seam& B) { return A.Index < B.Index; });
			return Out;
		}

		/// What one seam has already given. Absent means nothing yet.
		uint32 TakenOf(const World& W, const ColonyTypes& Colony, EntityHandle H)
		{
			const DepositTaken* T = W.Components().GetPool(Colony.Taken).TryGet(H);
			return T == nullptr ? 0u : T->Taken;
		}
	} // namespace

	ColonyTypes ColonyTypes::Declare(World& W)
	{
		ColonyTypes T;
		T.Colony = W.Types().Register<ColonyInfo>("ColonyInfo");
		T.Taken = W.Types().Register<DepositTaken>("DepositTaken");
		T.Mined = Economy::DeclareMined(W);
		W.Components().CreatePool(T.Colony);
		W.Components().CreatePool(T.Taken);
		return T;
	}

	bool FoundColony(World& W, const History::PreHistoryTypes& Types, const ColonyTypes& Colony, uint32 Region)
	{
		if (Region == 0)
		{
			return false;
		}
		EntityHandle RH;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region)
					{
						RH = H;
					}
				});
		if (RH.IsNull() || W.Components().GetPool(Colony.Colony).TryGet(RH) != nullptr)
		{
			return false; // no such region, or it is a colony already
		}
		ColonyInfo Info;
		Info.Region = Region;
		W.Components().GetPool(Colony.Colony).Add(RH, Info);
		W.Components().GetPool(Colony.Mined).Add(RH, Economy::RegionMined{});
		return true;
	}

	uint32 SeamLeft(const World& W, const History::PreHistoryTypes& Types, const ColonyTypes& Colony, uint32 Region)
	{
		uint64 Left = 0;
		for (const Seam& S : SeamsOf(W, Types, Region))
		{
			const uint32 Taken = TakenOf(W, Colony, S.Handle);
			Left += S.Richness > Taken ? uint64{S.Richness - Taken} : 0u;
		}
		return Left > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32>(Left);
	}

	void MiningSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		if (Rules.Region == 0 || Rules.DaysPerYear == 0)
		{
			return; // no colony; nobody is on the rock
		}
		// The colony's region entity. The pool holds one, so this walks one -
		// which is why the system reads the colony pool and not the region pool:
		// it runs every day, and a daily pass over every region of the world
		// would be the cost 11.01 taught us to measure and not to pay.
		EntityHandle RH;
		W.Components()
			.GetPool(Colony.Colony)
			.ForEach(
				[&](EntityHandle H, const ColonyInfo& C)
				{
					if (C.Region == Rules.Region)
					{
						RH = H;
					}
				});
		if (RH.IsNull())
		{
			return;
		}
		// The hands, and what a year of them would lift. Mining has no skill of
		// its own: Skill::Count sizes PersonTraits, so a sixth skill would move
		// the component digest of every world ever made. The colony's hands use
		// the craft 06.02 already turns ore into tools with.
		uint32 Hands = 0;
		uint64 YearTotal = 0;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.Region != Rules.Region || P.State != static_cast<uint8>(Population::LifeState::Alive))
					{
						return;
					}
					const uint64 Age = Context.Tick > P.Born ? (Context.Tick - P.Born) / History::TicksPerYear : 0ull;
					if (Age < Rules.WorkFromAge)
					{
						return;
					}
					++Hands;
					uint32 Skill = 128; // ordinary, when nobody observed the traits
					if (HasTraits)
					{
						const Population::PersonTraits* T = W.Components().GetPool(Traits).TryGet(H);
						if (T != nullptr)
						{
							Skill = T->Skills[SKILL_CRAFT];
						}
					}
					const uint64 PerMille =
						uint64{Rules.SkillFloorPerMille} + uint64{Rules.SkillSpanPerMille} * Skill / 255u;
					YearTotal += uint64{Rules.PerHandPerYear} * PerMille / 1000u;
				});
		ColonyInfo* Info = W.Components().GetPool(Colony.Colony).TryGet(RH);
		if (Info == nullptr)
		{
			return;
		}
		Info->Hands = Hands;
		if (YearTotal == 0)
		{
			return; // nobody old enough, or a rule that lifts nothing
		}
		// The day's share of the year, by the schedule 11.02 proved sums exactly:
		// a year of days lifts what a year should, and the colony cannot drift
		// from the world around it by rounding.
		const uint64 Day = Context.Tick / 24u;
		uint32 Take = Population::ShareOfDay(static_cast<uint32>(Day % Rules.DaysPerYear), Rules.DaysPerYear,
											 YearTotal > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32>(YearTotal));
		if (Take == 0)
		{
			return;
		}
		// Taken seam by seam in deposit-index order, never more than a seam held.
		const std::vector<Seam> Seams = SeamsOf(W, Types, Rules.Region);
		uint32 Lifted = 0;
		const PersistentId Subject = W.Entities().GetId(RH);
		for (const Seam& S : Seams)
		{
			if (Take == 0)
			{
				break;
			}
			DepositTaken* T = W.Components().GetPool(Colony.Taken).TryGet(S.Handle);
			if (T == nullptr)
			{
				W.Components().GetPool(Colony.Taken).Add(S.Handle, DepositTaken{});
				T = W.Components().GetPool(Colony.Taken).TryGet(S.Handle);
				if (T == nullptr)
				{
					continue;
				}
			}
			const uint32 Left = S.Richness > T->Taken ? S.Richness - T->Taken : 0u;
			if (Left == 0)
			{
				continue;
			}
			const uint32 N = Left < Take ? Left : Take;
			T->Taken += N;
			Take -= N;
			Lifted += N;
			if (T->Taken >= S.Richness)
			{
				Context.Events->Publish(Context.Tick, SeamWorkedOutEvent,
										Economy::StockPayload{Rules.Region, S.Index, G_ORE, 0u}, Subject);
			}
		}
		if (Lifted == 0)
		{
			return; // every seam of the colony is worked out
		}
		Info->Lifted += Lifted;
		// Through AddStock and through nothing else, so the lift is in the log
		// with a cause and the chronicle of 11.07 can tell where the ore came
		// from - which 06.02's yearly extraction cannot, because it writes the
		// common stock directly.
		const PersistentId Lift = Context.Events->Publish(
			Context.Tick, OreLiftedEvent, Economy::StockPayload{Rules.Region, 0u, G_ORE, Lifted}, Subject);
		Economy::AddStock(W, Types, Families, Economy, Rules.Region, 0u, Economy::Good::Ore, static_cast<int32>(Lifted),
						  Context.Tick, Lift);
	}

	MiningStats MeasureMining(const World& W, const History::PreHistoryTypes& Types, const ColonyTypes& Colony,
							  uint32 Region)
	{
		MiningStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		W.Components()
			.GetPool(Colony.Colony)
			.ForEach(
				[&](EntityHandle, const ColonyInfo& C)
				{
					if (Region != 0 && C.Region != Region)
					{
						return;
					}
					++Out.Colonies;
					Out.Hands += C.Hands;
					Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&C), sizeof(ColonyInfo)));
				});
		const uint32 Where = Region;
		for (const Seam& S : SeamsOf(W, Types, Where))
		{
			const uint32 Taken = TakenOf(W, Colony, S.Handle);
			++Out.Seams;
			Out.Richness += S.Richness;
			Out.Taken += Taken;
			if (Taken >= S.Richness)
			{
				++Out.WorkedOut;
			}
			Digest = HashCombine(Digest, HashCombine(Hash64{S.Index}, Hash64{Taken}));
		}
		// Lifts are filtered the same way the seams are: a caller that asks about
		// one region gets that region's answer and not the world's.
		for (const Event& E : W.Log().All())
		{
			if (E.Is(OreLiftedEvent) && (Region == 0 || E.Get<Economy::StockPayload>().Region == Region))
			{
				++Out.Lifts;
			}
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Colony
