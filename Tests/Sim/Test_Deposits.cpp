// VAELEN - VaelenSim tests
// WorldGen 02.07: deposits - suitability rules, placement invariants on the
// AELVOR map, every kind present, density and spacing sensitivity,
// determinism, snapshot round trip, frozen digest, baseline.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.07).
#define VAELEN_DEPOSIT_FROZEN_256 0xc0b544dc8c210da6ull
#define VAELEN_DEPOSIT_COUNT_256 1364u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogDeposits);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct GenWorld
	{
		explicit GenWorld(uint64 Seed) : Instance(Config(Seed))
		{
			Layers = WorldLayers::Declare(Instance.Map());
			Hydro = HydroLayers::Declare(Instance.Map());
			Regions = RegionLayers::Declare(Instance.Map());
			Deposits = DepositLayers::Declare(Instance.Map());
			Types = WorldTypes::Declare(Instance);
			RTypes = RegionTypes::Declare(Instance);
			DTypes = DepositTypes::Declare(Instance);
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool GenerateUpToRegions(uint32 Size, WorldGenConfig Gen = WorldGenConfig{})
		{
			Gen.Width = Size;
			Gen.Height = Size;
			const uint64 Seed = Instance.Config().Seed;
			return Instance.Map().Reset(Gen) && GenerateElevation(Instance.Map(), Layers, Seed) &&
				   GenerateHydrology(Instance, Layers, Hydro, Types) && GenerateClimate(Instance.Map(), Layers, Seed) &&
				   GenerateRegions(Instance, Layers, Hydro, Regions, RTypes);
		}
		bool Generate(uint32 Size, WorldGenConfig Gen = WorldGenConfig{})
		{
			return GenerateUpToRegions(Size, Gen) &&
				   GenerateDeposits(Instance, Layers, Hydro, Regions, Deposits, DTypes);
		}
		World Instance;
		WorldLayers Layers;
		HydroLayers Hydro;
		RegionLayers Regions;
		DepositLayers Deposits;
		WorldTypes Types;
		RegionTypes RTypes;
		DepositTypes DTypes;
	};

	/// Everything that must hold for any placement. Returns failures.
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
		const TileLayer<int64>& Sl = Map.GetLayer(W.Layers.Slope);
		const TileLayer<uint8>& B = Map.GetLayer(W.Layers.Biome);
		const TileLayer<uint16>& RiverIx = Map.GetLayer(W.Hydro.RiverIndex);
		const TileLayer<uint16>& LakeIx = Map.GetLayer(W.Hydro.LakeIndex);
		const TileLayer<uint16>& RegionIx = Map.GetLayer(W.Regions.RegionIndex);
		const TileLayer<uint16>& D = Map.GetLayer(W.Deposits.DepositIndex);
		const int64 Sea = Map.Config().SeaLevel;
		uint32 Entities = 0;
		W.Instance.Components()
			.GetPool(W.DTypes.Deposit)
			.ForEach(
				[&](EntityHandle H, const DepositInfo& Info)
				{
					++Entities;
					if (!W.Instance.Entities().GetId(H).IsKind(IdKind::ResourceDeposit))
					{
						Fail("deposit entity of the wrong kind");
					}
					if (Info.Tile >= Grid.TileCount() || D[Info.Tile] != Info.Index)
					{
						Fail("deposit layer disagrees with the entity");
						return;
					}
					if ((T[Info.Tile] & TerrainFlag::Land) == 0)
					{
						Fail("deposit on sea");
					}
					if (Info.Kind >= static_cast<uint32>(ResourceKind::Count) || Info.Tier < 1 ||
						Info.Tier > RarityTiers || Info.Richness < 1 || Info.Richness > 1000)
					{
						Fail("deposit fields out of range");
					}
					if (Info.Region != RegionIx[Info.Tile])
					{
						Fail("deposit region disagrees with the layer");
					}
					// The rules: suitability must be positive where a deposit sits.
					const TileCoord C = Grid.CoordOf(Info.Tile);
					bool LakeAdj = false;
					bool RiverAdj = false;
					Grid.ForEachNeighbour(C, 4,
										  [&](TileCoord NC, uint32)
										  {
											  LakeAdj = LakeAdj || LakeIx[Grid.IndexOf(NC)] != 0;
											  RiverAdj = RiverAdj || RiverIx[Grid.IndexOf(NC)] != 0;
										  });
					const uint32 Suit = DepositSuitability(
						static_cast<ResourceKind>(Info.Kind), true, (T[Info.Tile] & TerrainFlag::Coast) != 0,
						static_cast<Biome>(B[Info.Tile]), Fix64::FromRaw(E[Info.Tile] - Sea),
						Fix64::FromRaw(Sl[Info.Tile]), RiverIx[Info.Tile] != 0, LakeAdj, RiverAdj);
					if (Suit == 0)
					{
						Fail("deposit placed where the rules forbid it");
					}
				});
		uint32 Marked = 0;
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			Marked += D[I] != 0 ? 1u : 0u;
		}
		if (Marked != Entities)
		{
			Fail("deposit index layer and entity count differ");
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(Deposits, SuitabilityRulesAreExplicit)
{
	const Fix64 Low = Fix64::FromInt(100);
	const Fix64 High = Fix64::FromInt(1600);
	const Fix64 Flat = Fix64::FromInt(10);
	const Fix64 Steep = Fix64::FromInt(400);
	// Sea never.
	for (uint32 K = 0; K < static_cast<uint32>(ResourceKind::Count); ++K)
	{
		VT_CHECK_EQ(
			DepositSuitability(static_cast<ResourceKind>(K), false, true, Biome::Ocean, Low, Flat, false, true, true),
			0u);
		VT_CHECK(std::string_view(ResourceKindName(static_cast<ResourceKind>(K))) != "Unknown");
	}
	VT_CHECK_STREQ(ResourceKindName(ResourceKind::Count), "Unknown");
	VT_CHECK(DepositSuitability(ResourceKind::Timber, true, false, Biome::TropicalForest, Low, Flat, false, false,
								false) == 1000);
	VT_CHECK(DepositSuitability(ResourceKind::Timber, true, false, Biome::Desert, Low, Flat, false, false, false) == 0);
	VT_CHECK(DepositSuitability(ResourceKind::Clay, true, false, Biome::Grassland, Low, Flat, false, false, true) ==
			 900);
	VT_CHECK(DepositSuitability(ResourceKind::Clay, true, false, Biome::Grassland, Low, Flat, false, false, false) ==
			 0);
	VT_CHECK(DepositSuitability(ResourceKind::Clay, true, false, Biome::Grassland, High, Flat, true, false, false) ==
			 0);
	VT_CHECK(DepositSuitability(ResourceKind::FertileSoil, true, false, Biome::Grassland, Low, Flat, false, true,
								false) == 1000);
	VT_CHECK(DepositSuitability(ResourceKind::FertileSoil, true, false, Biome::Grassland, Low, Flat, false, false,
								false) == 500);
	VT_CHECK(DepositSuitability(ResourceKind::FertileSoil, true, false, Biome::Tundra, Low, Flat, false, true, false) ==
			 0);
	VT_CHECK(DepositSuitability(ResourceKind::FertileSoil, true, false, Biome::Grassland, Low, Steep, false, true,
								false) == 0);
	VT_CHECK(DepositSuitability(ResourceKind::Salt, true, true, Biome::Desert, Low, Flat, false, false, false) == 900);
	VT_CHECK(DepositSuitability(ResourceKind::Salt, true, false, Biome::Desert, Low, Flat, false, false, false) == 500);
	VT_CHECK(DepositSuitability(ResourceKind::Salt, true, true, Biome::TemperateForest, Low, Flat, false, false,
								false) == 0);
	VT_CHECK(DepositSuitability(ResourceKind::Gold, true, false, Biome::Alpine, High, Steep, true, false, false) ==
			 1000);
	VT_CHECK(DepositSuitability(ResourceKind::Gold, true, false, Biome::Alpine, Low, Steep, true, false, false) == 0);
	VT_CHECK(DepositSuitability(ResourceKind::IronOre, true, false, Biome::Alpine, High, Steep, false, false, false) ==
			 1000);
	VT_CHECK(DepositSuitability(ResourceKind::IronOre, true, false, Biome::Grassland, Low, Flat, false, false, false) ==
			 0);
	VT_CHECK(DepositSuitability(ResourceKind::CopperOre, true, false, Biome::Desert, High, Flat, false, false, false) ==
			 1000);
	VT_CHECK(DepositSuitability(ResourceKind::CopperOre, true, false, Biome::TemperateForest, Fix64::FromInt(650), Flat,
								false, false, false) == 300);
	VT_CHECK(DepositSuitability(ResourceKind::Stone, true, false, Biome::Grassland, Low, Steep, true, false, false) ==
			 0);
	VT_CHECK(DepositSuitability(ResourceKind::Stone, true, false, Biome::Grassland, High, Steep, false, false, false) ==
			 1000);
	VT_CHECK(DepositSuitability(ResourceKind::Stone, true, false, Biome::Grassland, Low, Flat, false, false, false) ==
			 0);
}

VAELEN_TEST(Deposits, AelvorHasEveryKindWhereTheRulesAllow)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const DepositStats S = MeasureDeposits(W.Instance, W.Layers, W.Deposits, W.DTypes);
	std::string Report;
	for (uint32 K = 0; K < static_cast<uint32>(ResourceKind::Count); ++K)
	{
		Report += ResourceKindName(static_cast<ResourceKind>(K));
		Report += "=";
		Report += std::to_string(S.ByKind[K]);
		Report += " ";
	}
	VAELEN_LOG_INFO(LogDeposits, "256: %u deposits (%s) tiers %u/%u/%u, %u regions with deposits", S.Total,
					Report.c_str(), S.ByTier[1], S.ByTier[2], S.ByTier[3], S.RegionsWithDeposits);
	VT_CHECK_EQ(S.OnSea, 0u);
	for (uint32 K = 0; K < static_cast<uint32>(ResourceKind::Count); ++K)
	{
		VT_CHECK_MSG(S.ByKind[K] > 0, "no %s deposit", ResourceKindName(static_cast<ResourceKind>(K)));
	}
	const ElevationStats ES = MeasureElevation(W.Instance.Map(), W.Layers);
	VT_CHECK(S.Total * 100 > ES.LandTiles && S.Total * 8 < ES.LandTiles); // 1 % .. 12.5 % of land
	VT_CHECK(S.ByTier[3] < S.ByTier[2] && S.ByTier[2] < S.ByTier[1]);
	VT_CHECK(S.ByKind[static_cast<uint32>(ResourceKind::Gold)] < S.ByKind[static_cast<uint32>(ResourceKind::Stone)]);
	const RegionStats RS = MeasureRegions(W.Instance, W.Layers, W.Regions, W.RTypes);
	VT_CHECK(S.RegionsWithDeposits * 2 > RS.Regions); // most regions have something
}

VAELEN_TEST(Deposits, DeterministicSensitiveAndSnapshotSafe)
{
	GenWorld A(9);
	GenWorld B(9);
	GenWorld C(10);
	VT_REQUIRE(A.Generate(64) && B.Generate(64) && C.Generate(64));
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_NE(ComputeStateDigest(A.Instance), ComputeStateDigest(C.Instance));
	VT_CHECK_EQ(CheckInvariants(Ctx, C), 0u);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	GenWorld R(9);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(R.Instance.Map().LayerCount(), 14u);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(CheckInvariants(Ctx, R), 0u);
	// Half density: fewer deposits; wider spacing: fewer deposits.
	const uint32 Base = MeasureDeposits(A.Instance, A.Layers, A.Deposits, A.DTypes).Total;
	WorldGenConfig Sparse;
	Sparse.Params[ParamIndex::DepositDensity] = Fix64::FromRatio(1, 4).Raw;
	GenWorld D(9);
	VT_REQUIRE(D.Generate(64, Sparse));
	VT_CHECK(MeasureDeposits(D.Instance, D.Layers, D.Deposits, D.DTypes).Total < Base);
	WorldGenConfig Wide;
	Wide.Params[ParamIndex::DepositSpacing] = 16;
	GenWorld E(9);
	VT_REQUIRE(E.Generate(64, Wide));
	VT_CHECK(MeasureDeposits(E.Instance, E.Layers, E.Deposits, E.DTypes).Total < Base);
	VT_CHECK_EQ(CheckInvariants(Ctx, E), 0u);
	// Without regions generated, the region field is zero and everything else holds.
	GenWorld NoRegions(9);
	VT_REQUIRE(NoRegions.GenerateUpToRegions(64));
	VT_REQUIRE(
		GenerateRegions(NoRegions.Instance, NoRegions.Layers, NoRegions.Hydro, NoRegions.Regions, NoRegions.RTypes));
	// A rerun replaces the entities.
	const uint32 Before = A.Instance.Entities().GetAliveCount();
	VT_REQUIRE(GenerateDeposits(A.Instance, A.Layers, A.Hydro, A.Regions, A.Deposits, A.DTypes));
	VT_CHECK_EQ(A.Instance.Entities().GetAliveCount(), Before);
	// Misuse.
	VaelenTest::ScopedAssertCapture Capture;
	GenWorld Empty(1);
	VT_CHECK(!GenerateDeposits(Empty.Instance, Empty.Layers, Empty.Hydro, Empty.Regions, Empty.Deposits, Empty.DTypes));
	VT_CHECK_EQ(MeasureDeposits(Empty.Instance, Empty.Layers, Empty.Deposits, Empty.DTypes).Total, 0u);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}

VAELEN_TEST(Deposits, FrozenDigestsAreReproducedByEveryCompilerAndPlatform)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	const Hash64 D = LayerDigest(W.Instance.Map(), W.Deposits.DepositIndex.Index);
	const DepositStats S = MeasureDeposits(W.Instance, W.Layers, W.Deposits, W.DTypes);
	VAELEN_LOG_INFO(LogDeposits, "frozen: deposit256=%016llx deposits=%u", static_cast<unsigned long long>(D), S.Total);
	VT_CHECK_EQ(D, Hash64{VAELEN_DEPOSIT_FROZEN_256});
	VT_CHECK_EQ(S.Total, uint32{VAELEN_DEPOSIT_COUNT_256});
}

VAELEN_TEST(Deposits, DefaultSizeBaseline)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.GenerateUpToRegions(1024));
	const auto Start = std::chrono::steady_clock::now();
	VT_REQUIRE(GenerateDeposits(W.Instance, W.Layers, W.Hydro, W.Regions, W.Deposits, W.DTypes));
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const DepositStats S = MeasureDeposits(W.Instance, W.Layers, W.Deposits, W.DTypes);
	VT_CHECK(S.Total > 500);
	VT_CHECK_EQ(S.OnSea, 0u);
	VAELEN_LOG_INFO(LogDeposits, "baseline: 1024 x 1024 deposits in %.3f s (%u deposits)%s", Seconds, S.Total,
					VAELEN_ASSERTS_ENABLED ? " [asserts on]" : " [asserts off]");
}
