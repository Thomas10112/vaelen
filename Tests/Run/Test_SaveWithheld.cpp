// VAELEN - VaelenRun tests
// Phase 16 task 16.12 clause (a), the control: the same matrix with the RUN
// section withheld.
//
// A GREEN TEST WITH A GREEN CONTROL CLOSES NOTHING. Run.SaveContinue says
// eighteen cells restore and continue to the same three digests. On its own
// that is also what a world would say if the run state it carries turned out
// not to matter, or if every cell happened to be a world where nothing the
// checkpoint restores is ever read again. This entry withholds exactly one
// thing - the RUN section - runs the identical matrix, and PASSES ONLY WHEN
// the cells that should part do part and the cells that should not, do not.
//
// THE RULE IT PINS: Options::Stream is what puts a warden on the day turn, and
// the warden reads what the run state holds. So every Stream=true cell must
// DIVERGE from the continuing source without its run, and every Stream=false
// cell must still AGREE - there is nothing in those worlds for the run to
// change. A control that only required divergence would pass just as happily
// if restoring were broken outright.
//
// THE AGREEING HALF USED TO AGREE ABOUT NOTHING. Every Stream=false cell was
// the "plain" wiring, which has Options::Play false, so its life digest was
// HashBytes(nullptr, 0) compared with itself - six cells asserting that an
// empty string equals an empty string. The "lively, no stream" row added
// closing the Phase 16 review is the other half of the rule: Play and Lively
// without Stream, a chronicle written across the very days being restored,
// and still required to agree when the run is withheld. That is now a claim
// with something in it. See SaveMatrix.h.
//
// THE RESTORE HERE IS Begin() + LoadSnapshot, NOT Adopt, because Adopt applies
// the run: it is the one path that can withhold it. That makes this the
// deliberate failure Run.SaveContinue is owed (ADR-0149) - the matrix has been
// made to fail, on purpose, by removing the thing it exists to check.
//
// STATUS: PROTOTYPE (Phase 16) - ctest Run.SaveWithheld
#include "SaveMatrix.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Snapshot.h"
#include "VaelenTest.h"

#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

VAELEN_TEST(SaveWithheld, TheRunSectionIsWhatTheStreamCellsNeed)
{
	uint32 Parted = 0;
	uint32 Agreed = 0;
	uint32 Moving = 0;
	uint32 Fixed = 0;
	uint32 None = 0;
	for (const Wiring& W : Wirings)
	{
		for (usize S = 0; S < SizeCount; ++S)
		{
			const uint32 Size = Sizes[S];
			const Options O = OptionsFor(W, Size);

			Aelvor Source(O);
			VT_REQUIRE(Source.Begin());
			Player::StartRules Rules;
			Door Recording(Source, Rules);
			if (W.Play)
			{
				VT_REQUIRE(Recording.TakeUp() == W.Who[S]);
			}

			std::vector<std::vector<uint8>> Saves;
			std::vector<uint32> SavedAt;
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
			const Where Truth = WhereItIs(Source);

			// The identical weighing the matrix does, because "the identical
			// matrix" has to mean the coverage too: a control walking cells
			// whose life arm has gone vacuous is controlling less than the
			// test it answers for, and would never say so.
			const LifeArm Measured = ArmOf(AtFirstSave, Truth);
			VT_CHECK_MSG(Measured == W.Life[S],
						 "%s at %u: the life arm measures %s and this cell is pinned to %s (%zu bytes at the first "
						 "save, %zu at the horizon)",
						 W.Name, Size, ArmName(Measured), ArmName(W.Life[S]), AtFirstSave.LifeBytes, Truth.LifeBytes);
			Moving += Measured == LifeArm::Moving ? 1u : 0u;
			Fixed += Measured == LifeArm::Fixed ? 1u : 0u;
			None += Measured == LifeArm::None ? 1u : 0u;

			for (usize k = 0; k < Saves.size(); ++k)
			{
				const uint32 From = SavedAt[k];

				CheckpointView View;
				VT_REQUIRE(ReadCheckpoint(Saves[k].data(), Saves[k].size(), View).Result == CheckpointResult::Ok);
				uint64 StateLength = 0;
				const uint8* State = View.Find(SectionKind::State, StateLength);
				VT_REQUIRE(State != nullptr);
				Player::InputStream Carried;
				Player::StartRules CarriedRules;
				if (W.Play)
				{
					VT_REQUIRE(ReadStreamSection(View, Carried, CarriedRules));
				}

				// The state, and NOT the run: no SetRunState call anywhere in
				// this file. That is the whole of the experiment.
				Aelvor Withheld(O);
				VT_REQUIRE(Withheld.Begin());
				VT_REQUIRE(LoadSnapshot(Withheld.Instance(), State, static_cast<usize>(StateLength)) ==
						   SnapshotResult::Ok);
				VT_CHECK_MSG(Withheld.Watching().empty(), "%s at %u day %u: the run did not come with the world",
							 W.Name, Size, From);

				Door Resumed(Withheld, CarriedRules, Carried);
				for (uint32 Day = From; Day < Horizon; ++Day)
				{
					Resumed.Look(BeatOf(Day));
					Resumed.Day();
				}

				const Where Landed = WhereItIs(Withheld);
				const bool Same = Landed.State == Truth.State && Landed.Log == Truth.Log && Landed.Life == Truth.Life;

				// THE OTHER SIDE OF THE ONE VARIABLE, and without it this file
				// proves less than it claims. A world restored here differs
				// from the matrix's in TWO ways - it was generated by Begin()
				// before the load, and it has no run - so "it parted" on its
				// own cannot say WHICH of the two parted it. The same path,
				// same generation, same load, with SetRunState added, has to
				// land on the continuing source. Then the run is the only
				// thing left that the divergence above can be made of.
				//
				// Only the Stream cells are paired: they are the ones whose
				// divergence is being attributed, and the plain cells agree
				// either way, which is what the else-branch below already
				// checks.
				if (W.Stream)
				{
					Aelvor Carrying(O);
					VT_REQUIRE(Carrying.Begin());
					VT_REQUIRE(LoadSnapshot(Carrying.Instance(), State, static_cast<usize>(StateLength)) ==
							   SnapshotResult::Ok);
					Aelvor::RunState TheRun;
					VT_REQUIRE(ReadRunSection(View, TheRun));
					VT_REQUIRE(Carrying.SetRunState(TheRun));
					Door Also(Carrying, CarriedRules, Carried);
					for (uint32 Day = From; Day < Horizon; ++Day)
					{
						Also.Look(BeatOf(Day));
						Also.Day();
					}
					const Where Kept = WhereItIs(Carrying);
					VT_CHECK_MSG(Kept.State == Truth.State && Kept.Log == Truth.Log && Kept.Life == Truth.Life,
								 "%s at %u saved on day %u: the SAME restore path WITH the run must reach the "
								 "continuing source, or the divergence below is not the run's doing "
								 "(state %016llx, source %016llx)",
								 W.Name, Size, From, static_cast<unsigned long long>(Kept.State),
								 static_cast<unsigned long long>(Truth.State));
				}
				Parted += Same ? 0u : 1u;
				Agreed += Same ? 1u : 0u;
				if (W.Stream)
				{
					VT_CHECK_MSG(!Same,
								 "%s at %u saved on day %u: WITHOUT the run this cell must part from the "
								 "continuing source, and it agreed (state %016llx)",
								 W.Name, Size, From, static_cast<unsigned long long>(Landed.State));
				}
				else
				{
					VT_CHECK_MSG(Same,
								 "%s at %u saved on day %u: nothing in a world without the daily cadence reads "
								 "the run, so withholding it must change nothing - state %016llx, source %016llx",
								 W.Name, Size, From, static_cast<unsigned long long>(Landed.State),
								 static_cast<unsigned long long>(Truth.State));
				}
			}
		}
	}

	// AND THE TALLY, so that a matrix which silently stopped covering both
	// sides cannot pass. Twelve Stream cells must part and twelve non-Stream
	// ones must agree; a run of twenty-four that is all one or all the other
	// is a matrix that lost a wiring, not a world that changed its mind.
	VT_CHECK_MSG(Parted == 12u, "twelve Stream cells parted without their run, got %u", Parted);
	VT_CHECK_MSG(Agreed == 12u, "twelve non-Stream cells were untouched by withholding it, got %u", Agreed);

	// The same coverage the matrix states, stated by the control. Six of the
	// twelve agreeing cells now carry a moving chronicle; before the Phase 16
	// review closed, none of them did.
	VT_CHECK_MSG(Moving == 4u, "four worlds carry a moving chronicle, got %u", Moving);
	VT_CHECK_MSG(Fixed == 2u, "two carry one that never moves after TakeUp, got %u", Fixed);
	VT_CHECK_MSG(None == 2u, "two carry none at all, got %u", None);
}
