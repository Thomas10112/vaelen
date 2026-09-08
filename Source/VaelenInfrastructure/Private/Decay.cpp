// VAELEN - VaelenInfrastructure
// Phase 09.05: decay and ruins.
//
// STATUS: PROTOTYPE (Phase 09) - unit/long-duration/edge tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/Decay.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Infrastructure
{
	namespace
	{
		constexpr uint32 G_TIMBER = static_cast<uint32>(Economy::Good::Timber);
	} // namespace

	void DecaySystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		std::vector<EntityHandle> RegionHandles;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= RegionHandles.size())
					{
						RegionHandles.resize(usize{R.Index} + 1u);
					}
					RegionHandles[R.Index] = H;
				});
		const usize N = RegionHandles.size();
		if (N <= 1)
		{
			return;
		}

		// 1. What the year did to every region: the weather that breaks things
		//    (a flood or an eruption, never a drought or a plague - those kill
		//    people, not walls) and the war that stood on it.
		std::vector<uint32> Extra(N, 0u);
		// Taken off the log rather than the record, because a fall wants to name
		// the blow that finished it: the why of a fallen mill is the flood.
		std::vector<PersistentId> Blame(N);
		{
			const std::vector<Event>& All = W.Log().All();
			for (usize i = All.size(); i > 0; --i)
			{
				const Event& E = All[i - 1];
				// This year's blows only. The system runs after Disasters (see
				// GetDependencies), so a blow of this year carries this tick.
				if (E.Tick != Context.Tick)
				{
					break;
				}
				if (!E.Is(History::DisasterStruckEvent))
				{
					continue;
				}
				const History::DisasterPayload P = E.Get<History::DisasterPayload>();
				if (P.Region == 0 || P.Region >= N || P.Severity == 0)
				{
					continue;
				}
				const bool Breaks = P.Kind == static_cast<uint32>(History::DisasterKind::Flood) ||
									P.Kind == static_cast<uint32>(History::DisasterKind::Eruption);
				if (!Breaks)
				{
					continue;
				}
				const uint32 S = P.Severity > 3 ? 2u : P.Severity - 1u;
				Extra[P.Region] += Rules.StormWear[S];
				Blame[P.Region] = E.Id; // the last one read is the earliest of the year
			}
		}

		if (HasWar)
		{
			std::vector<uint32> RuledBy(N, 0u);
			for (uint32 R = 1; R < N; ++R)
			{
				if (RegionHandles[R].IsNull())
				{
					continue;
				}
				const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
				RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
			}
			W.Components()
				.GetPool(Armies.Army)
				.ForEach(
					[&](EntityHandle, const Military::ArmyInfo& A)
					{
						if (A.Disbanded != 0 || A.Strength == 0 || A.Region == 0 || A.Region >= N)
						{
							return;
						}
						if (A.Polity != RuledBy[A.Region])
						{
							Extra[A.Region] += Rules.WarWear; // a host that is not the region's own
						}
					});
			for (uint32 R = 1; R < N; ++R)
			{
				if (RegionHandles[R].IsNull())
				{
					continue;
				}
				const Military::SiegeInfo* Walls = W.Components().GetPool(Sieges.Siege).TryGet(RegionHandles[R]);
				if (Walls != nullptr && Walls->Besieger != 0)
				{
					Extra[R] += Rules.SiegeWear;
				}
			}
		}

		// 2. Every standing work, in the order it was raised: what the region
		//    could pay to keep it, then what the year took, then whether that
		//    finished it.
		std::vector<std::pair<uint32, EntityHandle>> All;
		W.Components()
			.GetPool(Buildings.Building)
			.ForEach([&](EntityHandle H, const BuildingInfo& B) { All.push_back({B.Index, H}); });
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		for (const auto& [Index, H] : All)
		{
			BuildingInfo* Work_ = W.Components().GetPool(Buildings.Building).TryGet(H);
			if (Work_ == nullptr || Work_->Fell != 0 || Work_->Region == 0 || Work_->Region >= N ||
				Work_->Kind >= WorkCount || RegionHandles[Work_->Region].IsNull())
			{
				continue;
			}
			const uint32 Kind = Work_->Kind;
			const uint32 Upkeep = Rules.UpkeepPerSize[Kind] * Work_->Size;
			Economy::RegionStock* Stock = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[Work_->Region]);
			// The year takes its toll first, then the region mends what it paid
			// for: a work that is kept every year reaches full repair and stays
			// there, and one that is not falls at the rate of what it is.
			const uint64 Wear = uint64{Rules.WearPerYear[Kind]} + Extra[Work_->Region];
			Work_->Repair = Work_->Repair > Wear ? Work_->Repair - static_cast<uint32>(Wear) : 0u;
			const bool Kept = Upkeep > 0 && Stock != nullptr && Stock->Amount[G_TIMBER] >= Upkeep;
			if (Kept)
			{
				VAELEN_ENSURE(Economy::AddStock(W, Types, Families, Economy, Work_->Region, 0, Economy::Good::Timber,
												-static_cast<int32>(Upkeep), Context.Tick) == Upkeep);
				Work_ = W.Components().GetPool(Buildings.Building).TryGet(H);
				if (Work_ == nullptr)
				{
					continue;
				}
				Work_->Repair = std::min<uint32>(1000u, Work_->Repair + Rules.MendPerYear);
			}
			if (Work_->Repair != 0)
			{
				continue;
			}
			// It is finished. The ruin stays where it stood.
			Work_->Fell = Context.Tick;
			Context.Events->Publish(Context.Tick, BuildingFellEvent,
									WorksPayload{Work_->Region, Work_->Index, Kind, Work_->Size}, W.Entities().GetId(H),
									Blame[Work_->Region]);
		}
	}

	void RuinsIn(const World& W, const InfrastructureTypes& Buildings, uint32 Region, Work Kind,
				 std::vector<uint32>& Out)
	{
		Out.clear();
		const uint32 K = static_cast<uint32>(Kind);
		if (Region == 0 || K >= WorkCount)
		{
			return;
		}
		W.Components()
			.GetPool(Buildings.Building)
			.ForEach(
				[&](EntityHandle, const BuildingInfo& B)
				{
					if (B.Region == Region && B.Kind == K && B.Fell != 0)
					{
						Out.push_back(B.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	bool HasRuin(const World& W, const InfrastructureTypes& Buildings, uint32 Region, Work Kind)
	{
		std::vector<uint32> Out;
		RuinsIn(W, Buildings, Region, Kind, Out);
		return !Out.empty();
	}

	DecayStats MeasureDecay(const World& W, const InfrastructureTypes& Buildings, const DecayRules& Rules)
	{
		DecayStats S;
		(void)Rules;
		std::vector<BuildingInfo> All;
		W.Components()
			.GetPool(Buildings.Building)
			.ForEach([&](EntityHandle, const BuildingInfo& B) { All.push_back(B); });
		std::sort(All.begin(), All.end(),
				  [](const BuildingInfo& A, const BuildingInfo& B) { return A.Index < B.Index; });

		// A work raised on ground that already held a ruin of its kind: the ruin
		// is older than the work, on the same region, of the same kind.
		Hash64 D = HashString("Decay");
		for (const BuildingInfo& B : All)
		{
			D = HashCombine(D, HashCombine(HashUInt64(B.Index), HashUInt64(B.Repair)));
			if (B.Fell != 0)
			{
				++S.Fallen;
				S.Bad += B.Repair != 0 ? 1u : 0u; // a ruin cannot be in repair
				continue;
			}
			++S.Standing;
			if (B.Repair == 0)
			{
				++S.Bad; // a work standing with nothing left of it
				continue;
			}
			S.Sound += B.Repair >= 1000u ? 1u : 0u;
			S.Worn += B.Repair < 1000u ? 1u : 0u;
			S.Least = std::min(S.Least, B.Repair);
			for (const BuildingInfo& Older : All)
			{
				if (Older.Fell != 0 && Older.Region == B.Region && Older.Kind == B.Kind && Older.Index < B.Index)
				{
					++S.Rebuilt;
					break;
				}
			}
		}
		if (S.Standing == 0)
		{
			S.Least = 0;
		}
		for (const Event& E : W.Log().All())
		{
			S.Falls += E.Is(BuildingFellEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Infrastructure
