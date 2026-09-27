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
// 2026-09-25 (reviewed and corrected 2026-09-26, 19.11b), not yet built by
// UnrealBuildTool nor run: sitting S2 builds it.
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Vaelen/Scene/Fence.h"
#include "Vaelen/Scene/Layout.h"
#include "Vaelen/Scene/Sky.h"
#include "Vaelen/Scene/Terrain.h"
#include "VaelenLand.h"
#include "VaelenScenery.h"
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
		AVaelenScenery* Scenery = World_->SpawnActor<AVaelenScenery>();
		if (Land == nullptr || Sky == nullptr || Scenery == nullptr || !Land->Build(G, World->Climate(), Life.Region))
		{
			UE_LOG(LogVaelenWalk, Warning, TEXT("LogVaelenWalk: the ground could not be built"));
			return;
		}
		// The land repaints and re-aims the sky, the scenery redraws the
		// layout, after every retaking of the views; AddUObject, so the
		// bindings die with the actors. The fence walls and the water once.
		const int32 Walled = Land->BuildFenceWalls(G, Life.Region);
		Scenery->DrawWater(G);
		if (!Scenery->Draw(G, World->Layout()))
		{
			UE_LOG(LogVaelenWalk, Warning,
				   TEXT("LogVaelenWalk: a basic shape was not found; the scenery is not drawn"));
		}
		World->OnViewsTaken.AddUObject(Land, &AVaelenLand::OnViewsTaken);
		World->OnViewsTaken.AddUObject(Scenery, &AVaelenScenery::OnViewsTaken);
		Land->OnViewsTaken();
		UE_LOG(LogVaelenWalk, Log, TEXT("LogVaelenWalk: fence of region %u walled along %d edges"),
			   static_cast<unsigned>(Life.Region), Walled);

		// The walker on the played region's centroid tile - or the nearest
		// walkable tile of the region when the centroid is a lake (19.11b:
		// PlaceAfterDay's rule, the one Run.Walk proves) - standing.
		if (APlayerController* Controller = World_->GetFirstPlayerController())
		{
			// The pawn spawned at the origin over no floor, and fell: past the
			// world's KillZ (minutes) the engine destroyed it. Spawn it again
			// where the game's start would (19.11b).
			if (Controller->GetPawn() == nullptr)
			{
				if (AGameModeBase* Mode = World_->GetAuthGameMode())
				{
					Mode->RestartPlayer(Controller);
				}
			}
			AVaelenWalker* Walker = Cast<AVaelenWalker>(Controller->GetPawn());
			if (Walker == nullptr)
			{
				UE_LOG(LogVaelenWalk, Warning,
					   TEXT("LogVaelenWalk: no walker to place (is the game mode VaelenWalkGameMode?)"));
			}
			else
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
				Vaelen::Scene::PlaceAfterDay(G, Life.Region, X, Y);
				Vaelen::Scene::TileOfPoint(G, X, Y, Tile);
				const double Z = static_cast<double>(Vaelen::Scene::HeightAt(G, X, Y)) +
								 static_cast<double>(Walker->StandingHalfHeight()) + 10.0;
				Walker->SetActorLocation(FVector(static_cast<double>(X), static_cast<double>(Y), Z));
				// Whatever speed the fall gave it stays in the void: the body
				// stands on the ground it was put on.
				if (UCharacterMovementComponent* Movement = Walker->GetCharacterMovement())
				{
					Movement->StopMovementImmediately();
				}
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

	AVaelenScenery* SceneryOf(UWorld* World_)
	{
		for (TActorIterator<AVaelenScenery> It(World_); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/// The three LogVaelenScene lines of the world as it stands - the bytes
	/// `VaelenAtlas --replay <stream> --scene` prints for the same world on
	/// the same day - and what the scenery has drawn of the layout.
	void Scene(const TArray<FString>& Args, UWorld* World_)
	{
		(void)Args;
		UVaelenWorldSubsystem* World = Held(World_);
		AVaelenLand* Land = LandOf(World_);
		AVaelenScenery* Scenery = SceneryOf(World_);
		if (World == nullptr || !World->Begun() || Land == nullptr)
		{
			UE_LOG(LogVaelenWalk, Warning, TEXT("LogVaelenWalk: no scene (Vaelen.Walk first)"));
			return;
		}
		const uint32 Size = static_cast<uint32>(World->Size());
		{
			// The played region's line (the near chunks, as Vaelen.Walk prints
			// it) AND the whole ground's: `--replay --scene` prints the latter,
			// region "all", and the two could never be the same line (19.11b).
			char Line[Vaelen::Scene::TerrainLineBytes];
			if (Land->TerrainLine(Size, World->Seed(), Line, Vaelen::Scene::TerrainLineBytes) != 0u)
			{
				UE_LOG(LogVaelenWalk, Log, TEXT("%s"), ANSI_TO_TCHAR(Line));
			}
			Vaelen::Scene::TerrainStats All;
			Vaelen::Scene::MeasureChunks(World->Scene(), 0u, All);
			if (Vaelen::Scene::TerrainLine(Size, World->Seed(), 0u, All, Line, Vaelen::Scene::TerrainLineBytes) != 0u)
			{
				UE_LOG(LogVaelenWalk, Log, TEXT("%s"), ANSI_TO_TCHAR(Line));
			}
		}
		const Vaelen::View::LifeView& Life = World->Life();
		const Vaelen::Scene::LayoutStats Laid = Vaelen::Scene::MeasureLayout(World->Layout());
		{
			char Line[Vaelen::Scene::LayoutLineBytes];
			if (Vaelen::Scene::LayoutLine(Size, World->Seed(), Life.Day + 1u, Laid, Line,
										  Vaelen::Scene::LayoutLineBytes) != 0u)
			{
				UE_LOG(LogVaelenWalk, Log, TEXT("%s"), ANSI_TO_TCHAR(Line));
			}
		}
		{
			bool bEquatorIsPlusY = true;
			const uint32 Row = CentroidRow(*World, bEquatorIsPlusY);
			const Vaelen::Scene::SkyStats Sky = Vaelen::Scene::MeasureSky(World->Scene(), World->Climate(), Life, Row);
			char Line[Vaelen::Scene::SkyLineBytes];
			if (Vaelen::Scene::SkyLine(Size, World->Seed(), World->Climate().Day + 1u, Sky, Line,
									   Vaelen::Scene::SkyLineBytes) != 0u)
			{
				UE_LOG(LogVaelenWalk, Log, TEXT("%s"), ANSI_TO_TCHAR(Line));
			}
		}
		if (Scenery != nullptr)
		{
			const AVaelenScenery::FDrawn D = Scenery->Drawn();
			UE_LOG(LogVaelenWalk, Log,
				   TEXT("LogVaelenWalk: drawn houses %d figures %d (company %d) squares %d road tiles %d pits %d"),
				   D.Houses, D.Figures, D.Company, D.Squares, D.RoadTiles, D.Pits);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GScene(TEXT("Vaelen.Scene"),
											   TEXT("The three LogVaelenScene lines of the world as it stands, and "
													"what is drawn of the layout"),
											   FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Scene));

	FAutoConsoleCommandWithWorldAndArgs
		GWalk(TEXT("Vaelen.Walk"),
			  TEXT("Begin AELVOR with the daily cadence, build the ground around the "
				   "played region and stand the walker on it. Vaelen.Walk [size] [years]"),
			  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Walk));

	FAutoConsoleCommandWithWorldAndArgs
		GProbe(TEXT("Vaelen.Probe"), TEXT("Line traces against the builder's heights. Vaelen.Probe [n] [biasMm]"),
			   FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Probe));
} // namespace
