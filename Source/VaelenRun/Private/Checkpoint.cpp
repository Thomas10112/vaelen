// VAELEN - VaelenRun
// The container. See Public/Vaelen/Run/Checkpoint.h for the layout and for why
// nothing is ever added to the image itself.
//
// STATUS: PROTOTYPE (Phase 16 task 16.04) - Tests/Run/Test_Checkpoint.cpp round
//         trips it at three wirings, checks the table against the bytes, and
//         sweeps 180 flipped bytes past a recomputed container trailer. The
//         RUN, HOST and STREAM section kinds are declared and not yet written.
#include "Vaelen/Run/Checkpoint.h"

#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <cstring>

namespace Vaelen::Run
{
	namespace
	{
		constexpr usize MagicBytes = 8;
		constexpr usize HeaderBytes = MagicBytes + 4 + 4 + 4 + 8 + 8 + 8 + 8 + 4;
		constexpr usize EntryBytes = 2 + 4 + 8 + 8 + 8;
		constexpr usize TrailerBytes = 8;

		/// The must-understand bits this build knows. Every OTHER bit of the low
		/// half is a refusal, which is the whole point: a reader that shrugs at
		/// a bit it does not know is a reader that silently loses meaning.
		constexpr uint16 KnownRequiredFlags = 0u;

		void PutU16(std::vector<uint8>& Out, uint16 Value)
		{
			Out.push_back(static_cast<uint8>(Value & 0xFFu));
			Out.push_back(static_cast<uint8>((Value >> 8) & 0xFFu));
		}

		void PutU32(std::vector<uint8>& Out, uint32 Value)
		{
			for (int Shift = 0; Shift < 32; Shift += 8)
			{
				Out.push_back(static_cast<uint8>((Value >> Shift) & 0xFFu));
			}
		}

		void PutU64(std::vector<uint8>& Out, uint64 Value)
		{
			for (int Shift = 0; Shift < 64; Shift += 8)
			{
				Out.push_back(static_cast<uint8>((Value >> Shift) & 0xFFu));
			}
		}

		uint16 GetU16(const uint8* At) noexcept
		{
			return static_cast<uint16>(static_cast<uint16>(At[0]) | (static_cast<uint16>(At[1]) << 8));
		}

		uint32 GetU32(const uint8* At) noexcept
		{
			uint32 Value = 0;
			for (int Index = 3; Index >= 0; --Index)
			{
				Value = (Value << 8) | static_cast<uint32>(At[Index]);
			}
			return Value;
		}

		uint64 GetU64(const uint8* At) noexcept
		{
			uint64 Value = 0;
			for (int Index = 7; Index >= 0; --Index)
			{
				Value = (Value << 8) | static_cast<uint64>(At[Index]);
			}
			return Value;
		}

		Hash64 DigestOf(const uint8* At, usize Size) noexcept
		{
			return HashBytes(reinterpret_cast<const char*>(At), Size);
		}
	} // namespace

	const char* CheckpointResultToString(CheckpointResult Result) noexcept
	{
		switch (Result)
		{
		case CheckpointResult::Ok:
			return "Ok";
		case CheckpointResult::BadMagic:
			return "BadMagic";
		case CheckpointResult::VersionMismatch:
			return "VersionMismatch";
		case CheckpointResult::InnerVersionMismatch:
			return "InnerVersionMismatch";
		case CheckpointResult::Truncated:
			return "Truncated";
		case CheckpointResult::Corrupt:
			return "Corrupt";
		case CheckpointResult::UnknownRequiredFlag:
			return "UnknownRequiredFlag";
		case CheckpointResult::BadSectionTable:
			return "BadSectionTable";
		case CheckpointResult::StateRefused:
			return "StateRefused";
		}
		return "Unknown";
	}

	const uint8* CheckpointView::Find(SectionKind Kind, uint64& OutLength) const noexcept
	{
		for (const SectionEntry& Entry : Sections)
		{
			if (Entry.Kind == static_cast<uint16>(Kind))
			{
				OutLength = Entry.Length;
				return Base + Entry.Offset;
			}
		}
		OutLength = 0;
		return nullptr;
	}

	CheckpointResult BuildCheckpoint(const Aelvor& Run, std::vector<uint8>& Out, uint16 MayIgnore)
	{
		const usize Start = Out.size();
		const auto Refuse = [&Out, Start](CheckpointResult Why)
		{
			Out.resize(Start);
			return Why;
		};

		const World& W = Run.Instance();

		// THE STATE SECTION IS SaveSnapshot'S OUTPUT AND NOTHING ELSE. It is
		// built into its own buffer first, because the container's header has
		// to record the image's format version and the section's length before
		// either is known, and because a refusal here must leave Out alone.
		std::vector<uint8> State;
		const SnapshotResult Saved = SaveSnapshot(W, State);
		if (Saved != SnapshotResult::Ok)
		{
			return Refuse(CheckpointResult::StateRefused);
		}
		if (State.size() < MagicBytes + 4)
		{
			return Refuse(CheckpointResult::Truncated);
		}
		// Copied out of the image rather than assumed from the build's own
		// constant: a reader must be able to say WHICH of the two versions it
		// disagrees with, and that is only possible if this is the image's.
		const uint32 InnerFormat = GetU32(State.data() + MagicBytes);

		// The log's shape, so that a reader can say why a save is large without
		// parsing the image. Measured in 16.01: the log is 75% of any image
		// that has a history at all.
		std::vector<uint8> LogBytes;
		W.Log().WriteTo(LogBytes);

		// 16.04 writes one section. RUN, HOST and STREAM are declared in the
		// header and filled by 16.05 to 16.07; the table is variable-length by
		// design, so adding them costs no container version.
		const uint32 SectionCount = 1u;
		const uint64 TableAt = HeaderBytes;
		const uint64 PayloadAt = TableAt + static_cast<uint64>(SectionCount) * EntryBytes;

		Out.insert(Out.end(), CheckpointMagic, CheckpointMagic + MagicBytes);
		PutU32(Out, CheckpointVersion);
		PutU32(Out, static_cast<uint32>(MayIgnore) << 16);
		PutU32(Out, InnerFormat);
		PutU64(Out, W.Config().Seed);
		PutU64(Out, static_cast<uint64>(W.Now()));
		PutU64(Out, W.Log().Count());
		PutU64(Out, static_cast<uint64>(LogBytes.size()));
		PutU32(Out, SectionCount);

		PutU16(Out, static_cast<uint16>(SectionKind::State));
		PutU32(Out, 0u);
		PutU64(Out, PayloadAt);
		PutU64(Out, static_cast<uint64>(State.size()));
		PutU64(Out, DigestOf(State.data(), State.size()));

		Out.insert(Out.end(), State.begin(), State.end());

		PutU64(Out, DigestOf(Out.data() + Start, Out.size() - Start));
		return CheckpointResult::Ok;
	}

	CheckpointRefusal ReadCheckpoint(const uint8* Bytes, usize Size, CheckpointView& Out)
	{
		CheckpointRefusal Refusal;
		const auto Refuse = [&Refusal](CheckpointResult Why, uint32 Which = 0)
		{
			Refusal.Result = Why;
			Refusal.Section = Which;
			return Refusal;
		};

		Out = CheckpointView{};
		if (Bytes == nullptr || Size < HeaderBytes + TrailerBytes)
		{
			return Refuse(CheckpointResult::Truncated);
		}
		// The trailer first, so that a container which disagrees with itself is
		// turned away before anything in it is believed.
		if (DigestOf(Bytes, Size - TrailerBytes) != GetU64(Bytes + Size - TrailerBytes))
		{
			return Refuse(CheckpointResult::Corrupt);
		}
		if (std::memcmp(Bytes, CheckpointMagic, MagicBytes) != 0)
		{
			return Refuse(CheckpointResult::BadMagic);
		}

		const uint8* At = Bytes + MagicBytes;
		Out.Version = GetU32(At);
		Out.Flags = GetU32(At + 4);
		Out.InnerFormat = GetU32(At + 8);
		Out.Seed = GetU64(At + 12);
		Out.Tick = GetU64(At + 20);
		Out.LogEvents = GetU64(At + 28);
		Out.LogBytes = GetU64(At + 36);
		const uint32 SectionCount = GetU32(At + 44);

		if (Out.Version != CheckpointVersion)
		{
			return Refuse(CheckpointResult::VersionMismatch);
		}
		if (Out.InnerFormat != VAELEN_SAVE_FORMAT_VERSION)
		{
			return Refuse(CheckpointResult::InnerVersionMismatch);
		}

		// THE MUST-UNDERSTAND HALF. A set bit this build does not know is a
		// refusal that NAMES THE BIT, because "this file is from a newer
		// version" is not an answer anybody can act on.
		const uint16 Required = static_cast<uint16>(Out.Flags & 0xFFFFu);
		const uint16 Unknown = static_cast<uint16>(Required & ~KnownRequiredFlags);
		if (Unknown != 0u)
		{
			for (uint32 Bit = 0; Bit < 16u; ++Bit)
			{
				if ((Unknown & (1u << Bit)) != 0u)
				{
					Refusal.Result = CheckpointResult::UnknownRequiredFlag;
					Refusal.UnknownBit = Bit;
					return Refusal;
				}
			}
		}

		const usize TableBytes = static_cast<usize>(SectionCount) * EntryBytes;
		if (SectionCount > 64u || HeaderBytes + TableBytes + TrailerBytes > Size)
		{
			return Refuse(CheckpointResult::BadSectionTable, SectionCount);
		}

		const uint64 PayloadFloor = static_cast<uint64>(HeaderBytes + TableBytes);
		const uint64 PayloadCeil = static_cast<uint64>(Size - TrailerBytes);
		uint64 Reach = PayloadFloor;
		Out.Sections.reserve(SectionCount);
		for (uint32 Index = 0; Index < SectionCount; ++Index)
		{
			const uint8* Row = Bytes + HeaderBytes + static_cast<usize>(Index) * EntryBytes;
			SectionEntry Entry;
			Entry.Kind = GetU16(Row);
			Entry.Flags = GetU32(Row + 2);
			Entry.Offset = GetU64(Row + 6);
			Entry.Length = GetU64(Row + 14);
			Entry.Digest = GetU64(Row + 22);

			// A table that does not describe these bytes. Sections are required
			// to be in order and not to overlap: an overlapping table is how a
			// section is made to be read twice with two meanings.
			if (Entry.Offset < Reach || Entry.Length > PayloadCeil - Entry.Offset)
			{
				return Refuse(CheckpointResult::BadSectionTable, Index);
			}
			// PER-SECTION DIGESTS, and this is the control the phase asked for:
			// a flipped byte anywhere inside a section is caught HERE, by the
			// section, and not only by the container trailer - so a reader that
			// verifies one section and uses it can trust it before the rest of
			// the file has been looked at.
			if (DigestOf(Bytes + Entry.Offset, static_cast<usize>(Entry.Length)) != Entry.Digest)
			{
				return Refuse(CheckpointResult::Corrupt, Index);
			}
			Reach = Entry.Offset + Entry.Length;
			Out.Sections.push_back(Entry);
		}
		if (Reach != PayloadCeil)
		{
			// Bytes inside the container that no section claims. Refused rather
			// than ignored: unclaimed bytes are where a second meaning hides.
			return Refuse(CheckpointResult::BadSectionTable, SectionCount);
		}

		Out.Base = Bytes;
		return Refusal;
	}
} // namespace Vaelen::Run
