// VAELEN - VaelenRun tests
// Phase 16 task 16.12 clause (a): every day is a save point.
//
// ONE CELL AT ONE WIRING PROVED THE DIVERGENCE IN 16.05. Only a matrix proves
// the fix. This is {plain, lively no stream, stream no lively, stream and
// lively} x {64, 128} x {early, mid, late}: twenty-four cells, each one saved
// mid-flight, adopted into a fresh run, fed the SAME remaining records, and
// required to land on the same three digests as the run that was never
// interrupted.
//
// THE COMPARISON IS AGAINST THE CONTINUING SOURCE, not against another
// restore. Two restores that are wrong in the same way agree with each other
// perfectly for as long as you care to run them; 16.05 measured twelve day
// turns of that agreement before the shape of the test was fixed. So each
// (wiring, size) generates ONE world, records a horizon straight through, and
// the checkpoints taken along the way are answered against where that world
// actually ended up.
//
// AND EACH ARM IS WEIGHED, NOT JUST COMPARED. Three digests per cell read like
// three independent claims and were not. The life arm was found vacuous twice
// over, by two people counting differently, and BOTH counts are kept here
// because they catch different things:
//
//   WHO IS PLAYED (17.07, the nine). Nine of the original eighteen cells
//   carried nobody, so their life digests were equal for a reason that had
//   nothing to do with restoring. Two kinds: six with no Play at all, whose
//   Life() is the empty story, and three where Play is on and the world
//   offers nobody, whose Life() is the constant text "nobody". Each such cell
//   now asserts what it IS and is counted, and the harness refuses the digest
//   of no bytes outright (VT_CHECK_DIGEST_EQ).
//
//   WHETHER THE DIGEST MOVES (LifeArm, the twelve). Being played is not
//   enough. "stream, no lively" at 128 plays person 11 and writes its whole
//   chronicle at TakeUp, then stands still for the entire horizon - a PLAYED
//   cell whose life digest any restore lands on, which the count above passes
//   as genuine. Measured across the span the matrix restores over, only six
//   of eighteen cells had a life digest that moved, and all six sat in the
//   Stream rows: the agreeing half of the control had none.
//
// So a fourth wiring was added rather than the claim softened, every cell
// declares what its life digest is worth, and the declaration is measured on
// every run. The nine are still nine - the new row plays somebody at both
// sizes. See SaveMatrix.h.
//
// STATUS: PROTOTYPE (Phase 16) - ctest Run.SaveContinue
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Snapshot.h"
#include "SaveMatrix.h"
#include "VaelenTest.h"

#include <cstring>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	/// The digest of NO story: what `Where::Life` reads as when nobody is
	/// played, on both sides of every cell that carries nobody. Comparing it
	/// against itself was green for nine cells until 17.07 built a witness
	/// that refuses it; those cells now say what they are, and are counted.
	constexpr Hash64 EmptyStory = HashBytes(nullptr, 0);
	static_assert(EmptyStory == ::VaelenTest::Detail::EmptyBytesDigest, "the harness refuses this very value");
} // namespace

VAELEN_TEST(SaveContinue, EveryDayIsASavePointAcrossTheMatrix)
{
	uint32 Moving = 0;
	uint32 Fixed = 0;
	uint32 None = 0;
	uint32 EmptyCells = 0;
	for (const Wiring& W : Wirings)
	{
		for (usize S = 0; S < SizeCount; ++S)
		{
			const uint32 Size = Sizes[S];
			const Options O = OptionsFor(W, Size);

			// THE RUN THAT IS NEVER INTERRUPTED. It is built once, the saves
			// are taken out of it as it goes, and it is what every restore is
			// answered against.
			Aelvor Source(O);
			VT_REQUIRE(Source.Begin());
			Player::StartRules Rules;
			Door Recording(Source, Rules);
			if (W.Play)
			{
				const uint32 Who = Recording.TakeUp();
				// The anti-vacuity check this matrix would be worthless
				// without: a cell that claims to carry a played person and
				// carries nobody measures the empty world twice.
				VT_CHECK_MSG(Who == W.Who[S], "%s at %u: took person %u, and this cell is pinned to %u", W.Name, Size,
							 Who, W.Who[S]);
			}

			std::vector<std::vector<uint8>> Saves;
			std::vector<uint32> SavedAt;
			// The source's own life digest AT THE EARLIEST SAVE, which is what
			// says whether landing on Truth.Life is a claim or a tautology.
			Where AtFirstSave;
			for (uint32 Day = 0; Day < Horizon; ++Day)
			{
				for (uint32 Point : SavePoints)
				{
					if (Day == Point)
					{
						if (Saves.empty())
						{
							AtFirstSave = WhereItIs(Source);
						}
						std::vector<uint8> Bytes;
						VT_REQUIRE(BuildCheckpoint(Source, Recording.Stream(), Recording.Rules(), Bytes) ==
								   CheckpointResult::Ok);
						Saves.push_back(std::move(Bytes));
						SavedAt.push_back(Day);
					}
				}
				Recording.Look(BeatOf(Day));
				Recording.Day();
			}
			VT_REQUIRE(Saves.size() == 3u);
			const Where Truth = WhereItIs(Source);

			// WHAT THE LIFE ARM IS WORTH HERE, measured and answered against
			// the table. A cell whose chronicle quietly stops being written -
			// or starts - is a change in what this matrix covers, and it is
			// caught here rather than absorbed by three digests that all move
			// together.
			const LifeArm Measured = ArmOf(AtFirstSave, Truth);
			VT_CHECK_MSG(Measured == W.Life[S],
						 "%s at %u: the life arm measures %s and this cell is pinned to %s "
						 "(%zu bytes at the first save, %zu at the horizon)",
						 W.Name, Size, ArmName(Measured), ArmName(W.Life[S]), AtFirstSave.LifeBytes, Truth.LifeBytes);
			Moving += Measured == LifeArm::Moving ? 1u : 0u;
			Fixed += Measured == LifeArm::Fixed ? 1u : 0u;
			None += Measured == LifeArm::None ? 1u : 0u;

			for (usize k = 0; k < Saves.size(); ++k)
			{
				const uint32 From = SavedAt[k];

				// The tape as it stood AT the save, read back out of the file
				// rather than taken from the recorder, because what a host
				// gets back is the file.
				CheckpointView View;
				VT_REQUIRE(ReadCheckpoint(Saves[k].data(), Saves[k].size(), View).Result == CheckpointResult::Ok);
				Player::InputStream Carried;
				Player::StartRules CarriedRules;
				if (W.Play)
				{
					VT_REQUIRE(ReadStreamSection(View, Carried, CarriedRules));
				}

				Aelvor Restored(O);
				VT_REQUIRE(Restored.Adopt(Saves[k].data(), Saves[k].size()) == Aelvor::AdoptResult::Ok);
				Door Resumed(Restored, CarriedRules, Carried);
				for (uint32 Day = From; Day < Horizon; ++Day)
				{
					Resumed.Look(BeatOf(Day));
					Resumed.Day();
				}

				const Where Landed = WhereItIs(Restored);
				VT_CHECK_MSG(Landed.State == Truth.State,
							 "%s at %u saved on day %u: state %016llx, straight run %016llx", W.Name, Size, From,
							 static_cast<unsigned long long>(Landed.State),
							 static_cast<unsigned long long>(Truth.State));
				VT_CHECK_MSG(Landed.Log == Truth.Log, "%s at %u saved on day %u: log %016llx, straight run %016llx",
							 W.Name, Size, From, static_cast<unsigned long long>(Landed.Log),
							 static_cast<unsigned long long>(Truth.Log));
				if (W.Who[S] != 0u)
				{
					// A played life is a story, and a story is not the empty
					// string: without this line a cell whose life export broke
					// to "" on both sides would be green, and 17.07 measured
					// that nine cells were exactly that.
					VT_CHECK_MSG(Truth.Life != EmptyStory, "%s at %u: a played life reads as no story at all", W.Name,
								 Size);
					VT_CHECK_MSG(Landed.Life == Truth.Life,
								 "%s at %u saved on day %u: life %016llx, straight run %016llx", W.Name, Size, From,
								 static_cast<unsigned long long>(Landed.Life),
								 static_cast<unsigned long long>(Truth.Life));
				}
				else
				{
					// NOBODY IS PLAYED HERE, and comparing the two lives
					// measures nothing: the cell asserts what it IS instead,
					// and the count below pins how many there are. Two kinds,
					// found by asserting the wrong one first: the plain wiring
					// has no Play and its Life() is the EMPTY story; the stream
					// wiring at 64 has Play and the world offers nobody, so
					// ExportLife writes the story of "nobody" - a constant
					// text, the same on both sides of every save, which is
					// exactly as vacuous and not the empty string.
					VT_CHECK_MSG(Source.Played() == 0u && Restored.Played() == 0u,
								 "%s at %u saved on day %u: this cell is pinned to nobody and somebody is played (%u "
								 "straight, %u restored)",
								 W.Name, Size, From, static_cast<unsigned>(Source.Played()),
								 static_cast<unsigned>(Restored.Played()));
					VT_CHECK_MSG(W.Play ? Truth.Life != EmptyStory : Truth.Life == EmptyStory,
								 "%s at %u: a wiring %s Play should export %s (life %016llx)", W.Name, Size,
								 W.Play ? "with" : "without", W.Play ? "the story of nobody" : "no story",
								 static_cast<unsigned long long>(Truth.Life));
					++EmptyCells;
				}
			}
		}
	}

	// THE COVERAGE, STATED. Eight (wiring, size) worlds, three saves each:
	// twenty-four cells. Four worlds carry a life digest that moves across the
	// span the matrix restores over, two carry one that is written at TakeUp
	// and then stands still, and two carry none at all because Options::Play
	// is false and no restore can change that.
	//
	// The numbers are here so that the header above cannot go on claiming an
	// arm the matrix has stopped walking. Before the Phase 16 review closed,
	// this was Moving 2, Fixed 2, None 2 - and the whole agreeing half of the
	// control had no life arm whatsoever.
	VT_CHECK_MSG(Moving == 4u, "four worlds carry a moving chronicle, got %u", Moving);
	VT_CHECK_MSG(Fixed == 2u, "two carry one that never moves after TakeUp, got %u", Fixed);
	VT_CHECK_MSG(None == 2u, "two carry none at all, got %u", None);

	// AND THE NINE THAT CARRY NOBODY, 17.07's count, which the fourth wiring
	// leaves untouched: the row added above plays somebody at both sizes, so
	// the cells carrying nobody are still the two plain ones and "stream, no
	// lively" at 64. A tenth means a played cell stopped playing; an eighth
	// means a cell that carries nobody claims to.
	VT_CHECK_MSG(EmptyCells == 9u, "%u of the twenty-four cells carry nobody, pinned at 9", EmptyCells);
}

VAELEN_TEST(SaveContinue, TheLifeArmIsWorthWhatTheMatrixSaysItIs)
{
	// ADR-0149, and the reason the row above was added rather than the claim
	// softened. LifeArm is a declaration about what a cell's third digest can
	// SEE, and a declaration nothing tests is a comment. This walks the matrix
	// and breaks each cell on purpose in the one way the life arm is supposed
	// to catch - a world that adopts the save and then does not walk the days
	// it owes - and requires the life digest to notice exactly where the table
	// says it can, and to stay blind exactly where the table says it is blind.
	//
	// THE BLIND HALF IS ASSERTED, NOT SKIPPED. "None and Fixed cells do not
	// notice" is the vacuity the Phase 16 review found, written down as a
	// measurement instead of left as an assumption: six cells whose chronicle
	// is empty and six whose chronicle is settled at TakeUp cannot tell a
	// restored world that walked twenty-five days from one that walked none.
	// If that ever stops being true - if a wiring starts writing a chronicle
	// it did not write before - this fails and the table is re-measured,
	// which is the only way the coverage numbers stay honest.
	//
	// The EARLIEST save point is used because it is the widest span, and
	// ArmOf classifies on that span. At 64 the lively rows write their last
	// act before day 25, so the late save's life digest already matches the
	// horizon and would be blind in a Moving cell too - which is not a defect
	// in the arm, it is the arm telling the truth about that day.
	uint32 Caught = 0;
	uint32 Blind = 0;
	for (const Wiring& W : Wirings)
	{
		for (usize S = 0; S < SizeCount; ++S)
		{
			const uint32 Size = Sizes[S];
			const Options O = OptionsFor(W, Size);
			const uint32 From = SavePoints[0];

			Aelvor Source(O);
			VT_REQUIRE(Source.Begin());
			Player::StartRules Rules;
			Door Recording(Source, Rules);
			if (W.Play)
			{
				VT_REQUIRE(Recording.TakeUp() == W.Who[S]);
			}

			std::vector<uint8> Save;
			for (uint32 Day = 0; Day < Horizon; ++Day)
			{
				if (Day == From)
				{
					VT_REQUIRE(BuildCheckpoint(Source, Recording.Stream(), Recording.Rules(), Save) ==
							   CheckpointResult::Ok);
				}
				Recording.Look(BeatOf(Day));
				Recording.Day();
			}
			VT_REQUIRE(!Save.empty());
			const Where Truth = WhereItIs(Source);

			// The break: adopted, handed its tape, and then left standing.
			CheckpointView View;
			VT_REQUIRE(ReadCheckpoint(Save.data(), Save.size(), View).Result == CheckpointResult::Ok);
			Player::InputStream Carried;
			Player::StartRules CarriedRules;
			if (W.Play)
			{
				VT_REQUIRE(ReadStreamSection(View, Carried, CarriedRules));
			}
			Aelvor Stalled(O);
			VT_REQUIRE(Stalled.Adopt(Save.data(), Save.size()) == Aelvor::AdoptResult::Ok);
			Door Idle(Stalled, CarriedRules, Carried);
			(void)Idle;
			const Where Never = WhereItIs(Stalled);

			// The state arm catches this everywhere, which is precisely why it
			// could carry three vacuous life comparisons for two phases
			// without anybody noticing.
			VT_CHECK_MSG(Never.State != Truth.State,
						 "%s at %u: a world that walked none of its days must not match "
						 "the state of one that walked %u",
						 W.Name, Size, Horizon - From);

			const bool Noticed = Never.Life != Truth.Life;
			if (W.Life[S] == LifeArm::Moving)
			{
				++Caught;
				VT_CHECK_MSG(Noticed,
							 "%s at %u: this cell is pinned Moving, so its life digest must part when the "
							 "restored world never walks its days - and it did not (%016llx both ways, %zu bytes)",
							 W.Name, Size, static_cast<unsigned long long>(Truth.Life), Truth.LifeBytes);
			}
			else
			{
				++Blind;
				VT_CHECK_MSG(!Noticed,
							 "%s at %u: this cell is pinned %s, meaning its life digest CANNOT see the days "
							 "- and it parted, so the table under-states what this cell covers and must be "
							 "re-measured",
							 W.Name, Size, ArmName(W.Life[S]));
			}
		}
	}
	VT_CHECK_MSG(Caught == 4u, "four worlds catch it by the chronicle alone, got %u", Caught);
	VT_CHECK_MSG(Blind == 4u, "four are blind to it and say so, got %u", Blind);
}

VAELEN_TEST(SaveContinue, TheLifeArmCanTellVacuousFromMeasured)
{
	// ADR-0149: the classifier the matrix now leans on, made to answer each of
	// the three cases on purpose - including the one that used to pass
	// silently.
	//
	// THE MIDDLE CASE IS THE WHOLE POINT. A cell whose life digest is written
	// once and never moves compares a constant with itself at every save
	// point and passes however broken the restore is; it is indistinguishable
	// from a working one by the digest alone. ArmOf separates the two by
	// asking whether the digest moved across the span, which no comparison of
	// Landed.Life against Truth.Life can ask.
	Where Empty;
	VT_CHECK_MSG(ArmOf(Empty, Empty) == LifeArm::None, "no chronicle at all is None, got %s",
				 ArmName(ArmOf(Empty, Empty)));

	// The stories are HASHED RATHER THAN INVENTED. A hand-written sixteen-hex
	// digest here would be a frozen constant this file has no business
	// carrying - Tools/frozen_census.py says so, and it is right: a sentinel
	// that looks like a world digest is one grep away from being read as one.
	// These are what Where::Life actually holds, which is HashBytes over the
	// text ExportLife wrote.
	const auto StoryOf = [](const char* Text)
	{
		Where R;
		R.LifeBytes = std::strlen(Text);
		R.Life = HashBytes(Text, R.LifeBytes);
		return R;
	};

	// Non-empty and IDENTICAL at both ends - the "stream, no lively" shape,
	// whose chronicle is written at TakeUp and never touched again.
	const Where Still = StoryOf("nobody");
	VT_CHECK_MSG(ArmOf(Still, Still) == LifeArm::Fixed, "a chronicle that never moves is Fixed, got %s",
				 ArmName(ArmOf(Still, Still)));

	const Where Early = StoryOf("Ekde of Edavaken, the house of Unvarderdu");
	const Where Late = StoryOf("Ekde of Edavaken, the house of Unvarderdu\nand then a day was lived");
	VT_CHECK_MSG(ArmOf(Early, Late) == LifeArm::Moving, "one that differs across the span is Moving, got %s",
				 ArmName(ArmOf(Early, Late)));

	// AND EMPTINESS OUTRANKS MOVEMENT. HashBytes(nullptr, 0) is an ordinary
	// value, so a pair of Wheres could carry different life digests with no
	// chronicle behind either; length is what settles it, which is why Where
	// carries LifeBytes beside the digest at all.
	Where Nothing;
	Nothing.Life = Late.Life;
	VT_CHECK_MSG(ArmOf(Early, Nothing) == LifeArm::None, "an empty chronicle is None whatever the digest says, got %s",
				 ArmName(ArmOf(Early, Nothing)));
}
