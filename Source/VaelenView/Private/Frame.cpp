// VAELEN - VaelenView
// Phase 13 task 13.01: a read-only view of the world for a frame.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Frame.cpp
#include "Vaelen/View/Frame.h"

#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::View
{
	namespace
	{
		bool IsAlive(const Population::PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(Population::LifeState::Alive);
		}
	} // namespace

	void TakeView(const World& W, const ViewSources& From, WorldView& Out)
	{
		Out.Regions.clear();
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		Out.Width = W.Map().Config().Width;
		Out.Height = W.Map().Config().Height;
		Out.People = 0;
		Out.Played = From.HasPlayer ? Player::PlayerPerson(W, From.Played) : 0u;

		// The ground, in index order. A renderer redraws the same region in the
		// same slot every frame, which it cannot do if the order rides on which
		// entity happened to be made first.
		std::vector<std::pair<uint32, EntityHandle>> Ground;
		W.Components()
			.GetPool(From.Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index != 0)
					{
						Ground.emplace_back(R.Index, H);
					}
				});
		std::sort(Ground.begin(), Ground.end(),
				  [](const std::pair<uint32, EntityHandle>& A, const std::pair<uint32, EntityHandle>& B)
				  { return A.first < B.first; });
		if (Ground.empty())
		{
			return;
		}

		// Everything counted per region in ONE walk of each pool rather than a
		// walk per region. A frame is taken sixty times a second and the person
		// pool holds every person the world has ever made.
		const uint32 Most = Ground.back().first;
		std::vector<uint32> Standing(static_cast<usize>(Most) + 1u, 0u);
		std::vector<uint32> Held(static_cast<usize>(Most) + 1u, 0u);
		W.Components()
			.GetPool(From.Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (!IsAlive(P) || P.Region == 0 || P.Region > Most)
					{
						return;
					}
					++Standing[P.Region];
					if (From.HasBondage && W.Components().GetPool(From.Bondage.Bond).TryGet(H) != nullptr)
					{
						++Held[P.Region];
					}
				});
		std::vector<uint32> Ways(static_cast<usize>(Most) + 1u, 0u);
		std::vector<uint32> Seats(static_cast<usize>(Most) + 1u, 0u);
		if (From.HasTrade)
		{
			W.Components()
				.GetPool(From.Trade.Route)
				.ForEach(
					[&](EntityHandle, const Economy::RouteInfo& R)
					{
						if (R.Closed != 0)
						{
							return;
						}
						Ways[R.From <= Most ? R.From : 0u] += R.From <= Most && R.From != 0 ? 1u : 0u;
						Ways[R.To <= Most ? R.To : 0u] += R.To <= Most && R.To != 0 ? 1u : 0u;
					});
			W.Components()
				.GetPool(From.Trade.Settlement)
				.ForEach(
					[&](EntityHandle, const Economy::SettlementInfo& S)
					{
						if (S.Abandoned == 0 && S.Region != 0 && S.Region <= Most)
						{
							Seats[S.Region] = S.Index;
						}
					});
		}

		Out.Regions.reserve(Ground.size());
		for (const std::pair<uint32, EntityHandle>& G : Ground)
		{
			const WorldGen::RegionInfo* R =
				W.Components().GetPool(From.Types.World.RegionTypes_.Region).TryGet(G.second);
			if (R == nullptr)
			{
				continue;
			}
			RegionView V;
			V.Index = R->Index;
			V.CentroidTile = R->CentroidTile;
			V.Tiles = R->Tiles;
			V.Biome = R->DominantBiome;
			V.Elevation = R->MeanElevation;
			// The people who are actually there, when the world is simulating
			// them; the aggregate count when it is not. A renderer must not draw
			// an empty region because the simulation stopped tracking heads.
			const History::RegionPopulation* P =
				W.Components().GetPool(From.Types.Population.Population).TryGet(G.second);
			V.Detailed = Standing[R->Index] != 0 ? 1u : 0u;
			V.People = V.Detailed != 0 ? Standing[R->Index] : (P != nullptr ? P->Total : 0u);
			V.Bound = Held[R->Index];
			V.Settlement = Seats[R->Index];
			V.Roads = Ways[R->Index];
			if (From.HasFame)
			{
				const Gameplay::RegionNames* N = W.Components().GetPool(From.Fame.Names).TryGet(G.second);
				V.Names = N != nullptr ? N->Count : 0u;
			}
			Out.People += V.People;
			Out.Regions.push_back(V);
		}
	}

	const RegionView* RegionIn(const WorldView& V, uint32 Region)
	{
		const auto At = std::lower_bound(V.Regions.begin(), V.Regions.end(), Region,
										 [](const RegionView& A, uint32 B) { return A.Index < B; });
		return At != V.Regions.end() && At->Index == Region ? &*At : nullptr;
	}

	ViewStats MeasureView(const WorldView& V)
	{
		ViewStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const RegionView& R : V.Regions)
		{
			++Out.Regions;
			Out.Peopled += R.People != 0 ? 1u : 0u;
			Out.Detailed += R.Detailed != 0 ? 1u : 0u;
			Out.Named += R.Names != 0 ? 1u : 0u;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&R), sizeof(RegionView)));
		}
		Out.Bytes = static_cast<uint32>(sizeof(WorldView) + V.Regions.size() * sizeof(RegionView));
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
