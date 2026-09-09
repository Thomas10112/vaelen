// VAELEN - VaelenGameplay
// Phase 12 task 12.05: a name that travels.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic tests in Tests/Gameplay/Test_Fame.cpp
#include "Vaelen/Gameplay/Fame.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Gameplay
{
	namespace
	{
		bool IsAlive(const Population::PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(Population::LifeState::Alive);
		}

		int32 Magnitude(int32 Said) noexcept
		{
			return Said < 0 ? -Said : Said;
		}

		/// What a person's life adds up to, for the purpose of being spoken of.
		int32 Loudness(const PersonRepute& R, const FameRules& Rules) noexcept
		{
			const int64 Sum = int64{Rules.PerDeed} * int64{R.Kindnesses} + int64{Rules.PerWrong} * int64{R.Wrongs};
			const int64 Most = int64{Rules.Most};
			return static_cast<int32>(Sum > Most ? Most : (Sum < -Most ? -Most : Sum));
		}

		/// What survives a road. Truncating toward zero on purpose: a name too
		/// thin to survive one more telling stops rather than lingering at one.
		int32 Thinner(int32 Said, uint32 PerMille) noexcept
		{
			const int64 Scaled = (static_cast<int64>(Said) * static_cast<int64>(PerMille)) / 1000;
			return static_cast<int32>(Scaled);
		}

		/// The order a place says its names in: what is nearest first, then what
		/// is loudest, then by who - so the same world always keeps the same
		/// handful when more names arrive than a place can carry.
		bool Louder(const Fame& A, const Fame& B) noexcept
		{
			if (A.Hops != B.Hops)
			{
				return A.Hops < B.Hops;
			}
			const int32 MA = Magnitude(A.Said);
			const int32 MB = Magnitude(B.Said);
			if (MA != MB)
			{
				return MA > MB;
			}
			return A.Person < B.Person;
		}

		const Fame* Find(const std::vector<Fame>& In, uint32 Person) noexcept
		{
			for (const Fame& F : In)
			{
				if (F.Person == Person)
				{
					return &F;
				}
			}
			return nullptr;
		}

		Fame* FindMut(std::vector<Fame>& In, uint32 Person) noexcept
		{
			for (Fame& F : In)
			{
				if (F.Person == Person)
				{
					return &F;
				}
			}
			return nullptr;
		}

		/// A telling reaching a place that is already being told about the same
		/// person this year. The nearer account wins, and between two of equal
		/// distance the louder does - so a name that came straight down one road
		/// is not overwritten by the same name come the long way round.
		void Renew(std::vector<Fame>& Here, const Fame& In)
		{
			Fame* Ex = FindMut(Here, In.Person);
			if (Ex == nullptr)
			{
				Here.push_back(In);
				return;
			}
			const bool Nearer = In.Hops < Ex->Hops;
			const bool Louder_ = In.Hops == Ex->Hops && Magnitude(In.Said) > Magnitude(Ex->Said);
			if (Nearer || Louder_)
			{
				*Ex = In;
			}
		}
	} // namespace

	FameTypes FameTypes::Declare(World& W)
	{
		FameTypes T;
		T.Names = W.Types().Register<RegionNames>("RegionNames");
		W.Components().CreatePool(T.Names);
		return T;
	}

	void FameSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;

		// The ground, in index order, so nothing below depends on pool order.
		std::vector<uint32> Index;
		std::vector<EntityHandle> Handle;
		{
			std::vector<std::pair<uint32, EntityHandle>> Found;
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index != 0)
						{
							Found.emplace_back(R.Index, H);
						}
					});
			std::sort(Found.begin(), Found.end(),
					  [](const std::pair<uint32, EntityHandle>& A, const std::pair<uint32, EntityHandle>& B)
					  { return A.first < B.first; });
			for (const std::pair<uint32, EntityHandle>& P : Found)
			{
				Index.push_back(P.first);
				Handle.push_back(P.second);
			}
		}
		if (Index.empty())
		{
			return;
		}
		std::vector<int32> Slot(static_cast<usize>(Index.back()) + 1u, -1);
		for (usize i = 0; i < Index.size(); ++i)
		{
			Slot[Index[i]] = static_cast<int32>(i);
		}

		// What every place said last year.
		std::vector<std::vector<Fame>> Was(Index.size());
		for (usize i = 0; i < Index.size(); ++i)
		{
			const RegionNames* N = W.Components().GetPool(Fame_.Names).TryGet(Handle[i]);
			if (N == nullptr)
			{
				continue;
			}
			for (uint32 k = 0; k < N->Count && k < MostNames; ++k)
			{
				Was[i].push_back(N->Who[k]);
			}
		}

		std::vector<std::vector<Fame>> Now(Index.size());

		// A name starts where the person lives, out of what the people who have
		// dealt with them think (12.02). A person nobody thinks much of either
		// way has no name to carry, which is most people.
		{
			// One walk of the people, keeping who is spoken of and where they stand.
			std::vector<std::pair<uint32, Fame>> Seeds; // region, name
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const Population::PersonInfo& P)
					{
						if (!IsAlive(P) || P.Region == 0 || P.Region >= Slot.size() || Slot[P.Region] < 0)
						{
							return;
						}
						const PersonRepute* R = W.Components().GetPool(Repute.Repute).TryGet(H);
						if (R == nullptr)
						{
							return;
						}
						const int32 Said = Loudness(*R, Rules);
						if (Magnitude(Said) < Rules.WorthCarrying)
						{
							return;
						}
						Seeds.emplace_back(P.Region, Fame{static_cast<uint64>(Context.Tick), P.Index, Said, 0u, 0u});
					});
			std::sort(Seeds.begin(), Seeds.end(), [](const std::pair<uint32, Fame>& A, const std::pair<uint32, Fame>& B)
					  { return A.second.Person < B.second.Person; });
			for (const std::pair<uint32, Fame>& S : Seeds)
			{
				// A person at home always overwrites what the place remembers of
				// them: nobody's neighbours are behind on the news.
				std::vector<Fame>& Here = Now[static_cast<usize>(Slot[S.first])];
				Fame* Ex = FindMut(Here, S.second.Person);
				if (Ex == nullptr)
				{
					Here.push_back(S.second);
				}
				else
				{
					*Ex = S.second;
				}
			}
			for (std::vector<Fame>& Here : Now)
			{
				std::sort(Here.begin(), Here.end(), Louder);
				if (Here.size() > MostNames)
				{
					Here.resize(MostNames);
				}
			}
		}

		// The roads, in the order they were opened.
		std::vector<Economy::RouteInfo> Routes;
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle, const Economy::RouteInfo& R)
				{
					if (R.Closed == 0 && R.From != 0 && R.To != 0)
					{
						Routes.push_back(R);
					}
				});
		std::sort(Routes.begin(), Routes.end(),
				  [](const Economy::RouteInfo& A, const Economy::RouteInfo& B) { return A.Index < B.Index; });

		// A name crosses one road a year's worth of telling at a time, thinner
		// at every crossing. A place with no road to you has never heard of you,
		// however close it stands - which is the whole of why this waits for
		// 06.04 rather than reading the map.
		for (uint32 Hop = 1; Hop <= Rules.MostHops; ++Hop)
		{
			// Gathered before any of it is applied, so a name cannot cross two
			// roads in one round by arriving early on a low-numbered route.
			std::vector<std::vector<Fame>> Reached(Index.size());
			auto Carry = [&](uint32 FromRegion, uint32 ToRegion)
			{
				if (FromRegion >= Slot.size() || ToRegion >= Slot.size() || Slot[FromRegion] < 0 || Slot[ToRegion] < 0)
				{
					return;
				}
				const usize A = static_cast<usize>(Slot[FromRegion]);
				const usize B = static_cast<usize>(Slot[ToRegion]);
				for (const Fame& F : Now[A])
				{
					if (F.Hops != Hop - 1u)
					{
						continue; // it crossed its road in an earlier round
					}
					const int32 Said = Thinner(F.Said, Rules.PerHopPerMille);
					if (Said == 0)
					{
						continue; // too thin to be worth repeating
					}
					Renew(Reached[B], Fame{static_cast<uint64>(Context.Tick), F.Person, Said, Hop, 0u});
				}
			};
			for (const Economy::RouteInfo& R : Routes)
			{
				Carry(R.From, R.To);
				Carry(R.To, R.From);
			}
			for (usize i = 0; i < Index.size(); ++i)
			{
				for (const Fame& F : Reached[i])
				{
					Renew(Now[i], F);
				}
				std::sort(Now[i].begin(), Now[i].end(), Louder);
				if (Now[i].size() > MostNames)
				{
					Now[i].resize(MostNames);
				}
			}
		}

		for (usize i = 0; i < Index.size(); ++i)
		{
			// Say what changed, before writing it down. A place that was already
			// saying a name keeps the year it first heard it - that, and not a
			// decay, is what a place's memory amounts to here.
			for (Fame& F : Now[i])
			{
				const Fame* Old = Find(Was[i], F.Person);
				if (Old != nullptr)
				{
					F.Since = Old->Since;
					continue;
				}
				Context.Events->Publish(Context.Tick, NameTravelledEvent,
										FamePayload{F.Person, Index[i], F.Said, F.Hops});
			}
			for (const Fame& F : Was[i])
			{
				if (Find(Now[i], F.Person) == nullptr)
				{
					Context.Events->Publish(Context.Tick, NameForgottenEvent,
											FamePayload{F.Person, Index[i], F.Said, F.Hops});
				}
			}

			RegionNames* N = W.Components().GetPool(Fame_.Names).TryGet(Handle[i]);
			if (N == nullptr)
			{
				if (Now[i].empty())
				{
					continue; // a place that has heard nothing carries nothing
				}
				N = &W.Components().GetPool(Fame_.Names).Add(Handle[i], RegionNames{});
			}
			*N = RegionNames{};
			for (usize k = 0; k < Now[i].size() && k < MostNames; ++k)
			{
				N->Who[k] = Now[i][k];
				++N->Count;
			}
		}
	}

	const Fame* FameIn(const World& W, const History::PreHistoryTypes& Types, const FameTypes& Types_, uint32 Region,
					   uint32 Person)
	{
		const Fame* Out = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index != Region || Out != nullptr)
					{
						return;
					}
					const RegionNames* N = W.Components().GetPool(Types_.Names).TryGet(H);
					if (N == nullptr)
					{
						return;
					}
					for (uint32 k = 0; k < N->Count && k < MostNames; ++k)
					{
						if (N->Who[k].Person == Person)
						{
							Out = &N->Who[k];
							return;
						}
					}
				});
		return Out;
	}

	FameStats MeasureFame(const World& W, const FameTypes& Types)
	{
		FameStats Out;
		std::vector<const RegionNames*> All;
		W.Components().GetPool(Types.Names).ForEach([&](EntityHandle, const RegionNames& N) { All.push_back(&N); });
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		// Places in the order their names put them, so the digest does not ride
		// on which region entity was made first.
		std::sort(All.begin(), All.end(),
				  [](const RegionNames* A, const RegionNames* B)
				  {
					  if (A->Count != B->Count)
					  {
						  return A->Count < B->Count;
					  }
					  for (uint32 k = 0; k < A->Count && k < MostNames; ++k)
					  {
						  if (A->Who[k].Person != B->Who[k].Person)
						  {
							  return A->Who[k].Person < B->Who[k].Person;
						  }
						  if (A->Who[k].Said != B->Who[k].Said)
						  {
							  return A->Who[k].Said < B->Who[k].Said;
						  }
					  }
					  return false;
				  });
		for (const RegionNames* N : All)
		{
			if (N->Count == 0)
			{
				continue;
			}
			++Out.Places;
			for (uint32 k = 0; k < N->Count && k < MostNames; ++k)
			{
				++Out.Names;
				Out.Abroad += N->Who[k].Hops > 0 ? 1u : 0u;
				Out.Furthest = N->Who[k].Hops > Out.Furthest ? N->Who[k].Hops : Out.Furthest;
				Out.Loudness += static_cast<uint64>(Magnitude(N->Who[k].Said));
				Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&N->Who[k]), sizeof(Fame)));
			}
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
