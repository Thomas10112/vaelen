// VAELEN - VaelenPopulation
// Phase 04.06: the LOD bridge - promotions, demotions and the crossings.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/long-duration tests in Tests/Population

#include "Vaelen/Population/Lod.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Population
{
	namespace
	{
		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
		};

		uint32 HighestIndex(const World& W, const PersonTypes& Persons)
		{
			uint32 Highest = 0;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach([&](EntityHandle, const PersonInfo& P) { Highest = P.Index > Highest ? P.Index : Highest; });
			return Highest;
		}

		uint32 LanguageOfCulture(const World& W, const History::PreHistoryTypes& Types, uint32 Culture)
		{
			uint32 Best = 0;
			uint64 Founded = 0;
			W.Components()
				.GetPool(Types.Languages.Language)
				.ForEach(
					[&](EntityHandle, const History::LanguageInfo& L)
					{
						if (L.Culture == Culture && (Best == 0 || L.Founded >= Founded))
						{
							Best = L.Index;
							Founded = L.Founded;
						}
					});
			return Best;
		}
	} // namespace

	LodTypes LodTypes::Declare(World& W)
	{
		LodTypes T;
		T.State = W.Types().Register<LodState>("LodState");
		W.Components().CreatePool(T.State);
		return T;
	}

	LodState& LodStateOf(World& W, const LodTypes& Types)
	{
		EntityHandle Found;
		W.Components()
			.GetPool(Types.State)
			.ForEach(
				[&](EntityHandle H, const LodState&)
				{
					if (Found.IsNull())
					{
						Found = H;
					}
				});
		if (Found.IsNull())
		{
			Found = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(Types.State).Add(Found, LodState{});
		}
		return W.Components().GetPool(Types.State).Get(Found);
	}

	bool RequestDetail(World& W, const LodTypes& Types, uint32 Region)
	{
		if (Region == 0)
		{
			return false;
		}
		LodState& S = LodStateOf(W, Types);
		for (uint32 i = 0; i < S.WantedCount; ++i)
		{
			if (S.Wanted[i] == Region)
			{
				return true;
			}
		}
		if (S.WantedCount >= LodState::MaxWanted)
		{
			return false;
		}
		S.Wanted[S.WantedCount] = Region;
		++S.WantedCount;
		return true;
	}

	bool ReleaseDetail(World& W, const LodTypes& Types, uint32 Region)
	{
		LodState& S = LodStateOf(W, Types);
		for (uint32 i = 0; i < S.WantedCount; ++i)
		{
			if (S.Wanted[i] == Region)
			{
				for (uint32 j = i + 1; j < S.WantedCount; ++j)
				{
					S.Wanted[j - 1] = S.Wanted[j];
				}
				--S.WantedCount;
				S.Wanted[S.WantedCount] = 0;
				return true;
			}
		}
		return false;
	}

	bool IsWanted(const World& W, const LodTypes& Types, uint32 Region)
	{
		bool Wanted = false;
		W.Components()
			.GetPool(Types.State)
			.ForEach(
				[&](EntityHandle, const LodState& S)
				{
					for (uint32 i = 0; i < S.WantedCount; ++i)
					{
						Wanted = Wanted || S.Wanted[i] == Region;
					}
				});
		return Wanted;
	}

	void LodSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr || Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		LodState& State = LodStateOf(W, Lod);

		// Region handles by index.
		std::vector<EntityHandle> Regions;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= Regions.size())
					{
						Regions.resize(usize{R.Index} + 1u);
					}
					Regions[R.Index] = H;
				});
		auto Detailed = [&](uint32 R)
		{
			return R < Regions.size() && !Regions[R].IsNull() &&
				   W.Components().GetPool(Persons.Detail).TryGet(Regions[R]) != nullptr;
		};

		// 1. Demotions: detailed regions nobody wants any more.
		std::vector<uint32> Current;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const RegionDetail& D) { Current.push_back(D.Region); });
		std::sort(Current.begin(), Current.end());
		for (const uint32 R : Current)
		{
			if (IsWanted(W, Lod, R))
			{
				continue;
			}
			const uint32 Folded = DemoteRegion(W, Types, Persons, R);
			++State.Demotions;
			Context.Events->Publish(Context.Tick, RegionDemotedEvent, LodPayload{R, Folded, State.Demotions, 0},
									W.Entities().GetId(Regions[R]));
		}
		// 2. Promotions: wanted regions, in request order, up to the limit.
		uint32 DetailedCount = 0;
		W.Components().GetPool(Persons.Detail).ForEach([&](EntityHandle, const RegionDetail&) { ++DetailedCount; });
		for (uint32 i = 0; i < State.WantedCount && DetailedCount < Rules.MaxDetailed; ++i)
		{
			const uint32 R = State.Wanted[i];
			if (R >= Regions.size() || Regions[R].IsNull() || Detailed(R))
			{
				continue;
			}
			const uint32 Made = PromoteRegion(W, Types, Persons, Rules.Materialise, R, Context.Tick);
			if (Made == 0)
			{
				++State.Refused;
				continue;
			}
			++State.Promotions;
			++DetailedCount;
			Context.Events->Publish(Context.Tick, RegionPromotedEvent, LodPayload{R, Made, State.Promotions, 0},
									W.Entities().GetId(Regions[R]));
		}

		// 3. Crossings, detailed region by detailed region.
		Current.clear();
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const RegionDetail& D) { Current.push_back(D.Region); });
		std::sort(Current.begin(), Current.end());
		if (Current.empty())
		{
			return;
		}
		const Hash64 Key = HashBytes(reinterpret_cast<const char*>(&W.Map().Config()), sizeof(W.Map().Config()));
		if (Graph.Neighbours.empty() || GraphDigest != Key)
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Types.World.Regions);
			GraphDigest = Key;
		}
		uint32 NextIndex = HighestIndex(W, Persons);
		for (const uint32 D : Current)
		{
			if (D >= Graph.Neighbours.size() || D >= Regions.size() || Regions[D].IsNull())
			{
				continue;
			}
			History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(Regions[D]);
			if (Counts == nullptr)
			{
				continue;
			}
			bool Changed = false;
			auto CrowdLine = [&](const History::RegionPopulation& P)
			{ return static_cast<uint32>(uint64{P.Capacity} * Rules.CrowdedPerMille / 1000u); };
			auto RoomLine = [&](const History::RegionPopulation& P)
			{ return static_cast<uint32>(uint64{P.Capacity} * Rules.RoomPerMille / 1000u); };
			// 3a. Leaving: the crowd over the line goes to the neighbour with the most room.
			if (Counts->Total > CrowdLine(*Counts))
			{
				uint32 Movers =
					static_cast<uint32>(uint64{Counts->Total - CrowdLine(*Counts)} * Rules.LeaveSharePerMille / 1000u);
				uint32 Best = 0;
				uint32 BestRoom = 0;
				for (const uint16 N : Graph.Neighbours[D])
				{
					if (N >= Regions.size() || Regions[N].IsNull() || Detailed(N))
					{
						continue;
					}
					const History::RegionPopulation* Other =
						W.Components().GetPool(Types.Population.Population).TryGet(Regions[N]);
					const uint32 Room =
						Other != nullptr && RoomLine(*Other) > Other->Total ? RoomLine(*Other) - Other->Total : 0u;
					if (Room > BestRoom)
					{
						BestRoom = Room;
						Best = N;
					}
				}
				Movers = std::min(Movers, BestRoom);
				if (Movers > 0 && Best != 0)
				{
					std::vector<Ref> Candidates;
					W.Components()
						.GetPool(Persons.Person)
						.ForEach(
							[&](EntityHandle H, const PersonInfo& P)
							{
								if (P.Region != D || P.State != static_cast<uint8>(LifeState::Alive) || P.Spouse != 0)
								{
									return;
								}
								const uint32 Age = AgeYears(P, Context.Tick);
								if (Age >= Rules.MoverFrom && Age < Rules.MoverTo)
								{
									Candidates.push_back(Ref{H, P});
								}
							});
					std::sort(Candidates.begin(), Candidates.end(),
							  [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
					History::RegionPopulation& Dest =
						W.Components().GetPool(Types.Population.Population).Get(Regions[Best]);
					History::RegionFaith* DestFaith =
						W.Components().GetPool(Types.Religion.Faith).TryGet(Regions[Best]);
					for (usize i = 0; i < Candidates.size() && i < Movers; ++i)
					{
						const Ref& R = Candidates[i];
						if (!Dest.Add(R.Info.Culture, 1))
						{
							break;
						}
						if (DestFaith != nullptr && R.Info.Religion != 0)
						{
							DestFaith->Add(R.Info.Religion, 1);
						}
						Context.Events->Publish(Context.Tick, PersonLeftEvent,
												PersonPayload{R.Info.Index, D, AgeYears(R.Info, Context.Tick), Best},
												W.Entities().GetId(R.Handle));
						// The person stays in the world as history, out of every count.
						W.Components().GetPool(Persons.Person).Get(R.Handle).State =
							static_cast<uint8>(LifeState::Gone);
						++State.Emigrants;
						Changed = true;
					}
				}
			}
			// 3b. Arriving: crowded coarse neighbours send people while there is room.
			for (const uint16 N : Graph.Neighbours[D])
			{
				if (N >= Regions.size() || Regions[N].IsNull() || Detailed(N))
				{
					continue;
				}
				History::RegionPopulation* Other =
					W.Components().GetPool(Types.Population.Population).TryGet(Regions[N]);
				if (Other == nullptr || Other->Total <= CrowdLine(*Other) || Other->Majority == 0)
				{
					continue;
				}
				const uint32 Room = RoomLine(*Counts) > Counts->Total ? RoomLine(*Counts) - Counts->Total : 0u;
				uint32 Movers =
					static_cast<uint32>(uint64{Other->Total - CrowdLine(*Other)} * Rules.ArriveSharePerMille / 1000u);
				Movers = std::min(Movers, Room);
				History::RegionFaith* OtherFaith = W.Components().GetPool(Types.Religion.Faith).TryGet(Regions[N]);
				for (uint32 i = 0; i < Movers; ++i)
				{
					const uint32 Culture = Other->Majority;
					if (Culture == 0 || Other->Remove(Culture, 1) == 0)
					{
						break;
					}
					uint32 Religion = 0;
					if (OtherFaith != nullptr && OtherFaith->Majority != 0)
					{
						const uint32 Faith = OtherFaith->Majority;
						Religion = OtherFaith->Remove(Faith, 1) > 0 ? Faith : 0u;
					}
					PersonInfo P;
					P.Index = ++NextIndex;
					P.Region = D;
					P.Culture = Culture;
					P.Religion = Religion;
					P.Language = LanguageOfCulture(W, Types, Culture);
					const uint32 Span = Rules.MoverTo > Rules.MoverFrom ? Rules.MoverTo - Rules.MoverFrom : 1u;
					const uint32 Age = Rules.MoverFrom + static_cast<uint32>(Random.Below(Span));
					const uint64 AgeTicks = uint64{Age} * History::TicksPerYear + Random.Below(History::TicksPerYear);
					P.Born = Context.Tick >= AgeTicks ? Context.Tick - AgeTicks : 0u;
					P.Sex = static_cast<uint8>(Random.Below(1000) < Rules.FemalePerMille ? Sex::Female : Sex::Male);
					P.State = static_cast<uint8>(LifeState::Alive);
					P.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x41525256ull, static_cast<int32>(P.Index),
													static_cast<int32>(D));
					const EntityHandle E = W.CreateEntity(IdKind::Person);
					W.Components().GetPool(Persons.Person).Add(E, P);
					Context.Events->Publish(Context.Tick, PersonArrivedEvent, PersonPayload{P.Index, D, Age, N},
											W.Entities().GetId(E));
					++State.Immigrants;
					Counts = W.Components().GetPool(Types.Population.Population).TryGet(Regions[D]);
					Other = W.Components().GetPool(Types.Population.Population).TryGet(Regions[N]);
					OtherFaith = W.Components().GetPool(Types.Religion.Faith).TryGet(Regions[N]);
					Changed = true;
					if (Counts == nullptr || Other == nullptr)
					{
						break;
					}
					++Counts->Total; // provisional, so the room shrinks; reconciled below
				}
				if (Counts == nullptr)
				{
					break;
				}
			}
			if (Changed)
			{
				ReconcileRegion(W, Types, Persons, D);
			}
		}
	}

	LodStats MeasureLod(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
						const LodTypes& Types_)
	{
		(void)Types;
		LodStats S;
		W.Components().GetPool(Persons.Detail).ForEach([&](EntityHandle, const RegionDetail&) { ++S.Detailed; });
		W.Components()
			.GetPool(Types_.State)
			.ForEach(
				[&](EntityHandle, const LodState& L)
				{
					S.Wanted = L.WantedCount;
					S.Promotions = L.Promotions;
					S.Demotions = L.Demotions;
					S.Emigrants = L.Emigrants;
					S.Immigrants = L.Immigrants;
					S.Refused = L.Refused;
				});
		for (const Event& E : W.Log().All())
		{
			S.LeftEvents += E.Is(PersonLeftEvent) ? 1u : 0u;
			S.ArrivedEvents += E.Is(PersonArrivedEvent) ? 1u : 0u;
		}
		return S;
	}
} // namespace Vaelen::Population
