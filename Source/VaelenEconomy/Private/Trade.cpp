// VAELEN - VaelenEconomy
// Phase 06.04: trade and routes.
//
// STATUS: VALIDATED (Phase 06) - deterministic/long-duration tests in Tests/Economy

#include "Vaelen/Economy/Trade.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Economy
{
	namespace
	{
		constexpr uint64 RouteSalt = 0x524f555445ull;	   // "ROUTE"
		constexpr uint64 SettlementSalt = 0x5345544c45ull; // "SETLE"

		struct Route
		{
			EntityHandle Handle;
			RouteInfo Info;
		};

		struct Settlement
		{
			EntityHandle Handle;
			SettlementInfo Info;
		};

		uint32 Saturate(uint64 V) noexcept
		{
			return V > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(V);
		}
	} // namespace

	TradeTypes TradeTypes::Declare(World& W)
	{
		TradeTypes T;
		T.Route = W.Types().Register<RouteInfo>("RouteInfo");
		T.Settlement = W.Types().Register<SettlementInfo>("SettlementInfo");
		W.Components().CreatePool(T.Route);
		W.Components().CreatePool(T.Settlement);
		return T;
	}

	void TradeSystem::Tick(TickContext& Context)
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
		const Hash64 Key = HashBytes(reinterpret_cast<const char*>(&W.Map().Config()), sizeof(W.Map().Config()));
		if (Graph.Neighbours.empty() || GraphDigest != Key)
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Types.World.Regions);
			GraphDigest = Key;
		}
		// The people of every region (the living where detailed), what each holds
		// in all, and what each holds in common.
		std::vector<uint8> Detailed(N, 0u);
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach(
				[&](EntityHandle, const Population::RegionDetail& D)
				{
					if (D.Region < N)
					{
						Detailed[D.Region] = 1;
					}
				});
		std::vector<uint64> People(N, 0u);
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& P)
				{
					if (P.Region < N && Detailed[P.Region] != 0 &&
						P.State == static_cast<uint8>(Population::LifeState::Alive))
					{
						++People[P.Region];
					}
				});
		std::vector<uint64> Held(N * GoodCount, 0u);
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					const HouseStock* S = F.Region < N ? W.Components().GetPool(Economy.House).TryGet(H) : nullptr;
					if (S == nullptr)
					{
						return;
					}
					for (uint32 g = 0; g < GoodCount; ++g)
					{
						Held[usize{F.Region} * GoodCount + g] += S->Amount[g];
					}
				});
		std::vector<RegionStock*> Common(N, nullptr);
		std::vector<const RegionMarket*> Market(N, nullptr);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			Common[R] = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[R]);
			Market[R] = W.Components().GetPool(Markets.Market).TryGet(RegionHandles[R]);
			if (Detailed[R] == 0)
			{
				const History::RegionPopulation* Counts =
					W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
				People[R] = Counts != nullptr ? Counts->Total : 0u;
			}
			if (Common[R] != nullptr)
			{
				for (uint32 g = 0; g < GoodCount; ++g)
				{
					Held[usize{R} * GoodCount + g] += Common[R]->Amount[g];
				}
			}
		}
		auto Wanted = [&](uint32 R, uint32 g)
		{ return WantedStock(Consumption, Prices, static_cast<Good>(g), People[R]); };
		// The routes, open ones first by index; the highest index of all.
		std::vector<Route> Open;
		std::vector<Route> Closed;
		uint32 LastRoute = 0;
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle H, const RouteInfo& R)
				{
					LastRoute = std::max(LastRoute, R.Index);
					(R.Closed == 0 ? Open : Closed).push_back(Route{H, R});
				});
		std::sort(Closed.begin(), Closed.end(),
				  [](const Route& A, const Route& B) { return A.Info.Index < B.Info.Index; });
		std::sort(Open.begin(), Open.end(), [](const Route& A, const Route& B) { return A.Info.Index < B.Info.Index; });
		std::vector<uint32> RoutesAt(N, 0u);
		std::vector<uint64> Traffic(N, 0u);
		// 1. Carry: a share of the cheaper side's surplus of every good, up to the
		//    dearer side's want and the yearly limit, common stock to common stock.
		for (Route& Rt : Open)
		{
			const uint32 A = Rt.Info.From;
			const uint32 B = Rt.Info.To;
			uint64 Units = 0;
			// A made road (09.04) lets more of the trade that already wanted to
			// happen get across. No road is a factor of one, to the unit.
			const RouteEase* Made = HasEase ? W.Components().GetPool(Ease).TryGet(Rt.Handle) : nullptr;
			const uint64 Easier = 1000u + (Made != nullptr ? Made->CarryPerMille : 0u);
			if (A < N && B < N && Common[A] != nullptr && Common[B] != nullptr && Market[A] != nullptr &&
				Market[B] != nullptr)
			{
				for (uint32 g = 0; g < GoodCount; ++g)
				{
					const uint32 PA = Market[A]->Price[g];
					const uint32 PB = Market[B]->Price[g];
					if (PA == PB)
					{
						continue;
					}
					const uint32 S = PA < PB ? A : B; // the seller, where the good is cheap
					const uint32 D = PA < PB ? B : A;
					const uint64 HeldS = Held[usize{S} * GoodCount + g];
					const uint64 WantS = Wanted(S, g);
					const uint64 HeldD = Held[usize{D} * GoodCount + g];
					const uint64 WantD = Wanted(D, g);
					if (HeldS <= WantS || HeldD >= WantD)
					{
						continue;
					}
					uint64 Carry =
						std::min<uint64>({(HeldS - WantS) * Rules.CarryPerMille / 1000u * Easier / 1000u, WantD - HeldD,
										  uint64{Rules.CarryMax} * Easier / 1000u, Common[S]->Amount[g]});
					if (Carry == 0)
					{
						continue;
					}
					Common[S]->Amount[g] -= static_cast<uint32>(Carry);
					Common[D]->Amount[g] = Saturate(uint64{Common[D]->Amount[g]} + Carry);
					Held[usize{S} * GoodCount + g] -= Carry;
					Held[usize{D} * GoodCount + g] += Carry;
					Units += Carry;
				}
			}
			RouteInfo* Live = W.Components().GetPool(Trade.Route).TryGet(Rt.Handle);
			if (Live == nullptr)
			{
				continue;
			}
			if (Units > 0)
			{
				Live->Idle = 0;
				Live->Carried += Units;
				Context.Events->Publish(Context.Tick, GoodsCarriedEvent,
										TradePayload{Live->Index, A, B, Saturate(Units)},
										W.Entities().GetId(Rt.Handle));
				if (A < N)
				{
					Traffic[A] += Units;
				}
				if (B < N)
				{
					Traffic[B] += Units;
				}
			}
			else
			{
				Live->Idle = static_cast<uint16>(Live->Idle + 1u);
			}
			if (Live->Idle >= Rules.CloseAfterIdleYears)
			{
				Live->Closed = Context.Tick;
				Context.Events->Publish(Context.Tick, RouteClosedEvent, TradePayload{Live->Index, A, B, 0},
										W.Entities().GetId(Rt.Handle));
				Rt.Info.Closed = Context.Tick;
				continue;
			}
			if (A < N)
			{
				++RoutesAt[A];
			}
			if (B < N)
			{
				++RoutesAt[B];
			}
		}
		// 2. Open: between neighbours with markets and no open route, where a good
		//    is dear enough on one side, wanted there, and in surplus on the other.
		//    A road once built is reopened rather than built again.
		auto IsOpen = [&](uint32 A, uint32 B)
		{
			for (const Route& Rt : Open)
			{
				if (Rt.Info.Closed == 0 && Rt.Info.From == A && Rt.Info.To == B)
				{
					return true;
				}
			}
			return false;
		};
		for (uint32 A = 1; A < N && A < Graph.Neighbours.size(); ++A)
		{
			if (Market[A] == nullptr || Common[A] == nullptr)
			{
				continue;
			}
			std::vector<uint16> Around(Graph.Neighbours[A].begin(), Graph.Neighbours[A].end());
			std::sort(Around.begin(), Around.end());
			for (const uint16 Nb : Around)
			{
				const uint32 B = Nb;
				if (B <= A || B >= N || Market[B] == nullptr || Common[B] == nullptr || IsOpen(A, B) ||
					RoutesAt[A] >= Rules.MaxRoutesPerRegion || RoutesAt[B] >= Rules.MaxRoutesPerRegion)
				{
					continue;
				}
				bool Warranted = false;
				for (uint32 g = 0; g < GoodCount && !Warranted; ++g)
				{
					const uint64 PA = Market[A]->Price[g];
					const uint64 PB = Market[B]->Price[g];
					const uint64 Low = std::min(PA, PB);
					const uint64 High = std::max(PA, PB);
					const uint32 S = PA < PB ? A : B;
					const uint32 D = PA < PB ? B : A;
					Warranted = High * 1000u >= Low * (1000u + Rules.OpenGapPerMille) &&
								Held[usize{S} * GoodCount + g] > Wanted(S, g) &&
								Held[usize{D} * GoodCount + g] < Wanted(D, g);
				}
				if (!Warranted)
				{
					continue;
				}
				EntityHandle H;
				RouteInfo Info;
				for (const Route& Old : Closed)
				{
					if (Old.Info.From == A && Old.Info.To == B)
					{
						H = Old.Handle;
						Info = Old.Info;
						break;
					}
				}
				Info.Closed = 0;
				Info.Idle = 0;
				Info.Openings = static_cast<uint16>(Info.Openings < 0xffffu ? Info.Openings + 1u : Info.Openings);
				Info.Opened = Context.Tick;
				if (H.IsNull())
				{
					H = W.CreateEntity(IdKind::Route);
					Info.Index = ++LastRoute;
					Info.From = A;
					Info.To = B;
					Info.Identity =
						Noise::LatticeHash(W.Config().Seed ^ RouteSalt, static_cast<int32>(A), static_cast<int32>(B));
					W.Components().GetPool(Trade.Route).Add(H, Info);
				}
				else
				{
					W.Components().GetPool(Trade.Route).Get(H) = Info;
				}
				Open.push_back(Route{H, Info});
				++RoutesAt[A];
				++RoutesAt[B];
				Context.Events->Publish(Context.Tick, RouteOpenedEvent, TradePayload{Info.Index, A, B, 0},
										W.Entities().GetId(H));
			}
		}
		// 3. Settlements: founded where the traffic is, abandoned where it stopped.
		std::vector<Settlement> Alive;
		uint32 LastSettlement = 0;
		W.Components()
			.GetPool(Trade.Settlement)
			.ForEach(
				[&](EntityHandle H, const SettlementInfo& S)
				{
					LastSettlement = std::max(LastSettlement, S.Index);
					if (S.Abandoned == 0)
					{
						Alive.push_back(Settlement{H, S});
					}
				});
		std::sort(Alive.begin(), Alive.end(),
				  [](const Settlement& A, const Settlement& B) { return A.Info.Index < B.Info.Index; });
		std::vector<uint8> Settled(N, 0u);
		for (const Settlement& St : Alive)
		{
			SettlementInfo* Live = W.Components().GetPool(Trade.Settlement).TryGet(St.Handle);
			if (Live == nullptr || Live->Region >= N)
			{
				continue;
			}
			Settled[Live->Region] = 1;
			Live->Routes = RoutesAt[Live->Region];
			Live->Traffic = Saturate(Traffic[Live->Region]);
			Live->Quiet = Traffic[Live->Region] > 0 ? 0u : Live->Quiet + 1u;
			if (Live->Quiet >= Rules.AbandonAfterQuietYears)
			{
				Live->Abandoned = Context.Tick;
				Context.Events->Publish(Context.Tick, SettlementAbandonedEvent,
										TradePayload{Live->Index, Live->Region, 0, 0}, W.Entities().GetId(St.Handle));
			}
		}
		for (uint32 R = 1; R < N; ++R)
		{
			if (Settled[R] != 0 || Traffic[R] < Rules.SettleFromTraffic || RegionHandles[R].IsNull())
			{
				continue;
			}
			const EntityHandle H = W.CreateEntity(IdKind::Settlement);
			SettlementInfo Info;
			Info.Index = ++LastSettlement;
			Info.Region = R;
			Info.Routes = RoutesAt[R];
			Info.Traffic = Saturate(Traffic[R]);
			Info.Founded = Context.Tick;
			Info.Identity = Noise::LatticeHash(W.Config().Seed ^ SettlementSalt, static_cast<int32>(Info.Index),
											   static_cast<int32>(R));
			W.Components().GetPool(Trade.Settlement).Add(H, Info);
			Context.Events->Publish(Context.Tick, SettlementFoundedEvent, TradePayload{Info.Index, R, 0, Info.Traffic},
									W.Entities().GetId(H));
		}
	}

	void RoutesOf(const World& W, const TradeTypes& Trade, uint32 Region, std::vector<RouteInfo>& Out)
	{
		Out.clear();
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle, const RouteInfo& R)
				{
					if (R.Closed == 0 && (R.From == Region || R.To == Region))
					{
						Out.push_back(R);
					}
				});
		std::sort(Out.begin(), Out.end(), [](const RouteInfo& A, const RouteInfo& B) { return A.Index < B.Index; });
	}

	const RouteInfo* RouteBetween(const World& W, const TradeTypes& Trade, uint32 A, uint32 B)
	{
		const uint32 Lo = std::min(A, B);
		const uint32 Hi = std::max(A, B);
		const RouteInfo* Found = nullptr;
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle H, const RouteInfo& R)
				{
					if (Found == nullptr && R.Closed == 0 && R.From == Lo && R.To == Hi)
					{
						Found = W.Components().GetPool(Trade.Route).TryGet(H);
					}
				});
		return Found;
	}

	const SettlementInfo* SettlementOf(const World& W, const TradeTypes& Trade, uint32 Region)
	{
		const SettlementInfo* Found = nullptr;
		W.Components()
			.GetPool(Trade.Settlement)
			.ForEach(
				[&](EntityHandle H, const SettlementInfo& S)
				{
					if (Found == nullptr && S.Abandoned == 0 && S.Region == Region)
					{
						Found = W.Components().GetPool(Trade.Settlement).TryGet(H);
					}
				});
		return Found;
	}

	TradeStats MeasureTrade(const World& W, const History::PreHistoryTypes& Types, const TradeTypes& Trade,
							const TradeRules& Rules)
	{
		TradeStats S;
		const WorldGen::RegionGraph Graph = WorldGen::BuildRegionGraph(W.Map(), Types.World.Regions);
		std::vector<RouteInfo> Routes;
		W.Components().GetPool(Trade.Route).ForEach([&](EntityHandle, const RouteInfo& R) { Routes.push_back(R); });
		std::sort(Routes.begin(), Routes.end(),
				  [](const RouteInfo& A, const RouteInfo& B) { return A.Index < B.Index; });
		std::vector<uint32> At(Graph.Neighbours.size(), 0u);
		Hash64 D = HashString("Trade");
		for (usize i = 0; i < Routes.size(); ++i)
		{
			const RouteInfo& R = Routes[i];
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R), sizeof(R)));
			if (R.Closed != 0)
			{
				++S.RoutesClosed;
				continue;
			}
			++S.RoutesOpen;
			const bool Adjacent = R.From < Graph.Neighbours.size() && R.To < Graph.Neighbours.size() &&
								  Graph.AreAdjacent(static_cast<uint16>(R.From), static_cast<uint16>(R.To));
			bool Twice = false;
			for (usize j = 0; j < i; ++j)
			{
				Twice = Twice || (Routes[j].Closed == 0 && Routes[j].From == R.From && Routes[j].To == R.To);
			}
			if (R.From < At.size())
			{
				++At[R.From];
			}
			if (R.To < At.size())
			{
				++At[R.To];
			}
			S.Bad += !Adjacent || Twice || R.From >= R.To ? 1u : 0u;
		}
		for (const uint32 Count : At)
		{
			S.Bad += Count > Rules.MaxRoutesPerRegion ? 1u : 0u;
		}
		std::vector<SettlementInfo> Settlements;
		W.Components()
			.GetPool(Trade.Settlement)
			.ForEach([&](EntityHandle, const SettlementInfo& St) { Settlements.push_back(St); });
		std::sort(Settlements.begin(), Settlements.end(),
				  [](const SettlementInfo& A, const SettlementInfo& B) { return A.Index < B.Index; });
		for (const SettlementInfo& St : Settlements)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&St), sizeof(St)));
			S.Settlements += St.Abandoned == 0 ? 1u : 0u;
			S.MostTraffic = St.Abandoned == 0 ? std::max(S.MostTraffic, St.Traffic) : S.MostTraffic;
			S.Abandoned += St.Abandoned != 0 ? 1u : 0u;
		}
		for (const Event& E : W.Log().All())
		{
			if (E.Is(GoodsCarriedEvent))
			{
				++S.Carries;
				S.Carried += E.Get<TradePayload>().Amount;
			}
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Economy
