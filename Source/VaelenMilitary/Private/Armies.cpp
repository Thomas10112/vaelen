// VAELEN - VaelenMilitary
// Phase 08.01: levies and armies.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military

#include "Vaelen/Military/Armies.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Military
{
	namespace
	{
		constexpr uint64 ArmySalt = 0x41524d59ull; // "ARMY"
		constexpr uint32 G_GRAIN = static_cast<uint32>(Economy::Good::Grain);
	} // namespace

	ArmyTypes ArmyTypes::Declare(World& W)
	{
		ArmyTypes T;
		T.Army = W.Types().Register<ArmyInfo>("ArmyInfo");
		T.Levy = W.Types().Register<RegionLevy>("RegionLevy");
		W.Components().CreatePool(T.Army);
		W.Components().CreatePool(T.Levy);
		return T;
	}

	void ArmySystem::Tick(TickContext& Context)
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
		std::vector<uint32> RuledBy(N, 0u);
		std::vector<uint32> HoldOf(N, 0u);
		std::vector<uint64> People(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
			const Politics::RegionAuthority* A = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
			HoldOf[R] = A != nullptr ? A->Hold : 0u;
			const History::RegionPopulation* Counts =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
			People[R] = Counts != nullptr ? Counts->Total : 0u;
		}

		struct Seat
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Region = 0;
			bool Standing = false;
		};
		std::vector<Seat> Seats;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach([&](EntityHandle H, const Politics::PolityInfo& P)
					 { Seats.push_back(Seat{H, P.Index, P.Seat, P.Dissolved == 0}); });
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Index < B.Index; });

		// Who is at war, from the relations of 07.06.
		std::vector<uint32> AtWar;
		W.Components()
			.GetPool(Relations.Relation_)
			.ForEach(
				[&](EntityHandle, const Politics::Relation& Bond)
				{
					if (Bond.Stance_ != static_cast<uint32>(Politics::Stance::War))
					{
						return;
					}
					AtWar.push_back(Bond.A);
					AtWar.push_back(Bond.B);
				});
		std::sort(AtWar.begin(), AtWar.end());
		AtWar.erase(std::unique(AtWar.begin(), AtWar.end()), AtWar.end());
		auto Fighting = [&](uint32 Polity) { return std::binary_search(AtWar.begin(), AtWar.end(), Polity); };

		// 1. The hosts that already stand, in index order.
		struct Standing
		{
			EntityHandle Handle;
			uint32 Index = 0;
		};
		std::vector<Standing> Order;
		uint32 Highest = 0;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle H, const ArmyInfo& A)
				{
					Highest = std::max(Highest, A.Index);
					if (A.Disbanded == 0)
					{
						Order.push_back(Standing{H, A.Index});
					}
				});
		std::sort(Order.begin(), Order.end(), [](const Standing& A, const Standing& B) { return A.Index < B.Index; });

		/// Send an army home: the men return to the regions that gave them. Every
		/// one of them must be found, or the count of men away stops matching the
		/// count of men under arms and nobody notices until a digest moves.
		auto SendHome = [&](ArmyInfo& A, uint32 Men, LevyEnd Why)
		{ VAELEN_ENSURE(ReleaseLevy(W, Types, Armies, A.Polity, Men, Why, Context) == Men); };

		std::vector<uint32> Hosted; // polities with a host standing after this pass
		for (const Standing& It : Order)
		{
			ArmyInfo* A = W.Components().GetPool(Armies.Army).TryGet(It.Handle);
			if (A == nullptr)
			{
				continue;
			}
			const auto At = std::lower_bound(Seats.begin(), Seats.end(), A->Polity,
											 [](const Seat& S, uint32 V) { return S.Index < V; });
			const Seat* Master = At != Seats.end() && At->Index == A->Polity ? &*At : nullptr;
			// A host whose polity is gone, or that has no war left, goes home.
			if (Master == nullptr || !Master->Standing || !Fighting(A->Polity))
			{
				SendHome(*A, A->Strength, LevyEnd::Home);
				A->Disbanded = Context.Tick;
				Context.Events->Publish(Context.Tick, ArmyDisbandedEvent,
										Politics::PolityPayload{A->Polity, A->Region, A->Index, A->Strength},
										W.Entities().GetId(It.Handle));
				A->Strength = 0;
				continue;
			}
			// It eats. What the treasury cannot cover is men melting away.
			Politics::Treasury* Hoard = W.Components().GetPool(Laws.Hoard).TryGet(Master->Handle);
			const uint64 Want = uint64{A->Strength} * Rules.GrainPerManPerYear;
			const uint64 Have = Hoard != nullptr ? Hoard->Amount[G_GRAIN] : 0u;
			const uint64 Paid = std::min(Want, Have);
			if (Hoard != nullptr)
			{
				Hoard->Amount[G_GRAIN] -= static_cast<uint32>(Paid);
			}
			A->Fed = static_cast<uint32>(Paid);
			if (Paid >= Want)
			{
				A->Hungry = 0;
				Hosted.push_back(A->Polity);
				continue;
			}
			++A->Hungry;
			const uint32 Lost = std::max<uint32>(1u, A->Strength * Rules.MeltPerMille / 1000u);
			const uint32 Melted = std::min(Lost, A->Strength);
			SendHome(*A, Melted, LevyEnd::Melted);
			A->Strength -= Melted;
			Context.Events->Publish(Context.Tick, ArmyStarvedEvent,
									Politics::PolityPayload{A->Polity, A->Region, A->Index, Melted},
									W.Entities().GetId(It.Handle));
			if (A->Strength < Rules.RaiseAtStrength || A->Hungry >= Rules.HungryBeforeGone)
			{
				SendHome(*A, A->Strength, LevyEnd::Home);
				A->Disbanded = Context.Tick;
				Context.Events->Publish(Context.Tick, ArmyDisbandedEvent,
										Politics::PolityPayload{A->Polity, A->Region, A->Index, A->Strength},
										W.Entities().GetId(It.Handle));
				A->Strength = 0;
				continue;
			}
			Hosted.push_back(A->Polity);
		}
		std::sort(Hosted.begin(), Hosted.end());

		// 2. Raise a host where there is a war and none stands. The men come from
		//    the regions the polity holds, in region order, each giving by its
		//    people and how firmly it is held - a levy is obedience, and 07.03
		//    already measures obedience.
		for (const Seat& S : Seats)
		{
			if (!S.Standing || !Fighting(S.Index))
			{
				continue;
			}
			const usize Have = static_cast<usize>(std::count(Hosted.begin(), Hosted.end(), S.Index));
			if (Have >= Rules.ArmiesPerPolity)
			{
				continue;
			}
			struct Given
			{
				uint32 Region = 0;
				uint32 Men = 0;
			};
			std::vector<Given> Called;
			uint32 Total = 0;
			for (uint32 R = 1; R < N; ++R)
			{
				if (RuledBy[R] != S.Index || RegionHandles[R].IsNull() || HoldOf[R] < Rules.LevyFloorHold)
				{
					continue;
				}
				// A region whose men are already away for somebody else gives none.
				// Ground changes hands (07.03, 07.05) while its men are under arms
				// elsewhere, and taking them again would write over the first claim -
				// leaving men no army accounts for when that first army is destroyed.
				const RegionLevy* Owed = W.Components().GetPool(Armies.Levy).TryGet(RegionHandles[R]);
				if (Owed != nullptr && Owed->Men != 0 && Owed->Polity != S.Index)
				{
					continue;
				}
				const uint64 Men = People[R] * Rules.MenPerThousand / 1000u * HoldOf[R] / 1000u;
				if (Men == 0)
				{
					continue;
				}
				Called.push_back(Given{R, static_cast<uint32>(Men)});
				Total += static_cast<uint32>(Men);
			}
			if (Total < Rules.RaiseAtStrength)
			{
				continue; // not worth calling
			}
			ArmyInfo Fresh;
			Fresh.Index = ++Highest;
			Fresh.Polity = S.Index;
			Fresh.Region = S.Region;
			Fresh.Strength = Total;
			Fresh.Raised = Context.Tick;
			Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ ArmySalt, static_cast<int32>(Fresh.Index),
												static_cast<int32>(S.Region));
			const EntityHandle H = W.CreateEntity(IdKind::Army);
			W.Components().GetPool(Armies.Army).Add(H, Fresh);
			for (const Given& G : Called)
			{
				RegionLevy* Away = W.Components().GetPool(Armies.Levy).TryGet(RegionHandles[G.Region]);
				if (Away == nullptr)
				{
					W.Components().GetPool(Armies.Levy).Add(RegionHandles[G.Region], RegionLevy{S.Index, G.Men});
					continue;
				}
				Away->Polity = S.Index;
				Away->Men += G.Men;
			}
			Hosted.insert(std::lower_bound(Hosted.begin(), Hosted.end(), S.Index), S.Index);
			Context.Events->Publish(Context.Tick, ArmyRaisedEvent,
									Politics::PolityPayload{S.Index, S.Region, Fresh.Index, Total},
									W.Entities().GetId(H));
		}
	}

	const ArmyInfo* ArmyOf(const World& W, const ArmyTypes& Armies, uint32 Army)
	{
		const ArmyInfo* Found = nullptr;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle H, const ArmyInfo& A)
				{
					if (A.Index == Army && Found == nullptr)
					{
						Found = W.Components().GetPool(Armies.Army).TryGet(H);
					}
				});
		return Found;
	}

	void ArmiesOf(const World& W, const ArmyTypes& Armies, uint32 Polity, std::vector<uint32>& Out)
	{
		Out.clear();
		if (Polity == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle, const ArmyInfo& A)
				{
					if (A.Polity == Polity && A.Disbanded == 0)
					{
						Out.push_back(A.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	void WarPairs(const World& W, const Politics::DiplomacyTypes& Relations,
				  std::vector<std::pair<uint32, uint32>>& Out)
	{
		Out.clear();
		W.Components()
			.GetPool(Relations.Relation_)
			.ForEach(
				[&](EntityHandle, const Politics::Relation& Bond)
				{
					if (Bond.Stance_ != static_cast<uint32>(Politics::Stance::War))
					{
						return;
					}
					Out.push_back({Bond.A, Bond.B});
					Out.push_back({Bond.B, Bond.A});
				});
		std::sort(Out.begin(), Out.end());
	}

	uint32 ReleaseLevy(World& W, const History::PreHistoryTypes& Types, const ArmyTypes& Armies, uint32 Polity,
					   uint32 Men, LevyEnd Why, TickContext& Context)
	{
		if (Polity == 0 || Men == 0)
		{
			return 0;
		}
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
		uint32 Left = Men;
		for (usize R = 1; R < RegionHandles.size() && Left != 0; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			RegionLevy* Given = W.Components().GetPool(Armies.Levy).TryGet(RegionHandles[R]);
			if (Given == nullptr || Given->Polity != Polity || Given->Men == 0)
			{
				continue;
			}
			const uint32 Back = std::min(Given->Men, Left);
			Given->Men -= Back;
			Left -= Back;
			if (Given->Men == 0)
			{
				Given->Polity = 0;
			}
			// One record per region touched: the levy only knows the men are no
			// longer under arms, and 08.06 needs to know which people that was.
			if (Context.Events != nullptr)
			{
				Context.Events->Publish(
					Context.Tick, LevyReleasedEvent,
					Politics::PolityPayload{Polity, static_cast<uint32>(R), static_cast<uint32>(Why), Back},
					W.Entities().GetId(RegionHandles[R]));
			}
		}
		return Men - Left;
	}

	const RegionLevy* LevyOf(const World& W, const History::PreHistoryTypes& Types, const ArmyTypes& Armies,
							 uint32 Region)
	{
		const RegionLevy* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Armies.Levy).TryGet(H);
					}
				});
		return Found;
	}

	ArmyStats MeasureArmies(const World& W, const History::PreHistoryTypes& Types,
							const Politics::PolityTypes& Polities, const ArmyTypes& Armies, const ArmyRules& Rules)
	{
		ArmyStats S;
		(void)Rules;
		std::vector<uint32> StandingPolities;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const Politics::PolityInfo& P)
				{
					if (P.Dissolved == 0)
					{
						StandingPolities.push_back(P.Index);
					}
				});
		std::sort(StandingPolities.begin(), StandingPolities.end());
		std::vector<uint32> RuledBy;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(H);
					if (R.Index >= RuledBy.size())
					{
						RuledBy.resize(usize{R.Index} + 1u, 0u);
					}
					RuledBy[R.Index] = Rule != nullptr ? Rule->Polity : 0u;
				});

		std::vector<ArmyInfo> All;
		W.Components().GetPool(Armies.Army).ForEach([&](EntityHandle, const ArmyInfo& A) { All.push_back(A); });
		std::sort(All.begin(), All.end(), [](const ArmyInfo& A, const ArmyInfo& B) { return A.Index < B.Index; });
		std::vector<std::pair<uint32, RegionLevy>> Levies;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionLevy* L = W.Components().GetPool(Armies.Levy).TryGet(H);
					if (L != nullptr)
					{
						Levies.push_back({R.Index, *L});
					}
				});
		std::sort(Levies.begin(), Levies.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		Hash64 D = HashString("Armies");
		for (const ArmyInfo& A : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&A), sizeof(A)));
			if (A.Disbanded != 0)
			{
				++S.Gone;
				S.Bad += A.Strength != 0 ? 1u : 0u; // a host that went home left nobody behind
				continue;
			}
			++S.Standing;
			S.Men += A.Strength;
			S.Fed += A.Fed;
			S.Bad += A.Strength == 0 ? 1u : 0u;
			S.Bad += std::binary_search(StandingPolities.begin(), StandingPolities.end(), A.Polity) ? 0u : 1u;
			// It stands somewhere real. Whose ground that is belongs to the march.
			S.Bad += A.Region != 0 && A.Region < RuledBy.size() ? 0u : 1u;
		}
		for (const auto& [Region, Levy] : Levies)
		{
			D = HashCombine(D, HashUInt64(Region));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Levy), sizeof(Levy)));
			if (Levy.Men == 0)
			{
				S.Bad += Levy.Polity != 0 ? 1u : 0u; // nobody away, yet still owed to somebody
				continue;
			}
			++S.Levied;
			S.Away += Levy.Men;
			S.Bad += Levy.Polity == 0 ? 1u : 0u;
		}
		// Nothing is invented and nothing is lost: the men the regions say are
		// away are exactly the men under arms.
		S.Bad += S.Away != S.Men ? 1u : 0u;
		for (const Event& E : W.Log().All())
		{
			S.Raisings += E.Is(ArmyRaisedEvent) ? 1u : 0u;
			S.Disbandings += E.Is(ArmyDisbandedEvent) ? 1u : 0u;
			S.Starvings += E.Is(ArmyStarvedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Military
