// VAELEN - VaelenPolitics
// Phase 07.01: polities.
//
// STATUS: VALIDATED (Phase 07) - unit/deterministic/edge tests in Tests/Politics

#include "Vaelen/Politics/Polities.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Politics
{
	namespace
	{
		constexpr uint64 PolitySalt = 0x504f4c495459ull; // "POLITY"

		struct Seat
		{
			EntityHandle Handle;
			PolityInfo Info;
		};
	} // namespace

	PolityTypes PolityTypes::Declare(World& W)
	{
		PolityTypes T;
		T.Polity = W.Types().Register<PolityInfo>("PolityInfo");
		T.Rule = W.Types().Register<RegionRule>("RegionRule");
		W.Components().CreatePool(T.Polity);
		W.Components().CreatePool(T.Rule);
		return T;
	}

	void PolitySystem::Tick(TickContext& Context)
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
		// The councils of the world, and their heads, once.
		std::vector<uint32> CouncilOf(N, 0u); // region -> council index
		std::vector<uint32> HeadOf;			  // council index -> head person
		std::vector<uint32> MembersOf_;		  // council index -> living members
		std::vector<uint32> CultureOf;		  // council index -> culture
		W.Components()
			.GetPool(Organizations.Organization)
			.ForEach(
				[&](EntityHandle, const Society::OrganizationInfo& O)
				{
					if (O.Index >= HeadOf.size())
					{
						HeadOf.resize(usize{O.Index} + 1u, 0u);
						MembersOf_.resize(usize{O.Index} + 1u, 0u);
						CultureOf.resize(usize{O.Index} + 1u, 0u);
					}
					HeadOf[O.Index] = O.Head;
					MembersOf_[O.Index] = O.Members;
					CultureOf[O.Index] = O.Culture;
					if (O.Disbanded == 0 && O.Kind == static_cast<uint32>(Society::OrganizationKind::Council) &&
						O.Region < N && CouncilOf[O.Region] == 0)
					{
						CouncilOf[O.Region] = O.Index;
					}
				});
		// The living of every detailed region.
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
		std::vector<uint32> Alive(N, 0u);
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& P)
				{
					if (P.Region < N && P.State == static_cast<uint8>(Population::LifeState::Alive))
					{
						++Alive[P.Region];
					}
				});
		for (uint32 R = 1; R < N; ++R)
		{
			if (Detailed[R] != 0 || RegionHandles[R].IsNull())
			{
				continue;
			}
			const History::RegionPopulation* P =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
			Alive[R] = P != nullptr ? P->Total : 0u;
		}

		// The polities, standing first, in index order.
		std::vector<Seat> Seats;
		uint32 LastPolity = 0;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					LastPolity = std::max(LastPolity, P.Index);
					Seats.push_back(Seat{H, P});
				});
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Info.Index < B.Info.Index; });

		// 1. Found: a detailed region with a council of enough seats and people,
		//    belonging to nobody, becomes the seat of a new polity.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull() || Detailed[R] == 0 || CouncilOf[R] == 0)
			{
				continue;
			}
			const RegionRule* Held = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			if (Held != nullptr && Held->Polity != 0)
			{
				continue;
			}
			const uint32 Council = CouncilOf[R];
			if (Alive[R] < Rules.FoundFromPeople || MembersOf_[Council] < Rules.FoundFromSeats)
			{
				continue;
			}
			PolityInfo Info;
			Info.Index = ++LastPolity;
			Info.Seat = R;
			Info.Culture = Council < CultureOf.size() ? CultureOf[Council] : 0u;
			Info.Council = Council;
			Info.Regions = 1;
			Info.Founded = Context.Tick;
			Info.Identity =
				Noise::LatticeHash(W.Config().Seed ^ PolitySalt, static_cast<int32>(Info.Index), static_cast<int32>(R));
			const EntityHandle H = W.CreateEntity(IdKind::Polity);
			W.Components().GetPool(Polities.Polity).Add(H, Info);
			Seats.push_back(Seat{H, Info});
			RegionRule Rule;
			Rule.Polity = Info.Index;
			Rule.Since = Context.Tick;
			RegionRule* Existing = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			if (Existing != nullptr)
			{
				*Existing = Rule;
			}
			else
			{
				W.Components().GetPool(Polities.Rule).Add(RegionHandles[R], Rule);
			}
			Context.Events->Publish(Context.Tick, PolityFoundedEvent, PolityPayload{Info.Index, R, 0, Council},
									W.Entities().GetId(H));
			Context.Events->Publish(Context.Tick, RegionClaimedEvent, PolityPayload{Info.Index, R, 0, 1},
									W.Entities().GetId(RegionHandles[R]));
		}

		// 2. Every standing polity: its ruler, its regions, and whether it still rules.
		for (Seat& S : Seats)
		{
			PolityInfo* P = W.Components().GetPool(Polities.Polity).TryGet(S.Handle);
			if (P == nullptr || P->Dissolved != 0)
			{
				continue;
			}
			const uint32 Council = P->Council;
			const uint32 Head = Council < HeadOf.size() ? HeadOf[Council] : 0u;
			const uint32 Members = Council < MembersOf_.size() ? MembersOf_[Council] : 0u;
			// The ruler is the council's head, of age. A head that is gone leaves
			// the seat empty until the council names another (05.01).
			uint32 Ruler = 0;
			if (Head != 0)
			{
				const Population::PersonInfo* Person = Population::FindPerson(W, Persons, Head);
				const bool Fit = Person != nullptr &&
								 Person->State == static_cast<uint8>(Population::LifeState::Alive) &&
								 Population::AgeYears(*Person, Context.Tick) >= Rules.RulerFromAge;
				Ruler = Fit ? Head : 0u;
			}
			if (Ruler != P->Ruler)
			{
				P->Ruler = Ruler;
				if (Ruler != 0)
				{
					Context.Events->Publish(Context.Tick, RulerSeatedEvent,
											PolityPayload{P->Index, P->Seat, Ruler, Council},
											W.Entities().GetId(S.Handle));
				}
			}
			// The regions it holds, counted from the regions themselves.
			uint32 Held = 0;
			for (uint32 R = 1; R < N; ++R)
			{
				if (RegionHandles[R].IsNull())
				{
					continue;
				}
				const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
				Held += Rule != nullptr && Rule->Polity == P->Index ? 1u : 0u;
			}
			P->Regions = Held;
			// Dissolution: no council of its own, nothing left to rule, or the
			// seat itself gone. A polity rules through the council of its seat
			// (ADR-0056); once that ground belongs to somebody else the council
			// sits in another realm, and there is nothing left to rule through.
			// Only a conquest can take a seat - a faction never rises in a
			// capital (07.05) and a seat never slips for want of hold (07.03) -
			// so this is what makes a conquest decisive.
			const bool SeatLost = P->Seat >= N || RegionHandles[P->Seat].IsNull() || [&]
			{
				const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[P->Seat]);
				return Rule == nullptr || Rule->Polity != P->Index;
			}();
			const bool Ruined = Members == 0 || Held == 0 || SeatLost || (P->Seat < N && CouncilOf[P->Seat] != Council);
			if (!Ruined)
			{
				continue;
			}
			// A polity is given its first years to grow into itself - but not when
			// the seat itself is gone. Losing a capital is not a failure to grow,
			// and a polity ruling through a council that now sits in another
			// realm is a contradiction rather than a young state.
			const uint64 Years = uint64{History::TicksPerYear} * Rules.DissolveAfterYears;
			if (!SeatLost && Context.Tick < P->Founded + Years)
			{
				continue;
			}
			P->Dissolved = Context.Tick;
			P->Ruler = 0;
			for (uint32 R = 1; R < N; ++R)
			{
				if (RegionHandles[R].IsNull())
				{
					continue;
				}
				RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
				if (Rule == nullptr || Rule->Polity != P->Index)
				{
					continue;
				}
				Rule->Polity = 0;
				Rule->Since = Context.Tick;
				Context.Events->Publish(Context.Tick, RegionLostEvent, PolityPayload{P->Index, R, 0, 0},
										W.Entities().GetId(RegionHandles[R]));
			}
			P->Regions = 0;
			Context.Events->Publish(Context.Tick, PolityDissolvedEvent, PolityPayload{P->Index, P->Seat, 0, Held},
									W.Entities().GetId(S.Handle));
		}
	}

	const PolityInfo* PolityOf(const World& W, const PolityTypes& Types, uint32 Polity)
	{
		const PolityInfo* Found = nullptr;
		W.Components()
			.GetPool(Types.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					if (P.Index == Polity && Found == nullptr)
					{
						Found = W.Components().GetPool(Types.Polity).TryGet(H);
					}
				});
		return Found;
	}

	const RegionRule* RuleOf(const World& W, const History::PreHistoryTypes& Types, const PolityTypes& Polities,
							 uint32 Region)
	{
		const RegionRule* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Polities.Rule).TryGet(H);
					}
				});
		return Found;
	}

	void RegionsOf(const World& W, const History::PreHistoryTypes& Types, const PolityTypes& Polities, uint32 Polity,
				   std::vector<uint32>& Out)
	{
		Out.clear();
		if (Polity == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(H);
					if (Rule != nullptr && Rule->Polity == Polity)
					{
						Out.push_back(R.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	PolityStats MeasurePolities(const World& W, const History::PreHistoryTypes& Types,
								const Population::PersonTypes& Persons, const Society::OrganizationTypes& Organizations,
								const PolityTypes& Polities)
	{
		PolityStats S;
		std::vector<uint32> HeadOf;
		W.Components()
			.GetPool(Organizations.Organization)
			.ForEach(
				[&](EntityHandle, const Society::OrganizationInfo& O)
				{
					if (O.Index >= HeadOf.size())
					{
						HeadOf.resize(usize{O.Index} + 1u, 0u);
					}
					HeadOf[O.Index] = O.Head;
				});
		std::vector<PolityInfo> All;
		W.Components().GetPool(Polities.Polity).ForEach([&](EntityHandle, const PolityInfo& P) { All.push_back(P); });
		std::sort(All.begin(), All.end(), [](const PolityInfo& A, const PolityInfo& B) { return A.Index < B.Index; });
		std::vector<std::pair<uint32, RegionRule>> Rules_;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(H);
					if (Rule != nullptr)
					{
						Rules_.push_back({R.Index, *Rule});
					}
				});
		std::sort(Rules_.begin(), Rules_.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		Hash64 D = HashString("Polities");
		for (const PolityInfo& P : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&P), sizeof(P)));
			if (P.Dissolved != 0)
			{
				++S.Dissolved;
				S.Bad += P.Ruler != 0 || P.Regions != 0 ? 1u : 0u;
				continue;
			}
			++S.Standing;
			// The ruler is the council's head and alive. The seat is another
			// matter: a polity whose seat is ruled by somebody else has had it
			// stormed (08.04) and is dissolved on this system's next tick, which
			// is a fact about the world and not an incoherence in it.
			bool Bad = false;
			const auto Seat =
				std::find_if(Rules_.begin(), Rules_.end(), [&](const auto& E) { return E.first == P.Seat; });
			S.Doomed += Seat == Rules_.end() || Seat->second.Polity != P.Index ? 1u : 0u;
			if (P.Ruler != 0)
			{
				const Population::PersonInfo* Person = Population::FindPerson(W, Persons, P.Ruler);
				const bool Living =
					Person != nullptr && Person->State == static_cast<uint8>(Population::LifeState::Alive);
				S.Bereft += Living ? 0u : 1u;
				// Whether he is the council's head is the incoherence; whether he is
				// alive is the world, and this system answers it on its next tick.
				Bad = Bad || P.Council >= HeadOf.size() || (Living && HeadOf[P.Council] != P.Ruler);
			}
			else
			{
				++S.Headless;
			}
			S.Bad += Bad ? 1u : 0u;
		}
		for (const auto& [Index, Rule] : Rules_)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Rule), sizeof(Rule)));
			if (Rule.Polity == 0)
			{
				++S.Unruled;
				continue;
			}
			const auto Owner_ =
				std::find_if(All.begin(), All.end(), [&](const PolityInfo& P) { return P.Index == Rule.Polity; });
			if (Owner_ == All.end() || Owner_->Dissolved != 0)
			{
				++S.Bad; // a region held by a polity that is gone
				continue;
			}
			++S.Ruled;
		}
		for (const Event& E : W.Log().All())
		{
			S.Founded += E.Is(PolityFoundedEvent) ? 1u : 0u;
			S.Ended += E.Is(PolityDissolvedEvent) ? 1u : 0u;
			S.Seatings += E.Is(RulerSeatedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Politics
