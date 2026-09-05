// VAELEN - VaelenSim tests
// WorldGen 02.05: hydrology - synthetic bowl and basin, drainage of every land
// tile on the AELVOR map, accumulation monotonicity, rivers and lakes as
// entities, determinism and snapshot round trip, frozen digests, baseline.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <chrono>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.05).
#define VAELEN_FLOW_FROZEN_256 0x7b6f14f39284c7a0ull
#define VAELEN_ACC_FROZEN_256 0x09daae8f9829deedull
#define VAELEN_RIVER_FROZEN_256 0x00f52817ba9dcda8ull
#define VAELEN_HYDRO_RIVERS_256 40u
#define VAELEN_HYDRO_LAKES_256 22u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogHydrology);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct GenWorld
	{
		explicit GenWorld(uint64 Seed) : Instance(Config(Seed))
		{
			Layers = WorldLayers::Declare(Instance.Map());
			Hydro = HydroLayers::Declare(Instance.Map());
			Types = WorldTypes::Declare(Instance);
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool Generate(uint32 Size, WorldGenConfig Gen = WorldGenConfig{})
		{
			Gen.Width = Size;
			Gen.Height = Size;
			const uint64 Seed = Instance.Config().Seed;
			return Instance.Map().Reset(Gen) && GenerateElevation(Instance.Map(), Layers, Seed) &&
				   GenerateHydrology(Instance, Layers, Hydro, Types) && GenerateClimate(Instance.Map(), Layers, Seed);
		}
		World Instance;
		WorldLayers Layers;
		HydroLayers Hydro;
		WorldTypes Types;
	};

	/// Everything that must hold for any hydrology. Returns failures.
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
		const TileLayer<int64>& E = Map.GetLayer(W.Layers.Elevation);
		const TileLayer<uint8>& T = Map.GetLayer(W.Layers.Terrain);
		const TileLayer<int64>& F = Map.GetLayer(W.Hydro.Filled);
		const TileLayer<uint8>& Flow = Map.GetLayer(W.Hydro.FlowDirection);
		const TileLayer<uint32>& Acc = Map.GetLayer(W.Hydro.Accumulation);
		const TileLayer<uint16>& RiverIx = Map.GetLayer(W.Hydro.RiverIndex);
		const TileLayer<uint16>& LakeIx = Map.GetLayer(W.Hydro.LakeIndex);
		uint32 LandTiles = 0;
		uint64 AccSum = 0;
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			const bool Land = (T[I] & TerrainFlag::Land) != 0;
			if (F[I] < E[I])
			{
				Fail("filled below elevation");
				break;
			}
			if (!Land)
			{
				if (Flow[I] != FlowNone || Acc[I] != 0 || RiverIx[I] != 0 || LakeIx[I] != 0 || F[I] != E[I])
				{
					Fail("sea tile carries hydrology");
					break;
				}
				continue;
			}
			++LandTiles;
			AccSum += Acc[I];
			if (Flow[I] == FlowNone)
			{
				Fail("land tile without a lower neighbour");
				break;
			}
			const TileCoord C = Grid.CoordOf(I);
			const uint32 J = Grid.IndexOf({C.X + NeighbourOffsets[Flow[I]].X, C.Y + NeighbourOffsets[Flow[I]].Y});
			if (F[J] >= F[I])
			{
				Fail("flow does not descend");
				break;
			}
			if ((T[J] & TerrainFlag::Land) != 0 && Acc[J] <= Acc[I])
			{
				Fail("accumulation does not grow downstream");
				break;
			}
			if (LakeIx[I] != 0 && F[I] <= E[I])
			{
				Fail("lake tile is not raised");
				break;
			}
			if (RiverIx[I] != 0 && LakeIx[I] != 0)
			{
				Fail("river tile inside a lake");
				break;
			}
			if (I % 31 == 0 && StepsToSea(Map, W.Layers, W.Hydro, I) == 0xffffffffu)
			{
				Fail("land tile does not drain to the sea");
				break;
			}
		}
		// Every land tile drains one unit into the sea exactly once: the sum of
		// accumulation over the tiles that flow into the sea equals the land count.
		uint64 IntoSea = 0;
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			if ((T[I] & TerrainFlag::Land) == 0 || Flow[I] == FlowNone)
			{
				continue;
			}
			const TileCoord C = Grid.CoordOf(I);
			const uint32 J = Grid.IndexOf({C.X + NeighbourOffsets[Flow[I]].X, C.Y + NeighbourOffsets[Flow[I]].Y});
			if ((T[J] & TerrainFlag::Land) == 0)
			{
				IntoSea += Acc[I];
			}
		}
		if (IntoSea != LandTiles)
		{
			Fail("total flow into the sea != land tiles");
		}
		// Entities agree with the layers.
		uint32 Rivers = 0;
		W.Instance.Components()
			.GetPool(W.Types.River)
			.ForEach(
				[&](EntityHandle H, const RiverInfo& R)
				{
					++Rivers;
					if (!W.Instance.Entities().GetId(H).IsKind(IdKind::River) || RiverIx[R.SourceTile] != R.Index ||
						R.Length == 0)
					{
						Fail("river entity disagrees with the layer");
					}
				});
		uint32 Lakes = 0;
		W.Instance.Components()
			.GetPool(W.Types.Lake)
			.ForEach(
				[&](EntityHandle H, const LakeInfo& L)
				{
					++Lakes;
					if (!W.Instance.Entities().GetId(H).IsKind(IdKind::Lake) || L.Tiles == 0)
					{
						Fail("lake entity disagrees with the layer");
					}
				});
		uint16 MaxRiver = 0;
		uint16 MaxLake = 0;
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			MaxRiver = RiverIx[I] > MaxRiver ? RiverIx[I] : MaxRiver;
			MaxLake = LakeIx[I] > MaxLake ? LakeIx[I] : MaxLake;
		}
		if (MaxRiver != Rivers || MaxLake != Lakes)
		{
			Fail("index layers do not match the entity counts");
		}
		(void)AccSum;
		return Failures;
	}
} // namespace

VAELEN_TEST(Hydrology, SyntheticBasinFillsIntoALakeWithAnOutlet)
{
	// 24 x 24: sea ring, a cone rising to the centre, and a pit of 3 x 3 tiles
	// carved at (6..8, 6..8) that cannot drain without filling.
	GenWorld W(1);
	WorldGenConfig Gen;
	Gen.Width = 24;
	Gen.Height = 24;
	Gen.Params[ParamIndex::MinRiverLength] = 2;
	Gen.Params[ParamIndex::LakeMinTiles] = 4;
	VT_REQUIRE(W.Instance.Map().Reset(Gen));
	WorldMap& Map = W.Instance.Map();
	TileLayer<int64>& E = Map.GetLayer(W.Layers.Elevation);
	for (uint32 Y = 0; Y < 24; ++Y)
	{
		for (uint32 X = 0; X < 24; ++X)
		{
			const int32 DX = static_cast<int32>(X) - 12;
			const int32 DY = static_cast<int32>(Y) - 12;
			const int32 D = (DX < 0 ? -DX : DX) > (DY < 0 ? -DY : DY) ? (DX < 0 ? -DX : DX) : (DY < 0 ? -DY : DY);
			int64 H = D >= 11 ? Fix64::FromInt(-100).Raw : Fix64::FromInt(1000 - D * 80).Raw;
			if (X >= 6 && X <= 8 && Y >= 6 && Y <= 8)
			{
				H = Fix64::FromInt(50).Raw; // a pit far below its rim
			}
			E[Y * 24 + X] = H;
		}
	}
	ClassifyTerrain(Map, W.Layers);
	VT_REQUIRE(GenerateHydrology(W.Instance, W.Layers, W.Hydro, W.Types));
	VT_REQUIRE(GenerateClimate(Map, W.Layers, 1));
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const HydrologyStats S = MeasureHydrology(W.Instance, W.Layers, W.Hydro, W.Types);
	VT_CHECK_EQ(S.Lakes, 1u);
	VT_CHECK_EQ(S.LakeTiles, 9u);
	VT_CHECK_EQ(S.LargestLake, 9u);
	const TileLayer<int64>& F = Map.GetLayer(W.Hydro.Filled);
	const TileLayer<uint16>& LakeIx = Map.GetLayer(W.Hydro.LakeIndex);
	VT_CHECK_EQ(LakeIx[7 * 24 + 7], 1u);
	VT_CHECK(F[7 * 24 + 7] > E[7 * 24 + 7]);
	VT_CHECK(F[7 * 24 + 7] < Fix64::FromInt(1000).Raw);
	// The lake spills and every pit tile reaches the sea.
	LakeInfo Info;
	W.Instance.Components().GetPool(W.Types.Lake).ForEach([&](EntityHandle, const LakeInfo& L) { Info = L; });
	VT_CHECK_EQ(Info.Tiles, 9u);
	VT_CHECK(Info.OutletTile != 0);
	VT_CHECK_EQ(LakeIx[Info.OutletTile], 0u);
	VT_CHECK(StepsToSea(Map, W.Layers, W.Hydro, 7 * 24 + 7) < 24u);
	// The cone drains outward: the centre has accumulation 1, coast tiles more.
	const TileLayer<uint32>& Acc = Map.GetLayer(W.Hydro.Accumulation);
	VT_CHECK_EQ(Acc[12 * 24 + 12], 1u);
	VT_CHECK(S.MaxAccumulation > 9u);
	// A rerun replaces the entities instead of duplicating them.
	VT_REQUIRE(GenerateHydrology(W.Instance, W.Layers, W.Hydro, W.Types));
	VT_CHECK_EQ(MeasureHydrology(W.Instance, W.Layers, W.Hydro, W.Types).Lakes, 1u);
	VT_CHECK_EQ(W.Instance.Entities().GetAliveCount(), S.Rivers + 1u);
}

VAELEN_TEST(Hydrology, AelvorDrainsEverywhereAndHasRiversAndLakes)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const HydrologyStats S = MeasureHydrology(W.Instance, W.Layers, W.Hydro, W.Types);
	VAELEN_LOG_INFO(LogHydrology,
					"256: %u rivers (%u tiles, longest %u), %u lakes (%u tiles, largest %u), max accumulation %u, %u "
					"raised tiles",
					S.Rivers, S.RiverTiles, S.LongestRiver, S.Lakes, S.LakeTiles, S.LargestLake, S.MaxAccumulation,
					S.RaisedTiles);
	VT_CHECK(S.Rivers >= 5 && S.Rivers <= 150);
	VT_CHECK(S.LongestRiver >= 20);
	VT_CHECK(S.MaxAccumulation > 1000);
	VT_CHECK_EQ(S.LakeTiles, S.RaisedTiles); // after the sediment fill, only lakes stay raised
	VT_CHECK(S.Lakes >= 1 && S.Lakes <= 40);
	VT_CHECK(S.LakeTiles * 25 < 256u * 256u); // lakes cover under 4 % of the map
	// Every land tile, not only the sample, drains to the sea.
	const WorldMap& Map = W.Instance.Map();
	const TileLayer<uint8>& T = Map.GetLayer(W.Layers.Terrain);
	uint32 Stuck = 0;
	uint32 MaxSteps = 0;
	for (uint32 I = 0; I < Map.Grid().TileCount(); ++I)
	{
		if ((T[I] & TerrainFlag::Land) == 0)
		{
			continue;
		}
		const uint32 Steps = StepsToSea(Map, W.Layers, W.Hydro, I);
		Stuck += Steps == 0xffffffffu ? 1u : 0u;
		MaxSteps = Steps != 0xffffffffu && Steps > MaxSteps ? Steps : MaxSteps;
	}
	VT_CHECK_EQ(Stuck, 0u);
	VT_CHECK(MaxSteps > 50 && MaxSteps < 2000);
	std::string Picture;
	ExportHydroAscii(Map, W.Layers, W.Hydro, 64, Picture);
	VT_CHECK_EQ(Picture.size(), 64u * 32u + 32u);
	for (usize Row = 0; Row < 32; Row += 8)
	{
		const std::string Slice = Picture.substr(Row * 65, 8 * 65);
		VAELEN_LOG_INFO(LogHydrology, "AELVOR rivers at 256, rows %zu-%zu:\n%s", Row, Row + 7, Slice.c_str());
	}
}

VAELEN_TEST(Hydrology, DeterministicAcrossWorldsAndThroughSnapshots)
{
	GenWorld A(9);
	GenWorld B(9);
	GenWorld C(10);
	VT_REQUIRE(A.Generate(64) && B.Generate(64) && C.Generate(64));
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance)); // layers and entities
	VT_CHECK_NE(ComputeStateDigest(A.Instance), ComputeStateDigest(C.Instance));
	VT_CHECK_EQ(CheckInvariants(Ctx, C), 0u);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	GenWorld R(9);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(R.Instance.Map().LayerCount(), 12u);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(CheckInvariants(Ctx, R), 0u);
	const HydrologyStats SA = MeasureHydrology(A.Instance, A.Layers, A.Hydro, A.Types);
	const HydrologyStats SR = MeasureHydrology(R.Instance, R.Layers, R.Hydro, R.Types);
	VT_CHECK_EQ(SA.Rivers, SR.Rivers);
	VT_CHECK_EQ(SA.Lakes, SR.Lakes);
	// A lower threshold gives more river tiles.
	WorldGenConfig Dense;
	Dense.Params[ParamIndex::RiverThreshold] = Fix64::FromRatio(1, 2000).Raw;
	GenWorld D(9);
	VT_REQUIRE(D.Generate(64, Dense));
	VT_CHECK(MeasureHydrology(D.Instance, D.Layers, D.Hydro, D.Types).RiverTiles > SA.RiverTiles);
	// Misuse: hydrology before any grid.
	VaelenTest::ScopedAssertCapture Capture;
	GenWorld Empty(1);
	VT_CHECK(!GenerateHydrology(Empty.Instance, Empty.Layers, Empty.Hydro, Empty.Types));
	VT_CHECK_EQ(MeasureHydrology(Empty.Instance, Empty.Layers, Empty.Hydro, Empty.Types).Rivers, 0u);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}

VAELEN_TEST(Hydrology, FrozenDigestsAreReproducedByEveryCompilerAndPlatform)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	const Hash64 Flow = LayerDigest(W.Instance.Map(), W.Hydro.FlowDirection.Index);
	const Hash64 Acc = LayerDigest(W.Instance.Map(), W.Hydro.Accumulation.Index);
	const Hash64 River = LayerDigest(W.Instance.Map(), W.Hydro.RiverIndex.Index);
	const HydrologyStats S = MeasureHydrology(W.Instance, W.Layers, W.Hydro, W.Types);
	VAELEN_LOG_INFO(LogHydrology, "frozen: flow256=%016llx acc256=%016llx river256=%016llx rivers=%u lakes=%u",
					static_cast<unsigned long long>(Flow), static_cast<unsigned long long>(Acc),
					static_cast<unsigned long long>(River), S.Rivers, S.Lakes);
	VT_CHECK_EQ(Flow, Hash64{VAELEN_FLOW_FROZEN_256});
	VT_CHECK_EQ(Acc, Hash64{VAELEN_ACC_FROZEN_256});
	VT_CHECK_EQ(River, Hash64{VAELEN_RIVER_FROZEN_256});
	VT_CHECK_EQ(S.Rivers, uint32{VAELEN_HYDRO_RIVERS_256});
	VT_CHECK_EQ(S.Lakes, uint32{VAELEN_HYDRO_LAKES_256});
}

VAELEN_TEST(Hydrology, DefaultSizeBaseline)
{
	GenWorld W(AelvorSeed);
	WorldGenConfig Gen;
	Gen.Width = 1024;
	Gen.Height = 1024;
	VT_REQUIRE(W.Instance.Map().Reset(Gen) && GenerateElevation(W.Instance.Map(), W.Layers, AelvorSeed));
	const auto Start = std::chrono::steady_clock::now();
	VT_REQUIRE(GenerateHydrology(W.Instance, W.Layers, W.Hydro, W.Types));
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const HydrologyStats S = MeasureHydrology(W.Instance, W.Layers, W.Hydro, W.Types);
	VT_CHECK(S.Rivers > 20);
	VAELEN_LOG_INFO(LogHydrology, "baseline: 1024 x 1024 hydrology in %.3f s (%u rivers, %u lakes)%s", Seconds,
					S.Rivers, S.Lakes, VAELEN_ASSERTS_ENABLED ? " [asserts on]" : " [asserts off]");
}
