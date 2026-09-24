// VAELEN - VaelenRun tests
// Phase 16 task 16.12 clause (a): every day is a save point.
//
// ONE CELL AT ONE WIRING PROVED THE DIVERGENCE IN 16.05. Only a matrix proves
// the fix. This is {plain, stream, stream and lively} x {64, 128} x {early,
// mid, late}: eighteen cells, each one saved mid-flight, adopted into a fresh
// run, fed the SAME remaining records, and required to land on the same three
// digests as the run that was never interrupted.
//
// THE COMPARISON IS AGAINST THE CONTINUING SOURCE, not against another
// restore. Two restores that are wrong in the same way agree with each other
// perfectly for as long as you care to run them; 16.05 measured twelve day
// turns of that agreement before the shape of the test was fixed. So each
// (wiring, size) generates ONE world, records a horizon straight through, and
// the checkpoints taken along the way are answered against where that world
// actually ended up.
//
// STATUS: PROTOTYPE (Phase 16) - ctest Run.SaveContinue
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Snapshot.h"
#include "SaveMatrix.h"
#include "VaelenTest.h"

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
	uint32 EmptyCells = 0;
	for (const Wiring& W : Wirings)
	{
		for (uint32 Size : Sizes)
		{
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
				VT_CHECK_MSG(Who == WhoPlaysAt(W, Size), "%s at %u: took person %u, and this cell is pinned to %u",
							 W.Name, Size, Who, WhoPlaysAt(W, Size));
			}

			std::vector<std::vector<uint8>> Saves;
			std::vector<uint32> SavedAt;
			for (uint32 Day = 0; Day < Horizon; ++Day)
			{
				for (uint32 Point : SavePoints)
				{
					if (Day == Point)
					{
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
				if (PlaysAt(W, Size))
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
	// Nine of eighteen: the number 17.07 found. A tenth means a played cell
	// stopped playing; an eighth means a cell that carries nobody claims to.
	VT_CHECK_MSG(EmptyCells == 9u, "%u of the eighteen cells carry nobody, pinned at 9", EmptyCells);
}
