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
#include <string>
#include <string_view>

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

		/// The RUN section's payload. Its shape is deliberately boring - the
		/// interesting decision is that it exists at all.
		void PutRunState(std::vector<uint8>& Out, const Aelvor::RunState& R)
		{
			Out.push_back(R.Begun ? uint8{1} : uint8{0});
			PutU32(Out, R.Detail);
			PutU32(Out, R.Dug);
			PutU32(Out, R.Eyes.Region);
			PutU32(Out, R.Eyes.Reach);
			PutU32(Out, R.Eyes.Most);
			PutU32(Out, static_cast<uint32>(R.Near.size()));
			for (const uint16 Region : R.Near)
			{
				PutU16(Out, Region);
			}
			PutU32(Out, static_cast<uint32>(R.Watched.size()));
			for (const uint16 Region : R.Watched)
			{
				PutU16(Out, Region);
			}
		}

		/// False when the bytes do not describe a RunState - which, because the
		/// section digest has already agreed with them, means the section is
		/// from a build that wrote a different shape.
		bool GetRunState(const uint8* At, usize Size, Aelvor::RunState& Out)
		{
			usize Need = 1u + 4u * 6u;
			if (Size < Need)
			{
				return false;
			}
			Out.Begun = At[0] != 0u;
			Out.Detail = GetU32(At + 1);
			Out.Dug = GetU32(At + 5);
			Out.Eyes.Region = GetU32(At + 9);
			Out.Eyes.Reach = GetU32(At + 13);
			Out.Eyes.Most = GetU32(At + 17);

			usize Cursor = 21u;
			const auto Read = [&](std::vector<uint16>& Into)
			{
				if (Size - Cursor < 4u)
				{
					return false;
				}
				const uint32 Count = GetU32(At + Cursor);
				Cursor += 4u;
				if (Count > (Size - Cursor) / 2u)
				{
					return false;
				}
				Into.clear();
				Into.reserve(Count);
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					Into.push_back(GetU16(At + Cursor));
					Cursor += 2u;
				}
				return true;
			};
			if (!Read(Out.Near) || !Read(Out.Watched))
			{
				return false;
			}
			// Trailing bytes inside a section are the same fault as trailing
			// bytes inside the container: something wrote a meaning this build
			// does not read.
			return Cursor == Size;
		}

		/// The HOST section: the world the host DECLARED, so a reader can refuse
		/// a checkpoint of another one instead of loading it and leaving the
		/// host wrong about what it is holding.
		void PutOptions(std::vector<uint8>& Out, const Options& O)
		{
			PutU32(Out, O.Size);
			PutU32(Out, O.PreHistory);
			PutU32(Out, O.Years);
			PutU64(Out, O.Seed);
			// One byte per flag rather than a packed bitfield: a bit that moves
			// when somebody inserts a flag in the middle is a save that reads as
			// a different world, silently, which is the exact defect 16.10 is
			// about.
			Out.push_back(O.Colony ? uint8{1} : uint8{0});
			Out.push_back(O.Play ? uint8{1} : uint8{0});
			Out.push_back(O.Lively ? uint8{1} : uint8{0});
			Out.push_back(O.Stream ? uint8{1} : uint8{0});
		}

		bool GetOptions(const uint8* At, usize Size, Options& Out)
		{
			if (At == nullptr || Size != 4u + 4u + 4u + 8u + 4u)
			{
				return false;
			}
			Out.Size = GetU32(At);
			Out.PreHistory = GetU32(At + 4);
			Out.Years = GetU32(At + 8);
			Out.Seed = GetU64(At + 12);
			Out.Colony = At[20] != 0u;
			Out.Play = At[21] != 0u;
			Out.Lively = At[22] != 0u;
			Out.Stream = At[23] != 0u;
			return true;
		}

		/// The STREAM section: four StartRules words, then EncodeStream's text.
		void PutStream(std::vector<uint8>& Out, const Player::InputStream& Tape, const Player::StartRules& Rules)
		{
			PutU32(Out, Rules.FromAge);
			PutU32(Out, Rules.ToAge);
			PutU32(Out, Rules.WantBound);
			PutU32(Out, Rules.PreferOre);
			const std::string Text = Player::EncodeStream(Tape);
			Out.insert(Out.end(), Text.begin(), Text.end());
		}

		bool GetStream(const uint8* At, usize Size, Player::InputStream& Tape, Player::StartRules& Rules)
		{
			if (At == nullptr || Size < 16u)
			{
				return false;
			}
			Rules.FromAge = GetU32(At);
			Rules.ToAge = GetU32(At + 4);
			Rules.WantBound = GetU32(At + 8);
			Rules.PreferOre = GetU32(At + 12);
			const std::string_view Text(reinterpret_cast<const char*>(At + 16), Size - 16u);
			Player::StreamReport Report;
			// THE SAME DECODER THE .stream FILES GO THROUGH. A second one could
			// disagree with it about the same walk, and the Phase 14 and 15
			// gates pin this one.
			return Player::DecodeStream(Text, Tape, Report);
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

	const char* MigrateResultToString(MigrateResult Result) noexcept
	{
		switch (Result)
		{
		case MigrateResult::Ok:
			return "Ok";
		case MigrateResult::NothingToDo:
			return "NothingToDo";
		case MigrateResult::FromTheFuture:
			return "FromTheFuture";
		case MigrateResult::NoUpgrader:
			return "NoUpgrader";
		case MigrateResult::StepFailed:
			return "StepFailed";
		}
		return "Unknown";
	}

	UpgradePath BuiltInUpgrades() noexcept
	{
		// EMPTY, and the emptiness is the honest state of this project rather
		// than a gap. VAELEN_SAVE_FORMAT_VERSION is 3, CheckpointVersion is 1,
		// and nothing has shipped - there is no save anywhere that is older
		// than the one this build writes. The first entry here goes in beside
		// the change that bumps a version, which is the only moment anyone
		// knows what it has to do.
		return UpgradePath{};
	}

	MigrateReport Migrate(std::vector<uint8>& Bytes, uint32 From, uint32 To, const UpgradePath& Path)
	{
		MigrateReport Report;
		Report.At = From;
		if (From == To)
		{
			Report.Result = MigrateResult::NothingToDo;
			return Report;
		}
		if (From > To)
		{
			// A CONTAINER FROM THE FUTURE IS NEVER TOUCHED. An older build
			// cannot know what a newer one meant, and a guess here would be a
			// guess written back over the player's only copy.
			Report.Result = MigrateResult::FromTheFuture;
			return Report;
		}

		// The work happens on a COPY and replaces Bytes only on success, so a
		// chain that fails four steps in leaves nothing half-done. Same rule as
		// 16.02's writer and 16.03's loader, for the same reason.
		std::vector<uint8> Working = Bytes;
		for (uint32 Version = From; Version < To; ++Version)
		{
			const Upgrade* Found = nullptr;
			for (usize Index = 0; Index < Path.Count; ++Index)
			{
				if (Path.Steps[Index].From == Version && Path.Steps[Index].Step != nullptr)
				{
					Found = &Path.Steps[Index];
					break;
				}
			}
			if (Found == nullptr)
			{
				// STOPS AT THE GAP AND NAMES IT, rather than skipping to the
				// next step it does have. Each upgrader was written knowing
				// only what the one before it produced; running step N+1 over
				// bytes step N never saw is how a migration corrupts quietly.
				Report.Result = MigrateResult::NoUpgrader;
				Report.At = Version;
				return Report;
			}
			if (!Found->Step(Working))
			{
				Report.Result = MigrateResult::StepFailed;
				Report.At = Version;
				return Report;
			}
			++Report.Ran;
		}

		Bytes.swap(Working);
		Report.Result = MigrateResult::Ok;
		Report.At = To;
		return Report;
	}

	bool ReadStreamSection(const CheckpointView& View, Player::InputStream& Tape, Player::StartRules& Rules)
	{
		uint64 Length = 0;
		const uint8* At = View.Find(SectionKind::Stream, Length);
		return At != nullptr && GetStream(At, static_cast<usize>(Length), Tape, Rules);
	}

	bool ReadHostSection(const CheckpointView& View, Options& Out)
	{
		uint64 Length = 0;
		const uint8* At = View.Find(SectionKind::Host, Length);
		return At != nullptr && GetOptions(At, static_cast<usize>(Length), Out);
	}

	bool ReadRunSection(const CheckpointView& View, Aelvor::RunState& Out)
	{
		uint64 Length = 0;
		const uint8* At = View.Find(SectionKind::Run, Length);
		if (At == nullptr)
		{
			return false;
		}
		return GetRunState(At, static_cast<usize>(Length), Out);
	}

	CheckpointResult BuildCheckpoint(const Aelvor& Run, std::vector<uint8>& Out, uint16 MayIgnore)
	{
		return BuildCheckpoint(Run, Player::InputStream{}, Player::StartRules{}, Out, MayIgnore);
	}

	CheckpointResult BuildCheckpoint(const Aelvor& Run, const Player::InputStream& Tape,
									 const Player::StartRules& Rules, std::vector<uint8>& Out, uint16 MayIgnore)
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

		// 16.05 adds the RUN section beside STATE. HOST and STREAM are declared
		// in the header and filled by 16.07; the table is variable-length by
		// design, so this cost no container version - which is what the table
		// was for.
		std::vector<uint8> RunBytes;
		PutRunState(RunBytes, Run.GetRunState());

		// 16.10 adds HOST. It cost NO container version, because 16.04 made the
		// section table variable-length for exactly this - which is the first
		// time that decision has paid rather than merely been defensible.
		std::vector<uint8> HostBytes;
		PutOptions(HostBytes, Run.Given());

		// 16.11: the tape this world was played on. Written even when empty, so
		// that "this save carries no tape" and "this save is from a build that
		// did not carry tapes" are different states rather than one absence.
		std::vector<uint8> StreamBytes;
		PutStream(StreamBytes, Tape, Rules);

		const uint32 SectionCount = 4u;
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

		PutU16(Out, static_cast<uint16>(SectionKind::Run));
		PutU32(Out, 0u);
		PutU64(Out, PayloadAt + static_cast<uint64>(State.size()));
		PutU64(Out, static_cast<uint64>(RunBytes.size()));
		PutU64(Out, DigestOf(RunBytes.data(), RunBytes.size()));

		const uint64 HostAt = PayloadAt + static_cast<uint64>(State.size()) + static_cast<uint64>(RunBytes.size());
		PutU16(Out, static_cast<uint16>(SectionKind::Host));
		PutU32(Out, 0u);
		PutU64(Out, HostAt);
		PutU64(Out, static_cast<uint64>(HostBytes.size()));
		PutU64(Out, DigestOf(HostBytes.data(), HostBytes.size()));

		PutU16(Out, static_cast<uint16>(SectionKind::Stream));
		PutU32(Out, 0u);
		PutU64(Out, HostAt + static_cast<uint64>(HostBytes.size()));
		PutU64(Out, static_cast<uint64>(StreamBytes.size()));
		PutU64(Out, DigestOf(StreamBytes.data(), StreamBytes.size()));

		Out.insert(Out.end(), State.begin(), State.end());
		Out.insert(Out.end(), RunBytes.begin(), RunBytes.end());
		Out.insert(Out.end(), HostBytes.begin(), HostBytes.end());
		Out.insert(Out.end(), StreamBytes.begin(), StreamBytes.end());

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
