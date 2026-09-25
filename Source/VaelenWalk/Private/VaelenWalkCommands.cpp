// VAELEN - VaelenWalk
// Phase 19 task 19.06: two console commands - the walk begun, and the ground probed.
//
// Vaelen.Walk <size> <years>: begins AELVOR with the daily cadence (a walk is
// what that cadence is for, 15.10), spawns the land and the sky, builds the
// ground around the played region, aims the sun at the life's hour, and puts
// the walker on the played region's centroid tile. Prints the terrain line
// of that region - the bytes `VaelenAtlas --size S --years Y --scene-terrain R`
// prints - the climate line (the bytes the Atlas prints), and the sky line.
//
// Vaelen.Probe N [biasMm]: N line traces against the builder's HeightAt, the
// widest gap and the misses. With a bias every trace is offset, and the gap
// must show it: a probe that cannot fail is not a probe (ADR-0149).
//
// Nothing here turns the day or means a verb: those are the controller's and
// the subsystem's, and the fence refuses the words (check_ui_fence.py).
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Vaelen/Scene/Sky.h"
#include "Vaelen/Scene/Terrain.h"
#include "VaelenLand.h"
#include "VaelenSky.h"
#include "VaelenWalker.h"
#include "VaelenWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaelenWalk, Log, All);

namespace
{
	UVaelenWorldSubsystem* Held(UWorld* From)
	{
		if (From == nullptr || From->GetGameInstance() == nullptr)
		{
			return nullptr;
		}
		return From->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
	}

	int32 NumberAt(const TArray<FString>& Args, int32 Which, int32 OrElse)
	{
		return Args.Num() > Which ? FCString::Atoi(*Args[Which]) : OrElse;
	}

	/// The row of the played region's centroid, where the sun is measured,
	/// and whether the equator lies at +Y of it.
	uint32 CentroidRow(const UVaelenWorldSubsystem& World, bool& bEquatorIsPlusY)
	{
		const Vaelen::View::LifeView& Life = World.Life();
		const Vaelen::Scene::Ground& G = World.Scene();
		uint32 Row = G.Height / 2u;
		for (const Vaelen::View::RegionView& R : World.World().Regions)
		{
			if (R.Index == (Life.Region != 0u ? Life.Region : 1u) && G.Width != 0u)
			{
				Row = R.CentroidTile / G.Width;
				break;
			}
		}
		bEquatorIsPlusY = 2u * Row < G.Height;
		return Row;
	}

	/// The land and the sky of this level, spawned by Vaelen.Walk; null before it.
	AVaelenLand* LandOf(UWorld* World_)
	{
		for (TActorIterator<AVaelenLand> It(World_); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	void Walk(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr)
		{
			return;
		}
		const int32 Size = NumberAt(Args, 0, 128);
		const int32 Years = NumberAt(Args, 1, 120);
		if (!World->Begin(Size, Years, true))
		{
			UE_LOG(LogVaelenWalk, Warning,
				   TEXT("LogVaelenWalk: no world begun (one per host, and the map must generate)"));
			return;
		}
		const Vaelen::View::LifeView& Life = World->Life();
		const Vaelen::Scene::Ground& G = World->Scene();
		AVaelenLand* Land = World_->SpawnActor<AVaelenLand>();
		AVaelenSky* Sky = World_->SpawnActor<AVaelenSky>();
		if (Land == nullptr || Sky == nullptr || !Land->Build(G, World->Climate(), Life.Region))
		{
			UE_LOG(LogVaelenWalk, Warning, TEXT("LogVaelenWalk: the ground could not be built"));
			return;
		}
		// The land repaints and re-aims the sky after every retaking of the
		// views; AddUObject, so the binding dies with the actor.
		World->OnViewsTaken.AddUObject(Land, &AVaelenLand::OnViewsTaken);
		Land->OnViewsTaken();

		// The walker on the centroid tile of the played region, standing.
		if (APlayerController* Controller = World_->GetFirstPlayerController())
		{
			if (AVaelenWalker* Walker = Cast<AVaelenWalker>(Controller->GetPawn()))
			{
				uint32 Tile = 0;
				for (const Vaelen::View::RegionView& R : World->World().Regions)
				{
					if (R.Index == Life.Region)
					{
						Tile = R.CentroidTile;
						break;
					}
				}
				Vaelen::int64 X = 0, Y = 0;
				Vaelen::Scene::PointOfTile(G, Tile, X, Y);
				const double Z = static_cast<double>(Vaelen::Scene::HeightAt(G, X, Y)) +
								 static_cast<double>(Walker->StandingHalfHeight()) + 10.0;
				Walker->SetActorLocation(FVector(static_cast<double>(X), static_cast<double>(Y), Z));
				UE_LOG(LogVaelenWalk, Log, TEXT("LogVaelenWalk: walker on tile %u of region %u at (%lld, %lld, %.0f)"),
					   static_cast<unsigned>(Tile), static_cast<unsigned>(Life.Region), static_cast<long long>(X),
					   static_cast<long long>(Y), Z);
			}
		}

		// The three lines a sitting brings back, in the bytes the Atlas prints.
		char Line[Vaelen::Scene::TerrainLineBytes];
		if (Land->TerrainLine(static_cast<uint32>(Size), World->Seed(), Line, Vaelen::Scene::TerrainLineBytes) != 0u)
		{
			UE_LOG(LogVaelenWalk, Log, TEXT("%s"), ANSI_TO_TCHAR(Line));
		}
		UE_LOG(LogVaelenWalk, Log, TEXT("%s"), *World->ClimateLine());
		bool bEquatorIsPlusY = true;
		const uint32 Row = CentroidRow(*World, bEquatorIsPlusY);
		const Vaelen::Scene::SkyStats Sky_ = Vaelen::Scene::MeasureSky(G, World->Climate(), Life, Row);
		char SkyText[Vaelen::Scene::SkyLineBytes];
		if (Vaelen::Scene::SkyLine(static_cast<uint32>(Size), World->Seed(), World->Climate().Day + 1u, Sky_, SkyText,
								   Vaelen::Scene::SkyLineBytes) != 0u)
		{
			UE_LOG(LogVaelenWalk, Log, TEXT("%s"), ANSI_TO_TCHAR(SkyText));
		}
	}

	void Probe(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		AVaelenLand* Land = LandOf(World_);
		if (World == nullptr || Land == nullptr)
		{
			UE_LOG(LogVaelenWalk, Warning, TEXT("LogVaelenWalk: no land to probe (Vaelen.Walk first)"));
			return;
		}
		const int32 N = NumberAt(Args, 0, 64);
		const int32 BiasMm = NumberAt(Args, 1, 0);
		double MaxCm = 0.0;
		int32 Misses = 0;
		const int32 Made = Land->Probe(World->Scene(), N, static_cast<double>(BiasMm) / 10.0, MaxCm, Misses);
		UE_LOG(LogVaelenWalk, Log,
			   TEXT("LogVaelenWalk: probe %d of region %u, bias %d mm: max |trace-builder| %.1f cm, misses %d"), Made,
			   static_cast<unsigned>(Land->BuiltFor()), BiasMm, MaxCm, Misses);
	}

	FAutoConsoleCommandWithWorldAndArgs
		GWalk(TEXT("Vaelen.Walk"),
			  TEXT("Begin AELVOR with the daily cadence, build the ground around the "
				   "played region and stand the walker on it. Vaelen.Walk [size] [years]"),
			  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Walk));

	FAutoConsoleCommandWithWorldAndArgs
		GProbe(TEXT("Vaelen.Probe"), TEXT("Line traces against the builder's heights. Vaelen.Probe [n] [biasMm]"),
			   FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Probe));
} // namespace
