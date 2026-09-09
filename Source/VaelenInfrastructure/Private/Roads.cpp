// VAELEN - VaelenInfrastructure
// Phase 09.04: roads.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/Roads.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Infrastructure
{
	namespace
	{
		constexpr uint64 RoadSalt = 0x524f414453ull; // "ROADS"
		constexpr uint32 G_TIMBER = static_cast<uint32>(Economy::Good::Timber);
	} // namespace

	uint32 RoadWorth(const RoadInfo& Road, const RoadRules& Rules) noexcept
	{
		const uint64 Raw = uint64{Road.Grade} * uint64{Rules.EasePerGrade} * uint64{Road.Repair} / 1000u;
		return static_cast<uint32>(Raw);
	}

	RoadTypes RoadTypes::Declare(World& W)
	{
		RoadTypes T;
		T.Road = W.Types().Register<RoadInfo>("RoadInfo");
		T.Ease = W.Types().Register<Economy::RouteEase>("RouteEase");
		W.Components().CreatePool(T.Road);
		W.Components().CreatePool(T.Ease);
		return T;
	}

	void RoadSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;

		// 1. The regions, by index: their common stock and the hands they can spare.
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
		std::vector<uint64> Spare(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const History::RegionPopulation* Counts =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
			const uint64 People = Counts != nullptr ? Counts->Total : 0u;
			Spare[R] = People >= Rules.PeopleToWork ? People * Rules.SparePerMille / 1000u : 0u;
		}

		// What the two ends of a road can pay together, half each, and the hands
		// they can put on it. A road is never one region's.
		auto CanPay = [&](uint32 A, uint32 B, uint32 Timber, uint32 Hands) -> bool
		{
			if (A == 0 || B == 0 || A >= N || B >= N || RegionHandles[A].IsNull() || RegionHandles[B].IsNull())
			{
				return false;
			}
			const uint32 Half = (Timber + 1u) / 2u;
			const Economy::RegionStock* SA = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[A]);
			const Economy::RegionStock* SB = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[B]);
			if (SA == nullptr || SB == nullptr || SA->Amount[G_TIMBER] < Half || SB->Amount[G_TIMBER] < Half)
			{
				return false;
			}
			const uint64 Each = (uint64{Hands} + 1u) / 2u;
			return Spare[A] >= Each && Spare[B] >= Each;
		};
		auto Pay = [&](uint32 A, uint32 B, uint32 Timber, uint32 Hands, PersistentId Cause)
		{
			const uint32 Half = (Timber + 1u) / 2u;
			for (const uint32 End : {A, B})
			{
				VAELEN_ENSURE(Economy::AddStock(W, Types, Families, Economy, End, 0, Economy::Good::Timber,
												-static_cast<int32>(Half), Context.Tick, Cause) == Half);
			}
			const uint64 Each = (uint64{Hands} + 1u) / 2u;
			Spare[A] -= std::min(Spare[A], Each);
			Spare[B] -= std::min(Spare[B], Each);
		};

		// 2. Every route of 06.04, in the order it opened.
		struct Way
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 From = 0;
			uint32 To = 0;
			uint64 Carried = 0;
			bool Open = false;
		};
		std::vector<Way> Ways;
		W.Components()
			.GetPool(Trade.Route)
			.ForEach([&](EntityHandle H, const Economy::RouteInfo& R)
					 { Ways.push_back(Way{H, R.Index, R.From, R.To, R.Carried, R.Closed == 0}); });
		std::sort(Ways.begin(), Ways.end(), [](const Way& A, const Way& B) { return A.Index < B.Index; });

		for (const Way& Route : Ways)
		{
			RoadInfo* Road = W.Components().GetPool(Roads.Road).TryGet(Route.Handle);
			const uint64 ThisYear = Road != nullptr ? Route.Carried - std::min(Route.Carried, Road->Seen) : 0u;

			// A route nothing has been made of yet: cut a first grade only where
			// enough already crosses to be worth the timber.
			if (Road == nullptr)
			{
				if (!Route.Open || Route.Carried < Rules.TrafficPerGrade ||
					!CanPay(Route.From, Route.To, Rules.TimberPerGrade, Rules.HandsPerGrade))
				{
					continue;
				}
				RoadInfo Fresh;
				Fresh.Route = Route.Index;
				Fresh.From = Route.From;
				Fresh.To = Route.To;
				Fresh.Grade = 1;
				Fresh.Repair = 1000;
				Fresh.Timber = (Rules.TimberPerGrade + 1u) / 2u * 2u; // half each, rounded up
				Fresh.Hands = Rules.HandsPerGrade;
				Fresh.Cut = Context.Tick;
				Fresh.Seen = Route.Carried;
				Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ RoadSalt, static_cast<int32>(Route.Index),
													static_cast<int32>(Route.From));
				W.Components().GetPool(Roads.Road).Add(Route.Handle, Fresh);
				const PersistentId Cause = Context.Events->Publish(Context.Tick, RoadCutEvent,
																   RoadPayload{Route.Index, Route.From, Route.To, 1u},
																   W.Entities().GetId(Route.Handle));
				Pay(Route.From, Route.To, Rules.TimberPerGrade, Rules.HandsPerGrade, Cause);
				Road = W.Components().GetPool(Roads.Road).TryGet(Route.Handle);
				if (Road == nullptr)
				{
					continue;
				}
			}
			else
			{
				// Keeping it: a little timber a year, or it wears. A road with
				// nothing left to wear and nobody minding it loses a grade, and
				// at grade zero it is a track again.
				const uint32 Upkeep = Rules.UpkeepPerGrade * Road->Grade;
				const bool Kept = Road->Grade > 0 && Route.Open && CanPay(Route.From, Route.To, Upkeep, 0u);
				if (Kept)
				{
					Pay(Route.From, Route.To, Upkeep, 0u, PersistentId{});
					Road->Timber += (Upkeep + 1u) / 2u * 2u;
					Road->Repair = std::min<uint32>(1000u, Road->Repair + Rules.MendPerYear);
					Road->Idle = 0;
				}
				else if (Road->Grade > 0)
				{
					Road->Repair = Road->Repair > Rules.WearPerYear ? Road->Repair - Rules.WearPerYear : 0u;
					++Road->Idle;
					if (Road->Repair == 0 && Road->Idle >= Rules.IdleBeforeLosing)
					{
						--Road->Grade;
						Road->Repair = Road->Grade > 0 ? 1000u : 0u;
						Road->Idle = 0;
						Context.Events->Publish(Context.Tick, RoadLostEvent,
												RoadPayload{Route.Index, Route.From, Route.To, Road->Grade},
												W.Entities().GetId(Route.Handle));
					}
				}

				// Raising it: only where the traffic of the year warrants the next
				// grade, and only out of what both ends can pay.
				const uint32 Next = Road->Grade + 1u;
				if (Route.Open && Road->Grade < Rules.MostGrade && Road->Repair >= 1000u &&
					ThisYear >= uint64{Rules.TrafficPerGrade} * Next &&
					CanPay(Route.From, Route.To, Rules.TimberPerGrade, Rules.HandsPerGrade))
				{
					Road->Grade = Next;
					Road->Timber += (Rules.TimberPerGrade + 1u) / 2u * 2u;
					Road->Hands += Rules.HandsPerGrade;
					Road->Repair = 1000;
					Road->Idle = 0;
					const PersistentId Cause = Context.Events->Publish(
						Context.Tick, RoadCutEvent, RoadPayload{Route.Index, Route.From, Route.To, Next},
						W.Entities().GetId(Route.Handle));
					Pay(Route.From, Route.To, Rules.TimberPerGrade, Rules.HandsPerGrade, Cause);
					Road = W.Components().GetPool(Roads.Road).TryGet(Route.Handle);
					if (Road == nullptr)
					{
						continue;
					}
				}
			}

			Road->Seen = Route.Carried;
			// 3. What it is worth to whatever crosses it, where trade already reads.
			const uint32 Worth = RoadWorth(*Road, Rules);
			Economy::RouteEase* Easier = W.Components().GetPool(Roads.Ease).TryGet(Route.Handle);
			if (Easier != nullptr)
			{
				Easier->CarryPerMille = Worth;
			}
			else if (Worth != 0)
			{
				W.Components().GetPool(Roads.Ease).Add(Route.Handle, Economy::RouteEase{Worth, 0u});
			}
		}
	}

	const RoadInfo* RoadOn(const World& W, const RoadTypes& Roads, uint32 Route)
	{
		const RoadInfo* Found = nullptr;
		if (Route == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Roads.Road)
			.ForEach(
				[&](EntityHandle, const RoadInfo& R)
				{
					if (R.Route == Route && Found == nullptr)
					{
						Found = &R;
					}
				});
		return Found;
	}

	const RoadInfo* RoadBetween(const World& W, const Economy::TradeTypes& Trade, const RoadTypes& Roads, uint32 A,
								uint32 B)
	{
		const RoadInfo* Found = nullptr;
		if (A == 0 || B == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle H, const Economy::RouteInfo& R)
				{
					const bool Joins = (R.From == A && R.To == B) || (R.From == B && R.To == A);
					if (Joins && Found == nullptr)
					{
						Found = W.Components().GetPool(Roads.Road).TryGet(H);
					}
				});
		return Found;
	}

	RoadStats MeasureRoads(const World& W, const Economy::TradeTypes& Trade, const RoadTypes& Roads,
						   const RoadRules& Rules)
	{
		RoadStats S;
		struct Row
		{
			RoadInfo Road;
			uint32 Ease = 0;
			bool HasEase = false;
			bool HasRoute = false;
			uint32 From = 0;
			uint32 To = 0;
		};
		std::vector<Row> Rows;
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle H, const Economy::RouteInfo& R)
				{
					const RoadInfo* Road = W.Components().GetPool(Roads.Road).TryGet(H);
					if (Road == nullptr)
					{
						return;
					}
					Row Out;
					Out.Road = *Road;
					Out.HasRoute = true;
					Out.From = R.From;
					Out.To = R.To;
					const Economy::RouteEase* Easier = W.Components().GetPool(Roads.Ease).TryGet(H);
					Out.HasEase = Easier != nullptr;
					Out.Ease = Easier != nullptr ? Easier->CarryPerMille : 0u;
					Rows.push_back(Out);
				});
		// A road on something that is not a route at all would never be seen by
		// the walk above, so count the pool and compare.
		uint32 Pool = 0;
		W.Components().GetPool(Roads.Road).ForEach([&](EntityHandle, const RoadInfo&) { ++Pool; });
		S.Bad += Pool > Rows.size() ? Pool - static_cast<uint32>(Rows.size()) : 0u;

		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Road.Route < B.Road.Route; });
		Hash64 D = HashString("Roads");
		uint32 Previous = 0;
		for (const Row& R : Rows)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.Road), sizeof(R.Road)));
			D = HashCombine(D, HashUInt64(R.Ease));
			++S.Roads;
			if (R.Road.Route == 0 || R.Road.Route == Previous)
			{
				++S.Bad; // a route with two roads, or a road on no route
			}
			Previous = R.Road.Route;
			if (R.Road.From != R.From || R.Road.To != R.To)
			{
				++S.Bad; // joining ends its route does not join
			}
			if (R.Road.Grade > Rules.MostGrade || R.Road.Repair > 1000u)
			{
				++S.Bad; // past its cap, or better than sound
			}
			if ((R.Road.Grade == 0) != (R.Road.Repair == 0))
			{
				++S.Bad; // a track in repair, or a road with none
			}
			const uint32 Worth = RoadWorth(R.Road, Rules);
			if (R.HasEase ? R.Ease != Worth : Worth != 0)
			{
				++S.Bad; // what trade reads is not what the road is worth
			}
			S.Timber += R.Road.Timber;
			S.Hands += R.Road.Hands;
			if (R.Road.Grade == 0)
			{
				++S.Tracks;
				continue;
			}
			++S.Made;
			S.Grades += R.Road.Grade;
			S.Best = std::max(S.Best, R.Road.Grade);
		}

		for (const Event& E : W.Log().All())
		{
			S.Cuttings += E.Is(RoadCutEvent) ? 1u : 0u;
			S.Losses += E.Is(RoadLostEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Infrastructure
