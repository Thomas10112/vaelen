// VAELEN - VaelenRun tests
// Phase 16 task 16.12 clause (c): a horizon in which the played person DIES,
// saved before the death, on it, and after it.
//
// WHY A DEATH AND NOT ANOTHER HUNDRED ORDINARY DAYS. A death is the one day
// that exercises Release, a fresh TakeUp, NearDetail's give-back path and a
// recorded TakenUp all at once - the machinery Near_ feeds, and the likeliest
// place for a fourth piece of run state to be hiding where the matrix would
// never look. 15.03's history says what happens when a give-back path is never
// exercised: saturation at MaxWanted, every Move refused TooFar, in silence,
// for the rest of that world's life.
//
// THIS TEST ASSERTS ITS OWN PRECONDITION AND CANNOT PASS VACUOUSLY. If nobody
// dies it FAILS, loudly, rather than reporting three green save points over a
// horizon where nothing interesting happened. That is the whole difference
// between a death test and a test that happens to have the word death in it.
//
// FINDING THE CELL TOOK A SEARCH, and the numbers are worth keeping because
// they are not guessable. A world only contains people as old as its history:
// at 20+10 years nobody is over about thirty, so every start window above
// forty offers NOBODY and a death test built there would have quietly played
// nothing at all. At 60+30 years, taking up somebody aged 85 or over:
//
//   ages 80-120   person 100   no death in 1600 days
//   ages 85-120   person 300   dies on day 1080, person 100 taken up - 2 takings
//   ages 88-120   person 300   dies on day 1080 and NOBODY is left - 1 taking
//
// The middle row is this test. The last one is the case Door.h names - a played
// person who died with nobody to take up - and it is a different horizon for a
// different task, not this one.
//
// STATUS: PROTOTYPE (Phase 16) - ctest Run.SaveDeath
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Snapshot.h"
#include "VaelenTest.h"

#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	constexpr uint32 Horizon = 1200u;
	/// MEASURED, AND THEN ASSERTED. The day is written here so that one world
	/// generation serves the whole test, and the run below refuses to continue
	/// if the death does not land exactly here: a world that changed when
	/// somebody dies must fail this test rather than slide quietly to another
	/// day and keep passing.
	constexpr uint32 DeathDay = 1080u;
	constexpr uint32 BeforeDay = 1000u;
	constexpr uint32 AfterDay = 1150u;

	Attention BeatOf(uint32 Day)
	{
		Attention At;
		At.Region = static_cast<uint32>(1u + (Day % 7u));
		At.Reach = 1u;
		At.Most = 0u;
		return At;
	}

	Options DeathOptions()
	{
		Options O;
		O.Size = 64u;
		O.PreHistory = 60u;
		O.Years = 30u;
		O.Play = true;
		O.Stream = true;
		O.Lively = true;
		return O;
	}

	Player::StartRules OldEnoughToDie()
	{
		Player::StartRules R;
		R.FromAge = 85u;
		R.ToAge = 120u;
		// 0 takes whoever is there: at eighty-five, insisting on a bound life
		// as well narrows the world to nobody.
		R.WantBound = 0u;
		return R;
	}
} // namespace

VAELEN_TEST(SaveDeath, ASaveOnTheDayTheyDieIsStillASave)
{
	const Options O = DeathOptions();
	const Player::StartRules Rules = OldEnoughToDie();

	Aelvor Source(O);
	VT_REQUIRE(Source.Begin());
	Door Living(Source, Rules);
	const uint32 First = Living.TakeUp();
	VT_CHECK_MSG(First != 0u, "a world of 60+30 years offers somebody aged 85 or over");
	VT_REQUIRE(First != 0u);
	VT_CHECK_MSG(Source.PlayedAlive(), "and they are alive when they are taken up");

	std::vector<uint8> BeforeIt;
	std::vector<uint8> OnIt;
	std::vector<uint8> AfterIt;
	uint32 DiedOn = 0;
	uint32 Deaths = 0;
	uint32 Second = 0;
	for (uint32 Day = 0; Day < Horizon; ++Day)
	{
		const uint32 Playing = Source.Played();
		Living.Look(BeatOf(Day));
		Living.Day();
		if (Source.Played() != Playing)
		{
			++Deaths;
			if (DiedOn == 0u)
			{
				DiedOn = Day;
				Second = Source.Played();
			}
		}
		// AFTER the day turn, so that "saved on the death" means a container
		// holding the world in which the death has just happened - the release,
		// the give-back and the fresh taking all inside it.
		if (Day == BeforeDay)
		{
			VT_REQUIRE(BuildCheckpoint(Source, Living.Stream(), Living.Rules(), BeforeIt) == CheckpointResult::Ok);
		}
		if (Day == DeathDay)
		{
			VT_REQUIRE(BuildCheckpoint(Source, Living.Stream(), Living.Rules(), OnIt) == CheckpointResult::Ok);
		}
		if (Day == AfterDay)
		{
			VT_REQUIRE(BuildCheckpoint(Source, Living.Stream(), Living.Rules(), AfterIt) == CheckpointResult::Ok);
		}
	}

	// THE PRECONDITION, AND THE TEST STOPS HERE IF IT DOES NOT HOLD.
	VT_CHECK_MSG(Deaths != 0u, "somebody died in this horizon - without that this test measures nothing");
	VT_REQUIRE(Deaths != 0u);
	VT_CHECK_MSG(DiedOn == DeathDay, "the death is on day %u, where this test saves; it fell on day %u", DeathDay,
				 DiedOn);
	VT_REQUIRE(DiedOn == DeathDay);
	VT_CHECK_MSG(Second != 0u && Second != First, "somebody else was taken up: %u after %u", Second, First);
	VT_CHECK_MSG(Living.Stream().Takings.size() == 2u, "and the tape carries BOTH takings, not one: %zu",
				 Living.Stream().Takings.size());

	const Hash64 TrueState = Source.StateDigest();
	const Hash64 TrueLog = Source.LogDigest();
	const std::string Story = Source.Life();
	const Hash64 TrueLife = HashBytes(Story.data(), Story.size());

	struct Cell
	{
		const char* Name;
		uint32 Day;
		const std::vector<uint8>* Bytes;
	};
	const Cell Cells[] = {
		{"before the death", BeforeDay, &BeforeIt},
		{"on the death", DeathDay, &OnIt},
		{"after the death", AfterDay, &AfterIt},
	};

	for (const Cell& C : Cells)
	{
		CheckpointView View;
		VT_REQUIRE(ReadCheckpoint(C.Bytes->data(), C.Bytes->size(), View).Result == CheckpointResult::Ok);
		Player::InputStream Carried;
		Player::StartRules CarriedRules;
		VT_REQUIRE(ReadStreamSection(View, Carried, CarriedRules));
		// The tape taken at each point knows how many lives it has already
		// seen, which is the one thing a save across a death has to carry that
		// a save on an ordinary day does not.
		const usize Expected = C.Day < DeathDay ? 1u : 2u;
		VT_CHECK_MSG(Carried.Takings.size() == Expected, "%s: the carried tape holds %zu taking(s), expected %zu",
					 C.Name, Carried.Takings.size(), Expected);

		Aelvor Back(O);
		VT_REQUIRE(Back.Adopt(C.Bytes->data(), C.Bytes->size()) == Aelvor::AdoptResult::Ok);
		VT_CHECK_MSG(Back.Played() != 0u, "%s: somebody is still played after the restore", C.Name);
		Door Resumed(Back, CarriedRules, Carried);
		for (uint32 Day = C.Day + 1u; Day < Horizon; ++Day)
		{
			Resumed.Look(BeatOf(Day));
			Resumed.Day();
		}

		const std::string Told = Back.Life();
		VT_CHECK_MSG(Back.StateDigest() == TrueState, "%s: state %016llx, uninterrupted %016llx", C.Name,
					 static_cast<unsigned long long>(Back.StateDigest()), static_cast<unsigned long long>(TrueState));
		VT_CHECK_MSG(Back.LogDigest() == TrueLog, "%s: log differs from the uninterrupted run", C.Name);
		VT_CHECK_MSG(HashBytes(Told.data(), Told.size()) == TrueLife, "%s: life differs from the uninterrupted run",
					 C.Name);
	}
}

VAELEN_TEST(SaveDeath, ADoorRefusesToRelabelATapeFromAnotherWorld)
{
	// Found by the Phase 16 adversarial review, and it is a defect in 16.11's
	// own reasoning rather than in its machinery.
	//
	// Door(Aelvor&, StartRules, InputStream) stamped the WORLD's header onto
	// the tape unconditionally. Where the two already agree that is right, and
	// it is what 16.10 argued for: the world has been checked field by field
	// against what the file declared, the tape's copy has not. But the stamp
	// was applied where they DISAGREE too, and there it erases the only thing
	// Replay has to go on - Player::SameWorld(Fresh.Header(), S.Header) is the
	// single guard that a stream belongs to its world.
	//
	// Nothing on the 16.11 path compares a tape's header to anything:
	// ReadStreamSection does not, the decoder is called with no expectation,
	// and BuildCheckpoint accepts whatever InputStream it is handed. There is
	// also a restore route with no HOST check at all - Begin() + LoadSnapshot,
	// which Test_SaveWithheld uses. So a foreign tape could be adopted into a
	// live session, relabelled, and written back into the next save's STREAM
	// section under the wrong world's name.
	//
	// THIS TEST IS THE DELIBERATE FAILURE of the old behaviour: on the code as
	// it stood, Foreign() did not exist and the header comparison below found
	// two identical headers.
	Options Small;
	Small.Size = 32u;
	Small.PreHistory = 10u;
	Small.Years = 10u;
	Small.Play = true;
	Small.Stream = true;
	Small.Lively = true;

	Options Other = Small;
	Other.Seed = Small.Seed + 1u; // another world by the one field SameWorld weighs first

	Aelvor Here(Small);
	VT_REQUIRE(Here.Begin());
	Aelvor There(Other);
	VT_REQUIRE(There.Begin());
	VT_CHECK_MSG(!Player::SameWorld(Here.Header(), There.Header()),
				 "the two worlds really are different, or this test proves nothing");

	// A tape that was recorded THERE, handed to a door that opens HERE.
	Player::StartRules Rules;
	Door Elsewhere(There, Rules);
	Elsewhere.TakeUp();
	for (uint32 Day = 0; Day < 3u; ++Day)
	{
		Elsewhere.Look(BeatOf(Day));
		Elsewhere.Day();
	}
	const Player::StreamHeader Recorded = Elsewhere.Stream().Header;
	VT_REQUIRE(Recorded.Size != 0u);

	Door Wrong(Here, Rules, Elsewhere.Stream());
	VT_CHECK_MSG(Wrong.Foreign(), "the door says the tape names another world");
	VT_CHECK_MSG(Wrong.Stream().Header.Seed == Recorded.Seed, "and it did NOT relabel it: seed %llu, recorded %llu",
				 static_cast<unsigned long long>(Wrong.Stream().Header.Seed),
				 static_cast<unsigned long long>(Recorded.Seed));
	VT_CHECK_MSG(!Player::SameWorld(Wrong.Stream().Header, Here.Header()),
				 "so Replay's own guard still has something to refuse");

	// AND THE ORDINARY CASE IS UNTOUCHED: a tape from THIS world is taken, and
	// the header it ends up with is the world's verified one.
	Door Right(Here, Rules, Player::InputStream{});
	VT_CHECK_MSG(!Right.Foreign(), "a tape that never recorded names no world and is not foreign");
	VT_CHECK_MSG(Player::SameWorld(Right.Stream().Header, Here.Header()), "and it is stamped with this world");

	Door Mine(Here, Rules);
	Mine.TakeUp();
	Mine.Look(BeatOf(0u));
	Mine.Day();
	Door Resumed(Here, Rules, Mine.Stream());
	VT_CHECK_MSG(!Resumed.Foreign(), "a tape recorded HERE is not foreign");
	VT_CHECK_MSG(Player::SameWorld(Resumed.Stream().Header, Here.Header()), "and it still carries this world's header");
}
