// VAELEN - VaelenGame
// Phase 14 task 14.08: the four console commands a host is driven by.
//
// Every one of them resolves the subsystem the same way - the command's own
// UWorld, its GameInstance, its subsystem - and says so and stops when there
// is no game instance, which is what happens in the editor without -game.
// 13.07c's Vaelen.View built its own world and needed none; this one holds a
// world across commands, which is exactly what a GameInstance subsystem is.
//
// The world moves on Vaelen.Day and on nothing else. There is no Tick in this
// module: ADR-0138's day turn is a recorded input, and a world that also moved
// on the frame clock could not be replayed from a key sequence.
//
// STATUS: VALIDATED (Phase 14) for what Phase 14 left here - built by
// UnrealBuildTool and RUN on 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development
// Editor): eighty-three days played at the keyboard, and Tools/Atlas replayed
// the stream headlessly to the same four digests, byte for byte.
// Tests/Run/Streams/README.md has the lines.
//
// UNVERIFIED (Phase 15 task 15.10) for everything added on 2026-09-18 - the
// look, the cadence argument, the camera. It parses against Tools/EngineShim
// and UnrealBuildTool has never seen it. A file that said VALIDATED over code
// no compiler has read would be exactly the fake "done" this project refuses,
// so the line is split rather than dated forward.
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "VaelenWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaelenPlay, Log, All);

namespace
{
	/// The one way in, and the one refusal. Null when there is no game
	/// instance, which is the editor without -game and is not an error worth
	/// a crash: the line says what to do about it.
	UVaelenWorldSubsystem* Held(UWorld* World_)
	{
		if (World_ == nullptr || World_->GetGameInstance() == nullptr)
		{
			UE_LOG(LogVaelenPlay, Warning, TEXT("LogVaelenPlay: no game instance - run with -game"));
			return nullptr;
		}
		UVaelenWorldSubsystem* Found = World_->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
		if (Found == nullptr)
		{
			UE_LOG(LogVaelenPlay, Warning, TEXT("LogVaelenPlay: no game instance - run with -game"));
		}
		return Found;
	}

	int32 NumberAt(const TArray<FString>& Args, int32 Which, int32 OrElse)
	{
		return Args.Num() > Which ? FCString::Atoi(*Args[Which]) : OrElse;
	}

	/// The eight verbs by name, in the order Intent names them.
	Vaelen::Player::Intent VerbNamed(const FString& Word)
	{
		for (Vaelen::uint32 i = 1; i < static_cast<Vaelen::uint32>(Vaelen::Player::Intent::Count); ++i)
		{
			const Vaelen::Player::Intent Kind = static_cast<Vaelen::Player::Intent>(i);
			if (Word == FString(ANSI_TO_TCHAR(Vaelen::Player::IntentName(Kind))))
			{
				return Kind;
			}
		}
		return Vaelen::Player::Intent::None;
	}

	void Play(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr)
		{
			return;
		}
		const int32 Size = NumberAt(Args, 0, 128);
		const int32 Years = NumberAt(Args, 1, 120);
		// The third argument is Phase 15's cadence, and it is OFF unless asked
		// for: a world with it on is a different world from the same seed, and
		// every number Phase 14 froze belongs to a world without it. A walk for
		// the 15.10 gate wants `Vaelen.Play 128 100 1`.
		const bool bStreaming = NumberAt(Args, 2, 0) != 0;
		if (!World->Begin(Size, Years, bStreaming))
		{
			UE_LOG(LogVaelenPlay, Warning,
				   TEXT("LogVaelenPlay: no world begun (one per host, and the map must generate)"));
			return;
		}
		const Vaelen::View::LifeView& Life = World->Life();
		UE_LOG(LogVaelenPlay, Log,
			   TEXT("LogVaelenPlay: AELVOR %d at %d years, cadence %s: person %u in region %u, %u alive"), Size, Years,
			   bStreaming ? TEXT("daily (Options::Stream on, replay with --stream)") : TEXT("yearly"),
			   static_cast<unsigned>(Life.Person), static_cast<unsigned>(Life.Region),
			   static_cast<unsigned>(World->World().People));
	}

	void Day(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr || !World->Begun())
		{
			return;
		}
		const int32 Days = NumberAt(Args, 0, 1);
		for (int32 i = 0; i < Days; ++i)
		{
			const double Started = FPlatformTime::Seconds();
			World->AdvanceDay(1);
			const double Took = (FPlatformTime::Seconds() - Started) * 1000.0;
			const Vaelen::View::LifeView& Life = World->Life();
			UE_LOG(LogVaelenPlay, Log, TEXT("LogVaelenPlay: day %u year %u: %u held, %u taken, %u refused, %.1f ms"),
				   static_cast<unsigned>(Life.DaysLived), static_cast<unsigned>(Life.Year),
				   static_cast<unsigned>(Life.Held), static_cast<unsigned>(Life.Taken),
				   static_cast<unsigned>(Life.Refused), Took);
		}
	}

	void Do(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr || !World->Begun())
		{
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogVaelenPlay, Warning, TEXT("LogVaelenPlay: Vaelen.Do <verb> [target] [amount]"));
			return;
		}
		const Vaelen::Player::Intent Kind = VerbNamed(Args[0]);
		if (Kind == Vaelen::Player::Intent::None)
		{
			UE_LOG(LogVaelenPlay, Warning, TEXT("LogVaelenPlay: no verb named %s"), *Args[0]);
			return;
		}
		// Through the page, as a key press is: what the page does not offer
		// costs the world nothing, and the refusal is the one it foresaw.
		Vaelen::Player::PlayerCommand What;
		const Vaelen::Player::Refusal Foreseen =
			Vaelen::View::Press(World->Panel(), Kind, static_cast<Vaelen::uint32>(NumberAt(Args, 1, 0)),
								static_cast<Vaelen::uint32>(NumberAt(Args, 2, 1)), What);
		if (Foreseen != Vaelen::Player::Refusal::None)
		{
			UE_LOG(LogVaelenPlay, Log, TEXT("LogVaelenPlay: the page does not offer %s: %s"), *Args[0],
				   ANSI_TO_TCHAR(Vaelen::Player::RefusalName(Foreseen)));
			return;
		}
		const Vaelen::Player::Refusal Answer = World->Mean(What);
		UE_LOG(LogVaelenPlay, Log, TEXT("LogVaelenPlay: %s: the door says %s, %u held"), *Args[0],
			   ANSI_TO_TCHAR(Vaelen::Player::RefusalName(Answer)), static_cast<unsigned>(World->Life().Held));
	}

	/// Where the host is looking, named outright. The camera does this for
	/// itself on every day turn (AVaelenPlayerController); this is the same
	/// input typed, for a host with no camera over the played world and for
	/// asking the warden a question whose answer you can predict.
	///
	/// It does not turn a day and does not reach the world: the subsystem
	/// remembers it and the next day turn hands it through the door. So a look
	/// typed between two day turns is one record on the second one's tick, and
	/// typing four of them in a row records the last.
	void Look(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr || !World->Begun())
		{
			return;
		}
		if (Args.Num() < 1)
		{
			// No arguments is a QUESTION, not a mistake: what is the host
			// looking at now. The camera sets this on every day turn, so
			// "nothing yet" and "at nowhere" are both real answers.
			int32 Region = 0;
			int32 Reach = 0;
			int32 Most = 0;
			if (World->Watching(Region, Reach, Most))
			{
				UE_LOG(LogVaelenPlay, Log, TEXT("LogVaelenPlay: looking at region %d, reach %d, most %d"), Region,
					   Reach, Most);
			}
			else
			{
				UE_LOG(LogVaelenPlay, Log,
					   TEXT("LogVaelenPlay: not looking anywhere yet, and no look is recorded "
							"until something looks. Vaelen.Look <region> [reach] [most]"));
			}
			return;
		}
		const int32 Region = NumberAt(Args, 0, 0);
		const int32 Reach = NumberAt(Args, 1, 1);
		const int32 Most = NumberAt(Args, 2, 4);
		World->Watch(Region, Reach, Most);
		UE_LOG(LogVaelenPlay, Log,
			   TEXT("LogVaelenPlay: looking at region %d, reach %d, most %d - recorded on the next day turn%s"), Region,
			   Reach, Most,
			   World->Streaming() ? TEXT("")
								  : TEXT(" (and the world will not act on it: this one was begun with the yearly "
										 "cadence, Vaelen.Play <size> <years> 1)"));
	}

	/// The region under the ground point a world coordinate names, for a host
	/// checking what its camera is over before it trusts it. Reads and records
	/// nothing.
	void Where(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr || !World->Begun())
		{
			return;
		}
		if (Args.Num() < 2)
		{
			UE_LOG(LogVaelenPlay, Warning,
				   TEXT("LogVaelenPlay: Vaelen.Where <x> <y> [tilesize], in whole centimetres"));
			return;
		}
		// WHOLE CENTIMETRES, through NumberAt, which is FCString::Atoi and is
		// the only string-to-number this module has ever compiled. The first
		// version used FCString::Atod for the fractions, which this project has
		// never built - and a tenth of a centimetre decides nothing when a tile
		// is a hundred of them.
		const double X = static_cast<double>(NumberAt(Args, 0, 0));
		const double Y = static_cast<double>(NumberAt(Args, 1, 0));
		const double Tile = static_cast<double>(NumberAt(Args, 2, 100));
		UE_LOG(LogVaelenPlay, Log, TEXT("LogVaelenPlay: (%.0f, %.0f) at %.0f cm a tile is region %d"), X, Y, Tile,
			   World->RegionUnderGround(X, Y, Tile > 0.0 ? Tile : 100.0));
	}

	void WriteStream(const TArray<FString>& Args, UWorld* World_)
	{
		UVaelenWorldSubsystem* World = Held(World_);
		if (World == nullptr || !World->Begun())
		{
			return;
		}
		FString Path;
		if (!World->WriteStream(Path))
		{
			UE_LOG(LogVaelenPlay, Warning, TEXT("LogVaelenPlay: could not write %s"), *Path);
			return;
		}

		// THE TWO LINES. Quoted identically here, in 14.10's checker and in
		// ADR-0138, because a headless replay of the stream just written must
		// print the same bytes after the prefix or the replay is not a replay.
		const Vaelen::View::LifeView& Life = World->Life();
		const Vaelen::Player::InputStream& Tape = World->Stream();
		const UVaelenWorldSubsystem::FDigests Marks = World->Digests();
		UE_LOG(LogVaelenPlay, Log,
			   TEXT("LogVaelenPlay: AELVOR %u seed %012llx: played %s (person %u, region %u) %u days, %u intents "
					"(%u taken, %u refused by the world, %u dropped at the door), state %016llx, log %016llx, "
					"life %016llx, panel %016llx"),
			   static_cast<unsigned>(Tape.Header.Size), static_cast<unsigned long long>(Tape.Header.Seed),
			   ANSI_TO_TCHAR(Life.Name), static_cast<unsigned>(Life.Person), static_cast<unsigned>(Life.Region),
			   static_cast<unsigned>(Tape.Days.size()), static_cast<unsigned>(Tape.Commands.size()),
			   static_cast<unsigned>(Life.Taken), static_cast<unsigned>(Life.Refused),
			   static_cast<unsigned>(Life.Dropped), static_cast<unsigned long long>(Marks.State),
			   static_cast<unsigned long long>(Marks.Log), static_cast<unsigned long long>(Marks.Life),
			   static_cast<unsigned long long>(Marks.Panel));

		// The verbs, in the order the keys are lettered (14.09), counted from
		// the stream itself: what was MEANT, whatever the world made of it.
		Vaelen::uint32 Tally[static_cast<Vaelen::usize>(Vaelen::Player::Intent::Count)] = {};
		for (const Vaelen::Player::Recorded& One : Tape.Commands)
		{
			if (One.Command.Kind < static_cast<Vaelen::uint8>(Vaelen::Player::Intent::Count))
			{
				++Tally[One.Command.Kind];
			}
		}
		const auto Of = [&Tally](Vaelen::Player::Intent Kind)
		{ return static_cast<unsigned>(Tally[static_cast<Vaelen::usize>(Kind)]); };
		UE_LOG(LogVaelenPlay, Log,
			   TEXT("LogVaelenPlay: verbs work %u rest %u eat %u wait %u speak %u give %u take %u move %u"),
			   Of(Vaelen::Player::Intent::Work), Of(Vaelen::Player::Intent::Rest), Of(Vaelen::Player::Intent::Eat),
			   Of(Vaelen::Player::Intent::Wait), Of(Vaelen::Player::Intent::Speak), Of(Vaelen::Player::Intent::Give),
			   Of(Vaelen::Player::Intent::Take), Of(Vaelen::Player::Intent::Move));
		// THE THIRD LINE, and deliberately a third one: the two above are quoted
		// verbatim in 14.10's checker and in ADR-0138, and a replay of this
		// stream must print them back byte for byte. Nothing may be added to
		// them. What 15.10 needs said - that the stream carries looks, and
		// which cadence the world was begun under, which a replay must be TOLD
		// because the stream does not carry it - goes here.
		UE_LOG(LogVaelenPlay, Log,
			   TEXT("LogVaelenPlay: %u looks, %u takings, cadence %s: replay with VaelenAtlas "
					"--gate <file> --want-bound 0%s"),
			   static_cast<unsigned>(Tape.Looks.size()), static_cast<unsigned>(Tape.Takings.size()),
			   World->Streaming() ? TEXT("daily") : TEXT("yearly"),
			   World->Streaming() ? TEXT("") : TEXT(" - but a walk for the 15.10 gate wants the daily one"));
		UE_LOG(LogVaelenPlay, Log, TEXT("LogVaelenPlay: %s"), *Path);
	}

	FAutoConsoleCommandWithWorldAndArgs GPlay(TEXT("Vaelen.Play"),
											  TEXT("Begin AELVOR and take somebody up. Vaelen.Play [size] [years] "
												   "[daily cadence: 0 or 1]"),
											  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Play));

	FAutoConsoleCommandWithWorldAndArgs GDay(TEXT("Vaelen.Day"), TEXT("Turn the day, recorded. Vaelen.Day [n]"),
											 FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Day));

	FAutoConsoleCommandWithWorldAndArgs
		GDo(TEXT("Vaelen.Do"), TEXT("Mean one intent, through the page. Vaelen.Do <verb> [target] [amount]"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Do));

	FAutoConsoleCommandWithWorldAndArgs GLook(TEXT("Vaelen.Look"),
											  TEXT("Where the host is looking. Vaelen.Look <region> [reach] [most]"),
											  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Look));

	FAutoConsoleCommandWithWorldAndArgs
		GWhere(TEXT("Vaelen.Where"), TEXT("What region a ground point is in. Vaelen.Where <x> <y> [tilesize]"),
			   FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Where));

	FAutoConsoleCommandWithWorldAndArgs GWrite(TEXT("Vaelen.Stream.Write"),
											   TEXT("Write the stream and print what a replay must come to"),
											   FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WriteStream));
} // namespace
