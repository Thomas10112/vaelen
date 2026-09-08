// VAELEN - VaelenInfrastructure
// Phase 09.03: settlements as places.
//
// STATUS: PROTOTYPE (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/Places.h"

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
		constexpr uint64 PlaceSalt = 0x504c414345ull; // "PLACE"

		/// Squared distance between two tiles, in tiles. Small numbers: a region
		/// is a few dozen tiles across at the sizes this project runs.
		uint64 Apart(const WorldGrid& Grid, uint32 A, uint32 B) noexcept
		{
			const TileCoord CA = Grid.CoordOf(A);
			const TileCoord CB = Grid.CoordOf(B);
			const int64 DX = int64{CA.X} - int64{CB.X};
			const int64 DY = int64{CA.Y} - int64{CB.Y};
			return static_cast<uint64>(DX * DX + DY * DY);
		}
	} // namespace

	PlaceTypes PlaceTypes::Declare(World& W)
	{
		PlaceTypes T;
		T.Place = W.Types().Register<PlaceInfo>("PlaceInfo");
		T.At = W.Types().Register<BuildingPlace>("BuildingPlace");
		W.Components().CreatePool(T.Place);
		W.Components().CreatePool(T.At);
		return T;
	}

	void PlaceSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		if (!W.Map().IsReady())
		{
			return;
		}

		// 1. The regions, by index, and how many live on each.
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
		std::vector<uint32> Centre(N, 0u);
		std::vector<uint64> People(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const WorldGen::RegionInfo* Info =
				W.Components().GetPool(Types.World.RegionTypes_.Region).TryGet(RegionHandles[R]);
			Centre[R] = Info != nullptr ? Info->CentroidTile : 0u;
			const History::RegionPopulation* Counts =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
			People[R] = Counts != nullptr ? Counts->Total : 0u;
		}

		// 2. Every settlement of 06.04, in the order it was founded.
		struct Town
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Region = 0;
			uint32 Traffic = 0;
			bool Alive = false;
		};
		std::vector<Town> Towns;
		W.Components()
			.GetPool(Trade.Settlement)
			.ForEach([&](EntityHandle H, const Economy::SettlementInfo& S)
					 { Towns.push_back(Town{H, S.Index, S.Region, S.Traffic, S.Abandoned == 0}); });
		std::sort(Towns.begin(), Towns.end(), [](const Town& A, const Town& B) { return A.Index < B.Index; });
		if (Towns.empty())
		{
			return;
		}

		// 3. Tiles already spoken for, so that two places never share one.
		std::vector<uint32> Taken;
		W.Components()
			.GetPool(Places.Place)
			.ForEach([&](EntityHandle, const PlaceInfo& P) { Taken.push_back(P.Tile); });
		std::sort(Taken.begin(), Taken.end());

		// Where a town goes: on water where the region has water, at its heart
		// where it has none. Chosen once; a place never moves afterwards.
		const WorldGrid Grid = W.Map().Grid();
		const TileLayer<uint16>& RegionIx = W.Map().GetLayer(Types.World.Regions.RegionIndex);
		const TileLayer<uint16>& RiverIx = W.Map().GetLayer(Types.World.Hydro.RiverIndex);
		auto Choose = [&](uint32 Region) -> uint32
		{
			if (Region == 0 || Region >= N)
			{
				return 0;
			}
			const uint32 Heart = Centre[Region];
			uint32 BestWet = 0;
			uint64 BestWetApart = 0;
			uint32 BestDry = 0;
			uint64 BestDryApart = 0;
			const uint32 Tiles = Grid.TileCount();
			for (uint32 T = 0; T < Tiles; ++T)
			{
				if (RegionIx[T] != Region)
				{
					continue;
				}
				if (std::binary_search(Taken.begin(), Taken.end(), T))
				{
					continue;
				}
				const uint64 Far = Apart(Grid, T, Heart);
				if (RiverIx[T] != 0)
				{
					if (BestWet == 0 || Far < BestWetApart)
					{
						BestWet = T;
						BestWetApart = Far;
					}
				}
				else if (BestDry == 0 || Far < BestDryApart)
				{
					BestDry = T;
					BestDryApart = Far;
				}
			}
			return BestWet != 0 ? BestWet : BestDry;
		};

		// 4. The body of every settlement.
		std::vector<uint32> PlaceOfRegion(N, 0u);
		std::vector<uint32> TileOfRegion(N, 0u);
		for (const Town& T : Towns)
		{
			if (T.Region == 0 || T.Region >= N || RegionHandles[T.Region].IsNull())
			{
				continue;
			}
			PlaceInfo* Body = W.Components().GetPool(Places.Place).TryGet(T.Handle);
			if (Body == nullptr)
			{
				if (!T.Alive)
				{
					continue; // a settlement that died before it ever had a body
				}
				const uint32 Tile = Choose(T.Region);
				if (Tile == 0)
				{
					continue; // no ground left in the region to stand on
				}
				PlaceInfo Fresh;
				Fresh.Settlement = T.Index;
				Fresh.Region = T.Region;
				Fresh.Tile = Tile;
				Fresh.Size = 1;
				Fresh.Grown = 1;
				Fresh.Settled = Context.Tick;
				Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ PlaceSalt, static_cast<int32>(T.Index),
													static_cast<int32>(T.Region));
				W.Components().GetPool(Places.Place).Add(T.Handle, Fresh);
				Taken.insert(std::lower_bound(Taken.begin(), Taken.end(), Tile), Tile);
				Context.Events->Publish(Context.Tick, PlaceSettledEvent, PlacePayload{T.Index, T.Region, Tile, 1u},
										W.Entities().GetId(T.Handle));
				Body = W.Components().GetPool(Places.Place).TryGet(T.Handle);
				if (Body == nullptr)
				{
					continue;
				}
			}

			const uint32 Was = Body->Size;
			if (!T.Alive)
			{
				Body->Size = 0;
				Body->People = 0;
				if (Was != 0)
				{
					Context.Events->Publish(Context.Tick, PlaceEmptiedEvent,
											PlacePayload{T.Index, T.Region, Body->Tile, Was},
											W.Entities().GetId(T.Handle));
				}
				continue;
			}
			const uint64 Folk = People[T.Region] * Rules.TownSharePerMille / 1000u;
			const uint64 FromFolk = Rules.PeoplePerSize > 0 ? Folk / Rules.PeoplePerSize : 0u;
			const uint64 FromTrade = Rules.TrafficPerSize > 0 ? uint64{T.Traffic} / Rules.TrafficPerSize : 0u;
			const uint32 Size =
				static_cast<uint32>(std::min<uint64>(Rules.MostSize, std::max<uint64>(1u, FromFolk + FromTrade)));
			Body->People = static_cast<uint32>(Folk);
			Body->Size = Size;
			Body->Grown = std::max(Body->Grown, Size);
			if (PlaceOfRegion[T.Region] == 0)
			{
				PlaceOfRegion[T.Region] = T.Index; // the oldest one still standing
				TileOfRegion[T.Region] = Body->Tile;
			}
			if (Size != Was)
			{
				Context.Events->Publish(Context.Tick, PlaceGrewEvent, PlacePayload{T.Index, T.Region, Body->Tile, Size},
										W.Entities().GetId(T.Handle));
			}
		}

		// 5. Where every building of a region actually stands: in the town where
		//    the region has one, in the countryside where it has not.
		std::vector<uint32> Works(N, 0u);
		{
			std::vector<std::pair<uint32, EntityHandle>> All;
			W.Components()
				.GetPool(Buildings.Building)
				.ForEach([&](EntityHandle H, const BuildingInfo& B) { All.push_back({B.Index, H}); });
			std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
			for (const auto& [Index, H] : All)
			{
				const BuildingInfo* B = W.Components().GetPool(Buildings.Building).TryGet(H);
				if (B == nullptr || B->Region == 0 || B->Region >= N)
				{
					continue;
				}
				const bool InTown = Rules.WorksInTown != 0 && PlaceOfRegion[B->Region] != 0;
				const BuildingPlace Where{InTown ? PlaceOfRegion[B->Region] : 0u,
										  InTown ? TileOfRegion[B->Region] : 0u};
				BuildingPlace* Held = W.Components().GetPool(Places.At).TryGet(H);
				if (Held == nullptr)
				{
					W.Components().GetPool(Places.At).Add(H, Where);
				}
				else
				{
					*Held = Where;
				}
				if (InTown && B->Fell == 0)
				{
					++Works[B->Region];
				}
			}
		}
		W.Components()
			.GetPool(Places.Place)
			.ForEach([&](EntityHandle, PlaceInfo& P)
					 { P.Works = P.Region != 0 && P.Region < N && P.Size != 0 ? Works[P.Region] : 0u; });
	}

	const PlaceInfo* PlaceOf(const World& W, const PlaceTypes& Places, uint32 Settlement)
	{
		const PlaceInfo* Found = nullptr;
		if (Settlement == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Places.Place)
			.ForEach(
				[&](EntityHandle, const PlaceInfo& P)
				{
					if (P.Settlement == Settlement && Found == nullptr)
					{
						Found = &P;
					}
				});
		return Found;
	}

	const PlaceInfo* PlaceIn(const World& W, const PlaceTypes& Places, uint32 Region)
	{
		const PlaceInfo* Found = nullptr;
		if (Region == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Places.Place)
			.ForEach(
				[&](EntityHandle, const PlaceInfo& P)
				{
					// A region can have held several settlements over the centuries,
					// most of them abandoned. The one standing is the one meant;
					// the oldest ruin answers only when nothing stands.
					if (P.Region != Region)
					{
						return;
					}
					const bool Better = Found == nullptr || (P.Size != 0 && Found->Size == 0) ||
										((P.Size != 0) == (Found->Size != 0) && P.Settlement < Found->Settlement);
					if (Better)
					{
						Found = &P;
					}
				});
		return Found;
	}

	const BuildingPlace* PlaceOfBuilding(const World& W, const InfrastructureTypes& Buildings, const PlaceTypes& Places,
										 uint32 Building)
	{
		const BuildingPlace* Found = nullptr;
		if (Building == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Buildings.Building)
			.ForEach(
				[&](EntityHandle H, const BuildingInfo& B)
				{
					if (B.Index == Building && Found == nullptr)
					{
						Found = W.Components().GetPool(Places.At).TryGet(H);
					}
				});
		return Found;
	}

	PlaceStats MeasurePlaces(const World& W, const History::PreHistoryTypes& Types, const Economy::TradeTypes& Trade,
							 const InfrastructureTypes& Buildings, const PlaceTypes& Places, const PlaceRules& Rules)
	{
		PlaceStats S;
		// What trade says about every settlement.
		std::vector<std::pair<uint32, uint8>> Alive; // index -> still standing
		std::vector<std::pair<uint32, uint32>> Where;
		W.Components()
			.GetPool(Trade.Settlement)
			.ForEach(
				[&](EntityHandle, const Economy::SettlementInfo& T)
				{
					Alive.push_back({T.Index, T.Abandoned == 0 ? uint8{1} : uint8{0}});
					Where.push_back({T.Index, T.Region});
				});
		std::sort(Alive.begin(), Alive.end());
		std::sort(Where.begin(), Where.end());

		std::vector<PlaceInfo> All;
		W.Components().GetPool(Places.Place).ForEach([&](EntityHandle, const PlaceInfo& P) { All.push_back(P); });
		std::sort(All.begin(), All.end(),
				  [](const PlaceInfo& A, const PlaceInfo& B) { return A.Settlement < B.Settlement; });

		// What the buildings say stands in every town.
		std::vector<std::pair<uint32, uint32>> InTown; // settlement -> standing works
		W.Components()
			.GetPool(Buildings.Building)
			.ForEach(
				[&](EntityHandle H, const BuildingInfo& B)
				{
					const BuildingPlace* At = W.Components().GetPool(Places.At).TryGet(H);
					if (At == nullptr)
					{
						return;
					}
					if (At->Settlement == 0)
					{
						S.Loose += B.Fell == 0 ? 1u : 0u;
						return;
					}
					if (B.Fell != 0)
					{
						return;
					}
					++S.Works;
					bool Seen = false;
					for (auto& [Town, Count] : InTown)
					{
						if (Town == At->Settlement)
						{
							++Count;
							Seen = true;
						}
					}
					if (!Seen)
					{
						InTown.push_back({At->Settlement, 1u});
					}
				});
		std::sort(InTown.begin(), InTown.end());

		const bool Mapped = W.Map().IsReady();
		const WorldGrid Grid = Mapped ? W.Map().Grid() : WorldGrid{};
		const TileLayer<uint16>* RegionIx = Mapped ? &W.Map().GetLayer(Types.World.Regions.RegionIndex) : nullptr;

		std::vector<uint32> Tiles;
		Hash64 D = HashString("Places");
		uint32 Previous = 0;
		for (const PlaceInfo& P : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&P), sizeof(P)));
			++S.Places;
			if (P.Settlement == 0 || P.Settlement == Previous)
			{
				++S.Bad; // a settlement with two bodies, or none
			}
			Previous = P.Settlement;

			const auto Found = std::lower_bound(Alive.begin(), Alive.end(), std::pair<uint32, uint8>{P.Settlement, 0u});
			if (Found == Alive.end() || Found->first != P.Settlement)
			{
				++S.Bad; // a body with no settlement under it
				continue;
			}
			const bool Standing = Found->second != 0;
			// A size is zero exactly when the settlement is gone.
			if ((Standing && P.Size == 0) || (!Standing && P.Size != 0))
			{
				++S.Bad;
			}
			if (P.Size > Rules.MostSize || P.Grown < P.Size)
			{
				++S.Bad; // past its cap, or a high-water mark that fell
			}
			const auto Home = std::lower_bound(Where.begin(), Where.end(), std::pair<uint32, uint32>{P.Settlement, 0u});
			if (Home == Where.end() || Home->first != P.Settlement || Home->second != P.Region)
			{
				++S.Bad; // standing in a region that is not its settlement's
			}
			if (RegionIx != nullptr)
			{
				if (P.Tile >= Grid.TileCount() || (*RegionIx)[P.Tile] != P.Region)
				{
					++S.Bad; // standing on ground that is not its region's
				}
			}
			Tiles.push_back(P.Tile);
			uint32 Says = 0;
			const auto Count =
				std::lower_bound(InTown.begin(), InTown.end(), std::pair<uint32, uint32>{P.Settlement, 0u});
			if (Count != InTown.end() && Count->first == P.Settlement)
			{
				Says = Count->second;
			}
			if (P.Works != Says)
			{
				++S.Bad; // the town and its buildings disagree on what stands in it
			}
			if (Standing)
			{
				++S.Standing;
				S.Townsfolk += P.People;
				S.Largest = std::max(S.Largest, P.Size);
			}
			else
			{
				++S.Emptied;
			}
		}
		std::sort(Tiles.begin(), Tiles.end());
		for (usize i = 1; i < Tiles.size(); ++i)
		{
			S.Bad += Tiles[i] == Tiles[i - 1] ? 1u : 0u; // two places on one tile
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Infrastructure
