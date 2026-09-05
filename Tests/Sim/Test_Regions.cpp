// VAELEN - VaelenSim tests
// WorldGen 02.06: regions - exact cover of the land, contiguity, size floor,
// adjacency graph symmetry, entities, two-island synthetic map, determinism,
// snapshot round trip, parameter sensitivity, frozen digests, baseline.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <chrono>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.06).
#define VAELEN_REGION_FROZEN_256 0x02f8414a20107c30ull
#define VAELEN_REGION_COUNT_256 126u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogRegions);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct GenWorld
	{
		explicit GenWorld(uint64 Seed) : Instance(Config(Seed))
		{
			Layers = WorldLayers::Declare(Instance.Map());
			Hydro = HydroLayers::Declare(Instance.Map());
			Regions = RegionLayers::Declare(Instance.Map());
			Types = WorldTypes::Declare(Instance);
			RTypes = RegionTypes::Declare(Instance);
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool GenerateFrom(uint32 W, uint32 H, WorldGenConfig Gen = WorldGenConfig{})
		{
			Gen.Width = W;
			Gen.Height = H;
			const uint64 Seed = Instance.Config().Seed;
			return Instance.Map().Reset(Gen) && GenerateElevation(Instance.Map(), Layers, Seed) && Finish();
		}
		bool Finish()
		{
			const uint64 Seed = Instance.Config().Seed;
			return GenerateHydrology(Instance, Layers, Hydro, Types) && GenerateClimate(Instance.Map(), Layers, Seed) &&
				   GenerateRegions(Instance, Layers, Hydro, Regions, RTypes);
		}
		World Instance;
		WorldLayers Layers;
		HydroLayers Hydro;
		RegionLayers Regions;
		WorldTypes Types;
		RegionTypes RTypes;
	};

	/// Everything that must hold for any partition. Returns failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, const GenWorld& W)
	{
		uint32 Failures = 0;
		auto Fail = [&](const char* What)
		{
			++Failures;
			Ctx.ReportFailure(__FILE__, __LINE__, What);
		};
		const WorldMap& Map = W.Instance.Map();
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint16>& R = Map.GetLayer(W.Regions.RegionIndex);
		const RegionStats S = MeasureRegions(W.Instance, W.Layers, W.Regions, W.RTypes);
		if (S.UnassignedLand != 0 || S.AssignedSea != 0)
		{
			Fail("partition does not cover exactly the land");
		}
		const RegionParams P = RegionParams::Resolve(Map.Config());
		// Entities: index in range, tiles match, seed and centroid inside, contiguous.
		std::vector<uint32> Counted(S.Regions + 1, 0);
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			if (R[I] > S.Regions)
			{
				Fail("region index above the entity count");
				break;
			}
			++Counted[R[I]];
		}
		std::vector<uint8> Seen(Grid.TileCount(), 0);
		std::vector<uint32> Stack;
		W.Instance.Components()
			.GetPool(W.RTypes.Region)
			.ForEach(
				[&](EntityHandle H, const RegionInfo& RI)
				{
					if (!W.Instance.Entities().GetId(H).IsKind(IdKind::Region))
					{
						Fail("region entity of the wrong kind");
					}
					if (RI.Index == 0 || RI.Index > S.Regions || Counted[RI.Index] != RI.Tiles)
					{
						Fail("region tile count disagrees with the layer");
						return;
					}
					if (R[RI.SeedTile] != RI.Index || R[RI.CentroidTile] != RI.Index)
					{
						Fail("seed or centroid outside the region");
					}
					if (RI.DominantBiome == 0)
					{
						Fail("dominant biome is Ocean");
					}
					// Contiguity: flood from the seed reaches every tile of the region.
					Stack.clear();
					Stack.push_back(RI.SeedTile);
					Seen[RI.SeedTile] = 1;
					uint32 Reached = 0;
					while (!Stack.empty())
					{
						const uint32 I = Stack.back();
						Stack.pop_back();
						++Reached;
						Grid.ForEachNeighbour(Grid.CoordOf(I), 4,
											  [&](TileCoord NC, uint32)
											  {
												  const uint32 J = Grid.IndexOf(NC);
												  if (Seen[J] == 0 && R[J] == RI.Index)
												  {
													  Seen[J] = 1;
													  Stack.push_back(J);
												  }
											  });
					}
					if (Reached != RI.Tiles)
					{
						Fail("region is not contiguous");
					}
				});
		// Size floor: a region below the floor must be an island (no land neighbour region).
		const RegionGraph G = BuildRegionGraph(Map, W.Regions);
		W.Instance.Components()
			.GetPool(W.RTypes.Region)
			.ForEach(
				[&](EntityHandle, const RegionInfo& RI)
				{
					if (RI.Tiles < P.MinTiles && !G.Neighbours[RI.Index].empty())
					{
						Fail("region below the size floor has neighbours");
					}
				});
		// Graph symmetry.
		for (uint16 A = 1; A <= S.Regions; ++A)
		{
			for (usize K = 0; K < G.Neighbours[A].size(); ++K)
			{
				const uint16 B = G.Neighbours[A][K];
				if (!G.AreAdjacent(B, A))
				{
					Fail("adjacency is not symmetric");
					break;
				}
				// Shared border symmetric too.
				const auto It = std::lower_bound(G.Neighbours[B].begin(), G.Neighbours[B].end(), A);
				const usize At = static_cast<usize>(It - G.Neighbours[B].begin());
				if (G.SharedBorder[B][At] != G.SharedBorder[A][K])
				{
					Fail("shared border is not symmetric");
					break;
				}
			}
			if (G.AreAdjacent(A, A))
			{
				Fail("region adjacent to itself");
			}
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(Regions, AelvorIsPartitionedIntoContiguousRegions)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.GenerateFrom(256, 256));
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const RegionStats S = MeasureRegions(W.Instance, W.Layers, W.Regions, W.RTypes);
	VAELEN_LOG_INFO(LogRegions, "256: %u regions, sizes %u..%u tiles, max %u neighbours", S.Regions, S.SmallestTiles,
					S.LargestTiles, S.MaxNeighbours);
	VT_CHECK(S.Regions >= 30 && S.Regions <= 200);
	VT_CHECK(S.LargestTiles < 256u * 256u / 10u);
	VT_CHECK(S.MaxNeighbours >= 4 && S.MaxNeighbours <= 16);
	std::string Picture;
	ExportRegionAscii(W.Instance.Map(), W.Regions, 64, Picture);
	VT_CHECK_EQ(Picture.size(), 64u * 32u + 32u);
	for (usize Row = 0; Row < 32; Row += 8)
	{
		const std::string Slice = Picture.substr(Row * 65, 8 * 65);
		VAELEN_LOG_INFO(LogRegions, "AELVOR regions at 256, rows %zu-%zu:\n%s", Row, Row + 7, Slice.c_str());
	}
}

VAELEN_TEST(Regions, TwoIslandsNeverShareARegion)
{
	// 48 x 24: two square islands separated by a sea channel.
	GenWorld W(3);
	WorldGenConfig Gen;
	Gen.Width = 48;
	Gen.Height = 24;
	VT_REQUIRE(W.Instance.Map().Reset(Gen));
	TileLayer<int64>& E = W.Instance.Map().GetLayer(W.Layers.Elevation);
	for (uint32 Y = 0; Y < 24; ++Y)
	{
		for (uint32 X = 0; X < 48; ++X)
		{
			const bool West = X >= 3 && X < 20 && Y >= 3 && Y < 21;
			const bool East = X >= 28 && X < 45 && Y >= 3 && Y < 21;
			E[Y * 48 + X] = Fix64::FromInt(West || East ? 200 + static_cast<int32>((X * 7 + Y * 3) % 50) : -300).Raw;
		}
	}
	ClassifyTerrain(W.Instance.Map(), W.Layers);
	VT_REQUIRE(W.Finish());
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const TileLayer<uint16>& R = W.Instance.Map().GetLayer(W.Regions.RegionIndex);
	std::vector<uint8> OnWest(MeasureRegions(W.Instance, W.Layers, W.Regions, W.RTypes).Regions + 1, 0);
	std::vector<uint8> OnEast(OnWest.size(), 0);
	for (uint32 Y = 0; Y < 24; ++Y)
	{
		for (uint32 X = 0; X < 48; ++X)
		{
			const uint16 V = R[Y * 48 + X];
			if (V == 0)
			{
				continue;
			}
			(X < 24 ? OnWest : OnEast)[V] = 1;
		}
	}
	uint32 Straddling = 0;
	uint32 WestRegions = 0;
	uint32 EastRegions = 0;
	for (usize K = 1; K < OnWest.size(); ++K)
	{
		Straddling += (OnWest[K] != 0 && OnEast[K] != 0) ? 1u : 0u;
		WestRegions += OnWest[K];
		EastRegions += OnEast[K];
	}
	VT_CHECK_EQ(Straddling, 0u);
	VT_CHECK(WestRegions >= 1 && EastRegions >= 1);
	// The graph has no edge between the islands.
	const RegionGraph G = BuildRegionGraph(W.Instance.Map(), W.Regions);
	for (usize A = 1; A < OnWest.size(); ++A)
	{
		for (uint16 B : G.Neighbours[A])
		{
			VT_CHECK(OnWest[A] == OnWest[B]);
		}
	}
}

VAELEN_TEST(Regions, DeterministicSensitiveAndSnapshotSafe)
{
	GenWorld A(9);
	GenWorld B(9);
	GenWorld C(10);
	VT_REQUIRE(A.GenerateFrom(64, 64) && B.GenerateFrom(64, 64) && C.GenerateFrom(64, 64));
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_NE(ComputeStateDigest(A.Instance), ComputeStateDigest(C.Instance));
	VT_CHECK_EQ(CheckInvariants(Ctx, C), 0u);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	GenWorld Restored(9);
	VT_REQUIRE(LoadSnapshot(Restored.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(Restored.Instance.Map().LayerCount(), 13u);
	VT_CHECK_EQ(ComputeStateDigest(Restored.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(CheckInvariants(Ctx, Restored), 0u);
	// Wider spacing: fewer regions.
	WorldGenConfig Coarse;
	Coarse.Params[ParamIndex::RegionSpacing] = Fix64::FromRatio(1, 4).Raw;
	GenWorld D(9);
	VT_REQUIRE(D.GenerateFrom(64, 64, Coarse));
	VT_CHECK(MeasureRegions(D.Instance, D.Layers, D.Regions, D.RTypes).Regions <
			 MeasureRegions(A.Instance, A.Layers, A.Regions, A.RTypes).Regions);
	VT_CHECK_EQ(CheckInvariants(Ctx, D), 0u);
	// A rerun replaces the entities.
	const uint32 Before = A.Instance.Entities().GetAliveCount();
	VT_REQUIRE(GenerateRegions(A.Instance, A.Layers, A.Hydro, A.Regions, A.RTypes));
	VT_CHECK_EQ(A.Instance.Entities().GetAliveCount(), Before);
	// Misuse.
	VaelenTest::ScopedAssertCapture Capture;
	GenWorld Empty(1);
	VT_CHECK(!GenerateRegions(Empty.Instance, Empty.Layers, Empty.Hydro, Empty.Regions, Empty.RTypes));
	VT_CHECK_EQ(BuildRegionGraph(Empty.Instance.Map(), Empty.Regions).RegionCount(), 0u);
	std::string Picture;
	ExportRegionAscii(Empty.Instance.Map(), Empty.Regions, 8, Picture);
	VT_CHECK(Picture.empty());
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}

VAELEN_TEST(Regions, FrozenDigestsAreReproducedByEveryCompilerAndPlatform)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.GenerateFrom(256, 256));
	const Hash64 R = LayerDigest(W.Instance.Map(), W.Regions.RegionIndex.Index);
	const RegionStats S = MeasureRegions(W.Instance, W.Layers, W.Regions, W.RTypes);
	VAELEN_LOG_INFO(LogRegions, "frozen: region256=%016llx regions=%u", static_cast<unsigned long long>(R), S.Regions);
	VT_CHECK_EQ(R, Hash64{VAELEN_REGION_FROZEN_256});
	VT_CHECK_EQ(S.Regions, uint32{VAELEN_REGION_COUNT_256});
}

VAELEN_TEST(Regions, DefaultSizeBaseline)
{
	GenWorld W(AelvorSeed);
	WorldGenConfig Gen;
	Gen.Width = 1024;
	Gen.Height = 1024;
	VT_REQUIRE(W.Instance.Map().Reset(Gen) && GenerateElevation(W.Instance.Map(), W.Layers, AelvorSeed) &&
			   GenerateHydrology(W.Instance, W.Layers, W.Hydro, W.Types) &&
			   GenerateClimate(W.Instance.Map(), W.Layers, AelvorSeed));
	const auto Start = std::chrono::steady_clock::now();
	VT_REQUIRE(GenerateRegions(W.Instance, W.Layers, W.Hydro, W.Regions, W.RTypes));
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const RegionStats S = MeasureRegions(W.Instance, W.Layers, W.Regions, W.RTypes);
	VT_CHECK(S.Regions > 30);
	VT_CHECK_EQ(S.UnassignedLand, 0u);
	VAELEN_LOG_INFO(LogRegions, "baseline: 1024 x 1024 regions in %.3f s (%u regions, sizes %u..%u)%s", Seconds,
					S.Regions, S.SmallestTiles, S.LargestTiles,
					VAELEN_ASSERTS_ENABLED ? " [asserts on]" : " [asserts off]");
}
