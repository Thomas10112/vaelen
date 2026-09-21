// VAELEN - VaelenRun tests
// Phase 16 task 16.04: the container round-trips, its section table is honest,
// and a flipped byte is caught by the SECTION it lands in.
//
// The container exists because the image cannot grow. SaveSnapshot's trailer is
// computed over bytes that include the format version and the layout digest,
// ComputeStateDigest IS that trailer, and every frozen digest of fifteen phases
// hangs off it - so one added field moves all of them at once. Everything this
// phase needs therefore lives OUTSIDE the image, and the STATE section is the
// image copied verbatim. That last word is what the memcmp below is for: a
// container that re-encoded the image would still round-trip, still agree with
// its own digests, and still be wrong.
//
// STATUS: PROTOTYPE (Phase 16)
#include "VaelenTest.h"

#include "Vaelen/Core/Version.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	/// Three wirings, because a container that only ever saw one has never been
	/// asked whether its table describes bytes or describes a constant.
	struct Wiring
	{
		const char* Name;
		uint32 Size;
		uint32 Years;
		bool Play;
	};

	constexpr Wiring Wirings[] = {
		{"bare 16", 16u, 4u, false},
		{"play 16", 16u, 6u, true},
		{"bare 32", 32u, 4u, false},
	};

	Options OptionsFor(const Wiring& W)
	{
		Options O;
		O.Size = W.Size;
		O.PreHistory = W.Years;
		O.Years = W.Years;
		O.Play = W.Play;
		return O;
	}
} // namespace

VAELEN_TEST(Checkpoint, ContainerRoundTrip)
{
	for (const Wiring& W : Wirings)
	{
		Aelvor A(OptionsFor(W));
		VT_REQUIRE(A.Begin());

		std::vector<uint8> Bytes;
		VT_CHECK_MSG(BuildCheckpoint(A, Bytes) == CheckpointResult::Ok, "%s builds", W.Name);
		VT_REQUIRE(!Bytes.empty());

		CheckpointView View;
		const CheckpointRefusal R = ReadCheckpoint(Bytes.data(), Bytes.size(), View);
		VT_CHECK_MSG(R.Result == CheckpointResult::Ok, "%s", CheckpointResultToString(R.Result));
		VT_REQUIRE(R.Result == CheckpointResult::Ok);

		VT_CHECK_MSG(View.Version == CheckpointVersion, "the container's own version");
		VT_CHECK_MSG(View.InnerFormat == VAELEN_SAVE_FORMAT_VERSION, "the image's, read out of the image");
		VT_CHECK_MSG(View.Seed == A.Instance().Config().Seed, "seed");
		VT_CHECK_MSG(View.Tick == static_cast<uint64>(A.Now()), "tick");
		VT_CHECK_MSG(View.LogEvents == A.Instance().Log().Count(), "the log's event count");
		VT_CHECK_MSG(View.LogBytes > View.LogEvents, "and its byte length, which is the larger number");

		// THE VERBATIM CLAUSE. A fresh SaveSnapshot of the same world must be
		// byte-identical to the STATE section. A container that re-encoded -
		// even losslessly - would pass every other check on this page.
		uint64 Length = 0;
		const uint8* State = View.Find(SectionKind::State, Length);
		VT_REQUIRE(State != nullptr);
		std::vector<uint8> Fresh;
		VT_REQUIRE(SaveSnapshot(A.Instance(), Fresh) == SnapshotResult::Ok);
		VT_CHECK_MSG(Length == Fresh.size(), "%s: STATE is the image's length", W.Name);
		VT_REQUIRE(Length == Fresh.size());
		VT_CHECK_MSG(std::memcmp(State, Fresh.data(), Fresh.size()) == 0,
					 "%s: STATE is the image, byte for byte, never re-encoded", W.Name);
	}
}

VAELEN_TEST(Checkpoint, SectionTableIsHonest)
{
	// The table describes the bytes or it describes nothing. Every section is
	// inside the payload area, they are in order, they do not overlap, and
	// together they account for EVERY byte between the table and the trailer -
	// unclaimed bytes are where a second meaning hides.
	for (const Wiring& W : Wirings)
	{
		Aelvor A(OptionsFor(W));
		VT_REQUIRE(A.Begin());
		std::vector<uint8> Bytes;
		VT_REQUIRE(BuildCheckpoint(A, Bytes) == CheckpointResult::Ok);

		CheckpointView View;
		VT_REQUIRE(ReadCheckpoint(Bytes.data(), Bytes.size(), View).Result == CheckpointResult::Ok);
		VT_REQUIRE(!View.Sections.empty());

		uint64 Reach = View.Sections.front().Offset;
		uint64 Total = 0;
		for (const SectionEntry& E : View.Sections)
		{
			VT_CHECK_MSG(E.Offset >= Reach, "%s: sections are in order and do not overlap", W.Name);
			VT_CHECK_MSG(E.Offset + E.Length + 8u <= Bytes.size(), "%s: and stay inside the container", W.Name);
			Reach = E.Offset + E.Length;
			Total += E.Length;
		}
		const uint64 Payload = static_cast<uint64>(Bytes.size()) - View.Sections.front().Offset - 8u;
		VT_CHECK_MSG(Total == Payload, "%s: the lengths sum to the payload with delta 0", W.Name);
	}
}

VAELEN_TEST(Checkpoint, UnknownRequiredFlagIsRefusedAndTheOtherHalfIsCarried)
{
	// THE TWO HALVES OF THE FLAG WORD, which is the channel the image's own
	// Flags field was reserved for in Phase 01 and never given - it is written
	// 0 and never read.
	Aelvor A(OptionsFor(Wirings[0]));
	VT_REQUIRE(A.Begin());

	// The may-ignore half is carried through a read untouched, so a tool that
	// does not understand a bit cannot silently drop it.
	std::vector<uint8> Carried;
	VT_REQUIRE(BuildCheckpoint(A, Carried, uint16{1} << 5) == CheckpointResult::Ok);
	CheckpointView View;
	const CheckpointRefusal Kept = ReadCheckpoint(Carried.data(), Carried.size(), View);
	VT_CHECK_MSG(Kept.Result == CheckpointResult::Ok, "a may-ignore bit is not a refusal");
	VT_CHECK_MSG((View.Flags >> 16) == (uint32{1} << 5), "and it comes back out where it went in");

	// The must-understand half is the opposite. Bit 5 of the low half is set by
	// hand here - no build writes one yet, which is the point: this is what
	// happens to TODAY's reader when a LATER one adds meaning.
	std::vector<uint8> Refused;
	VT_REQUIRE(BuildCheckpoint(A, Refused) == CheckpointResult::Ok);
	const usize FlagsAt = 8u + 4u;
	Refused[FlagsAt] = static_cast<uint8>(Refused[FlagsAt] | (1u << 5));
	// Resealed, so that the trailer is not what turns it away. Without this the
	// test would measure the trailer and claim to have measured the flags.
	const Hash64 Sealed = HashBytes(reinterpret_cast<const char*>(Refused.data()), Refused.size() - 8u);
	std::memcpy(Refused.data() + Refused.size() - 8u, &Sealed, 8u);

	CheckpointView Ignored;
	const CheckpointRefusal R = ReadCheckpoint(Refused.data(), Refused.size(), Ignored);
	VT_CHECK_MSG(R.Result == CheckpointResult::UnknownRequiredFlag, "%s", CheckpointResultToString(R.Result));
	VT_CHECK_MSG(R.UnknownBit == 5u, "and it names the bit rather than saying 'newer version'");
}

VAELEN_TEST(Checkpoint, AFlippedByteIsCaughtBySectionAndNotOnlyByTheTrailer)
{
	// THE CONTROL THE PHASE ASKED FOR, and it has two arms.
	//
	// A byte is flipped and the CONTAINER TRAILER IS THEN RECOMPUTED, so the
	// trailer agrees with the damaged bytes - exactly what an attacker, or a
	// tool that rewrote a file it half understood, would leave behind. What
	// must still catch it is the per-section digest, which lives in the section
	// TABLE and not in the image, and therefore costs no frozen digest.
	//
	// The second arm is the one that makes the first mean anything: the same
	// sweep with the section digest check skipped must let them through. A
	// container whose sections were not digested would pass arm one by accident
	// if the trailer happened to be the thing doing the work.
	Aelvor A(OptionsFor(Wirings[1]));
	VT_REQUIRE(A.Begin());
	std::vector<uint8> Good;
	VT_REQUIRE(BuildCheckpoint(A, Good) == CheckpointResult::Ok);

	CheckpointView View;
	VT_REQUIRE(ReadCheckpoint(Good.data(), Good.size(), View).Result == CheckpointResult::Ok);
	uint64 StateLength = 0;
	VT_REQUIRE(View.Find(SectionKind::State, StateLength) != nullptr);
	const uint64 StateAt = View.Sections.front().Offset;

	// Three bands across the STATE section, because the image is not uniform:
	// the header and clock at the front, the pools in the middle, the map and
	// the log at the back. 60 offsets in each.
	uint32 Swept = 0;
	uint32 CaughtWithDigests = 0;
	uint32 CaughtWithout = 0;
	for (uint32 Band = 0; Band < 3u; ++Band)
	{
		const uint64 BandAt = StateAt + (StateLength * Band) / 3u;
		for (uint32 Step = 0; Step < 60u; ++Step)
		{
			const uint64 At = BandAt + Step * 7u;
			if (At >= StateAt + StateLength)
			{
				continue;
			}
			std::vector<uint8> Bad = Good;
			Bad[static_cast<usize>(At)] = static_cast<uint8>(Bad[static_cast<usize>(At)] ^ 0xFFu);
			// Reseal the CONTAINER trailer over the damaged bytes.
			const Hash64 Sealed = HashBytes(reinterpret_cast<const char*>(Bad.data()), Bad.size() - 8u);
			std::memcpy(Bad.data() + Bad.size() - 8u, &Sealed, 8u);

			++Swept;
			CheckpointView Out;
			CaughtWithDigests += ReadCheckpoint(Bad.data(), Bad.size(), Out).Result != CheckpointResult::Ok ? 1u : 0u;

			// ARM TWO: the same bytes judged WITHOUT the per-section digest.
			// Simulated by resealing the section's digest too, which is exactly
			// what a container that did not have per-section digests would
			// amount to - nothing left to disagree with the bytes.
			std::vector<uint8> Blind = Bad;
			const Hash64 Section =
				HashBytes(reinterpret_cast<const char*>(Blind.data() + StateAt), static_cast<usize>(StateLength));
			const usize DigestAt = static_cast<usize>(8u + 4u + 4u + 4u + 8u + 8u + 8u + 8u + 4u) + 2u + 4u + 8u + 8u;
			std::memcpy(Blind.data() + DigestAt, &Section, 8u);
			const Hash64 Reseal = HashBytes(reinterpret_cast<const char*>(Blind.data()), Blind.size() - 8u);
			std::memcpy(Blind.data() + Blind.size() - 8u, &Reseal, 8u);
			CheckpointView Away;
			CaughtWithout += ReadCheckpoint(Blind.data(), Blind.size(), Away).Result != CheckpointResult::Ok ? 1u : 0u;
		}
	}

	// PRINTED, because "the control passed" is not a measurement and the
	// margin between the two arms is the whole result.
	std::printf("  [checkpoint] %u offsets swept: %u refused with section digests, %u without\n", Swept,
				CaughtWithDigests, CaughtWithout);
	VT_CHECK_MSG(Swept >= 180u, "180 offsets across three bands");
	VT_CHECK_MSG(CaughtWithDigests == Swept, "every one of them refused, by the section it landed in");
	VT_CHECK_MSG(CaughtWithout < Swept / 2u, "and the control: without the section digest most of them get through, "
											 "so it is the digest doing the work and not the trailer");
}
