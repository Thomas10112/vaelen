// VAELEN - VaelenMilitary
// Phase 08.06: what war costs the living.
//
// STATUS: PROTOTYPE (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military

#include "Vaelen/Military/Toll.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Military
{
	namespace
	{
		bool IsAlive(const Population::PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(Population::LifeState::Alive);
		}
	} // namespace

	TollTypes TollTypes::Declare(World& W)
	{
		TollTypes T;
		T.Toll = W.Types().Register<RegionToll>("RegionToll");
		W.Components().CreatePool(T.Toll);
		return T;
	}

	void TollSystem::Tick(TickContext& Context)
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

		// What the year's releases were, region by region. The levy said the men
		// are no longer under arms; only the reason says whether they are alive.
		const uint64 Since = Context.Tick > History::TicksPerYear ? Context.Tick - History::TicksPerYear : 0u;
		std::vector<uint32> Fell(N, 0u);
		std::vector<uint32> Came(N, 0u);
		std::vector<uint32> Whose(N, 0u);
		// The release that reported the fallen, so that a burial can say why: the
		// men did not come back, because a battle was lost.
		std::vector<PersistentId> Because(N, PersistentId{});
		for (const Event& E : W.Log().All())
		{
			if (E.Tick <= Since || !E.Is(LevyReleasedEvent))
			{
				continue;
			}
			const Politics::PolityPayload P = E.Get<Politics::PolityPayload>();
			if (P.Region == 0 || P.Region >= N || P.Value == 0)
			{
				continue;
			}
			if (P.Person == static_cast<uint32>(LevyEnd::Fallen))
			{
				Fell[P.Region] += P.Value;
				Because[P.Region] = E.Id;
			}
			else
			{
				Came[P.Region] += P.Value;
			}
			Whose[P.Region] = P.Polity;
		}

		auto TollOn = [&](uint32 Region) -> RegionToll*
		{
			RegionToll* Kept = W.Components().GetPool(Toll.Toll).TryGet(RegionHandles[Region]);
			if (Kept == nullptr)
			{
				W.Components().GetPool(Toll.Toll).Add(RegionHandles[Region], RegionToll{});
				Kept = W.Components().GetPool(Toll.Toll).TryGet(RegionHandles[Region]);
			}
			return Kept;
		};

		/// The living adults of a region, in person index order, so that the
		/// same men are struck out or honoured on every run of a seed.
		auto AdultsOf = [&](uint32 Region)
		{
			std::vector<std::pair<uint32, EntityHandle>> Out;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const Population::PersonInfo& P)
					{
						if (P.Region == Region && IsAlive(P) &&
							Population::AgeYears(P, Context.Tick) >= Rules.AdultFrom)
						{
							Out.push_back({P.Index, H});
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		};

		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull() || (Fell[R] == 0 && Came[R] == 0))
			{
				continue;
			}
			RegionToll* Kept = TollOn(R);
			if (Kept == nullptr)
			{
				continue;
			}
			++Kept->Years;

			if (Fell[R] != 0)
			{
				// The dead. Where the region is simulated person by person they
				// are people, struck out; where it is not they are people all
				// the same, taken off the count.
				uint32 Struck = 0;
				if (Population::IsDetailed(W, Types, Persons, R))
				{
					for (const auto& [Index, H] : AdultsOf(R))
					{
						if (Struck >= Fell[R])
						{
							break;
						}
						Population::PersonInfo* Man = W.Components().GetPool(Persons.Person).TryGet(H);
						if (Man == nullptr)
						{
							continue;
						}
						Man->State = static_cast<uint8>(Population::LifeState::Dead);
						Man->Died = Context.Tick;
						++Struck;
					}
				}
				else
				{
					History::RegionPopulation* People =
						W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
					if (People != nullptr && People->Majority != 0)
					{
						Struck = People->Remove(People->Majority, Fell[R]);
						People->Recount();
					}
				}
				Kept->Fallen += Struck;
				if (Struck != 0)
				{
					Context.Events->Publish(Context.Tick, WarDeadEvent,
											Politics::PolityPayload{Whose[R], R, 0u, Struck},
											W.Entities().GetId(RegionHandles[R]), Because[R]);
				}
			}

			if (Came[R] != 0)
			{
				Kept->Home += Came[R];
				// What they carry back is what other people know about them.
				uint32 Honoured = 0;
				for (const auto& [Index, H] : AdultsOf(R))
				{
					if (Honoured >= Came[R])
					{
						break;
					}
					Society::PersonService* Served = W.Components().GetPool(Standing.Service).TryGet(H);
					if (Served == nullptr)
					{
						W.Components().GetPool(Standing.Service).Add(H, Society::PersonService{1u, 0u});
					}
					else
					{
						++Served->Wars;
					}
					++Honoured;
				}
			}
		}

		// People do not stay under a host that will not get off. This is coarse
		// only: moving a simulated person from one region to another is
		// migration, and migration is not this task's business.
		const WorldGen::RegionGraph& Graph = Roads.Of(W.Map(), Types.World.Regions);
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
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull() || Population::IsDetailed(W, Types, Persons, R) ||
				R >= Graph.Neighbours.size())
			{
				continue;
			}
			const RegionForage* Eaten = W.Components().GetPool(Marches.Forage).TryGet(RegionHandles[R]);
			if (Eaten == nullptr || Eaten->Years < Rules.FleeAfterYears)
			{
				continue;
			}
			History::RegionPopulation* Here =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
			if (Here == nullptr || Here->Total < Rules.MinToFlee || Here->Majority == 0)
			{
				continue;
			}
			// Somewhere its own ruler still holds, else anywhere at all; ties to
			// the lower region, so the road out is the same on every run.
			uint32 Away = 0;
			uint32 Anywhere = 0;
			for (const uint16 Next : Graph.Neighbours[R])
			{
				if (Next == 0 || Next >= N || RegionHandles[Next].IsNull())
				{
					continue;
				}
				const RegionForage* There = W.Components().GetPool(Marches.Forage).TryGet(RegionHandles[Next]);
				if (There != nullptr && There->Years != 0)
				{
					continue; // no sense fleeing onto another army
				}
				if (RuledBy[Next] == RuledBy[R] && (Away == 0 || Next < Away))
				{
					Away = Next;
				}
				if (Anywhere == 0 || Next < Anywhere)
				{
					Anywhere = Next;
				}
			}
			const uint32 To = Away != 0 ? Away : Anywhere;
			if (To == 0)
			{
				continue;
			}
			History::RegionPopulation* Beyond =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[To]);
			if (Beyond == nullptr)
			{
				continue;
			}
			const uint32 Want = std::max<uint32>(1u, Here->Total * Rules.FleePerMille / 1000u);
			const uint32 Culture = Here->Majority;
			const uint32 Left = Here->Remove(Culture, Want);
			if (Left == 0)
			{
				continue;
			}
			Here->Recount();
			if (!Beyond->Add(Culture, Left))
			{
				// Nowhere to put them: they stay rather than vanish.
				Here->Add(Culture, Left);
				Here->Recount();
				continue;
			}
			Beyond->Recount();
			RegionToll* Kept = TollOn(R);
			if (Kept != nullptr)
			{
				Kept->Fled += Left;
				Kept->Years += Fell[R] == 0 && Came[R] == 0 ? 1u : 0u;
			}
			Context.Events->Publish(Context.Tick, PeopleFledEvent, Politics::PolityPayload{0u, R, To, Left},
									W.Entities().GetId(RegionHandles[R]));
		}
	}

	const RegionToll* TollOf(const World& W, const History::PreHistoryTypes& Types, const TollTypes& Toll,
							 uint32 Region)
	{
		const RegionToll* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Toll.Toll).TryGet(H);
					}
				});
		return Found;
	}

	TollStats MeasureToll(const World& W, const History::PreHistoryTypes& Types, const Society::StandingTypes& Standing,
						  const TollTypes& Toll, const TollRules& Rules)
	{
		TollStats S;
		(void)Rules;
		usize Highest = 0;
		std::vector<std::pair<uint32, RegionToll>> All;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					Highest = std::max(Highest, usize{R.Index});
					const RegionToll* Kept = W.Components().GetPool(Toll.Toll).TryGet(H);
					if (Kept != nullptr)
					{
						All.push_back({R.Index, *Kept});
					}
				});
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		Hash64 D = HashString("Toll");
		for (const auto& [Region, Kept] : All)
		{
			D = HashCombine(D, HashUInt64(Region));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Kept), sizeof(Kept)));
			++S.Regions;
			S.Fallen += Kept.Fallen;
			S.Home += Kept.Home;
			S.Fled += Kept.Fled;
			// A toll is kept on a region that is there, and a region that has
			// been written about has been cost something in some year.
			S.Bad += Region != 0 && Region <= Highest ? 0u : 1u;
			S.Bad += Kept.Years != 0 ? 0u : 1u;
			S.Bad += Kept.Fallen != 0 || Kept.Home != 0 || Kept.Fled != 0 ? 0u : 1u;
		}
		W.Components()
			.GetPool(Standing.Service)
			.ForEach([&](EntityHandle, const Society::PersonService& Served)
					 { S.Served += Served.Wars != 0 ? 1u : 0u; });
		for (const Event& E : W.Log().All())
		{
			S.Deaths += E.Is(WarDeadEvent) ? 1u : 0u;
			S.Flights += E.Is(PeopleFledEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Military
