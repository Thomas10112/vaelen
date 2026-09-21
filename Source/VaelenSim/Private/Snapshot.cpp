// VAELEN - VaelenSim
// Snapshot save/load through one symmetric serialisation routine.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_Snapshot.cpp
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Version.h"
#include "Vaelen/Sim/Archive.h"
#include "Vaelen/Sim/World.h"

#include <cstring>

namespace Vaelen
{
	namespace
	{
		constexpr char Magic[8] = {'V', 'A', 'E', 'L', 'E', 'N', 'S', 'N'};

		void SerializeRules(IArchive& Ar, CalendarRules& Rules) noexcept
		{
			Ar << Rules.TicksPerHour << Rules.HoursPerDay << Rules.DaysPerMonth << Rules.MonthsPerYear
			   << Rules.MonthsPerSeason;
		}

		void SerializeStream(IArchive& Ar, RandomStreamState& State) noexcept
		{
			Ar << State.Seed << State.S[0] << State.S[1] << State.S[2] << State.S[3] << State.DrawCount;
		}

		SnapshotResult SerializeBody(IArchive& Ar, World& W)
		{
			// Clock.
			SimTick Tick = W.Clock().Now();
			CalendarRules Rules = W.Clock().GetRules();
			Ar << Tick;
			SerializeRules(Ar, Rules);
			if (Ar.IsLoading())
			{
				if (Ar.HasError())
				{
					return SnapshotResult::Truncated;
				}
				if (!Rules.IsValid())
				{
					return SnapshotResult::Inconsistent;
				}
				W.Clock() = SimClock(Tick, Rules);
			}

			// Root random stream.
			RandomStreamState Stream = W.RootStream().GetState();
			SerializeStream(Ar, Stream);
			if (Ar.IsLoading())
			{
				if (Ar.HasError())
				{
					return SnapshotResult::Truncated;
				}
				W.RootStream().SetState(Stream);
			}

			// Id allocator.
			IdAllocator::State Ids = W.Ids().GetState();
			for (uint64& Next : Ids.NextSerial)
			{
				Ar << Next;
			}
			if (Ar.IsLoading())
			{
				if (Ar.HasError())
				{
					return SnapshotResult::Truncated;
				}
				W.Ids().SetState(Ids);
			}

			// Entities. Slots are written field by field: the Slot struct has
			// padding and raw bytes would not be deterministic.
			{
				EntityRegistry::State State = W.Entities().GetState();
				uint32 FreeHead = State.FreeHead;
				uint32 AliveCount = State.AliveCount;
				uint64 SlotCount = State.Slots.size();
				Ar << FreeHead << AliveCount << SlotCount;
				if (Ar.IsLoading())
				{
					if (Ar.HasError() || SlotCount > EntityHandle::MaxIndex + uint64{1} ||
						SlotCount > Ar.RemainingBytes() / 17u)
					{
						return SnapshotResult::Truncated;
					}
					State.Slots.assign(static_cast<usize>(SlotCount), EntityRegistry::Slot{});
					State.FreeHead = FreeHead;
					State.AliveCount = AliveCount;
				}
				for (EntityRegistry::Slot& S : State.Slots)
				{
					uint64 Id = S.Id.Value;
					uint8 Flags = static_cast<uint8>((S.Alive ? 1u : 0u) | (S.Retired ? 2u : 0u));
					Ar << Id << S.Generation << S.NextFree << Flags;
					if (Ar.IsLoading())
					{
						if (Flags > 3)
						{
							return SnapshotResult::Corrupt;
						}
						S.Id = PersistentId(Id);
						S.Alive = (Flags & 1u) != 0;
						S.Retired = (Flags & 2u) != 0;
					}
				}
				if (Ar.IsLoading())
				{
					if (Ar.HasError())
					{
						return SnapshotResult::Truncated;
					}
					if (!W.Entities().SetState(State))
					{
						return SnapshotResult::Inconsistent;
					}
				}
			}

			// Component pools, in type-id order.
			{
				uint32 PoolCount = W.Components().PoolCount();
				Ar << PoolCount;
				if (Ar.IsLoading())
				{
					if (Ar.HasError())
					{
						return SnapshotResult::Truncated;
					}
					if (PoolCount != W.Components().PoolCount())
					{
						return SnapshotResult::MissingPool;
					}
				}
				const uint32 TypeCount = W.Types().Count();
				uint32 Written = 0;
				for (uint32 Id = 0; Id < TypeCount && Written < PoolCount; ++Id)
				{
					IComponentPool* Pool = W.Components().GetPoolBase(static_cast<ComponentTypeId>(Id));
					if (Pool == nullptr)
					{
						continue;
					}
					uint16 TypeId = static_cast<uint16>(Id);
					Hash64 NameHash = W.Types().GetInfo(TypeId).NameHash;
					uint32 ElementSize = Pool->ElementSize();
					Ar << TypeId << NameHash << ElementSize;
					if (Ar.IsLoading())
					{
						if (Ar.HasError())
						{
							return SnapshotResult::Truncated;
						}
						if (TypeId != Id || NameHash != W.Types().GetInfo(TypeId).NameHash ||
							ElementSize != Pool->ElementSize())
						{
							return SnapshotResult::LayoutMismatch;
						}
					}
					if (!Pool->Serialize(Ar))
					{
						return Ar.HasError() ? SnapshotResult::Truncated : SnapshotResult::Inconsistent;
					}
					++Written;
				}
				if (Written != PoolCount)
				{
					return SnapshotResult::MissingPool;
				}
			}

			// World map (format version 2).
			if (!W.Map().Serialize(Ar))
			{
				return Ar.HasError() ? SnapshotResult::Truncated : SnapshotResult::Inconsistent;
			}

			// Pending events and the log.
			{
				// SaveSnapshot refuses a dispatching world before it writes a
				// byte (16.02); LoadSnapshot replaces the pending list wholesale
				// and a dispatching target is the caller's error either way.
				if (W.Events().IsDispatching())
				{
					return SnapshotResult::Inconsistent;
				}
				std::vector<Event> Pending = W.Events().GetPending();
				if (!SerializeVector(Ar, Pending))
				{
					return SnapshotResult::Truncated;
				}
				if (Ar.IsLoading())
				{
					W.Events().SetPending(Pending);
				}

				std::vector<uint8> LogBytes;
				if (Ar.IsSaving())
				{
					W.Log().WriteTo(LogBytes);
				}
				if (!SerializeVector(Ar, LogBytes, uint64{1} << 40))
				{
					return SnapshotResult::Truncated;
				}
				if (Ar.IsLoading() && !W.Log().ReadFrom(LogBytes.data(), LogBytes.size()))
				{
					return SnapshotResult::Corrupt;
				}
			}
			return SnapshotResult::Ok;
		}
	} // namespace

	const char* SnapshotResultToString(SnapshotResult Result) noexcept
	{
		switch (Result)
		{
		case SnapshotResult::Ok:
			return "Ok";
		case SnapshotResult::BadMagic:
			return "BadMagic";
		case SnapshotResult::VersionMismatch:
			return "VersionMismatch";
		case SnapshotResult::LayoutMismatch:
			return "LayoutMismatch";
		case SnapshotResult::MissingPool:
			return "MissingPool";
		case SnapshotResult::Truncated:
			return "Truncated";
		case SnapshotResult::Corrupt:
			return "Corrupt";
		case SnapshotResult::Inconsistent:
			return "Inconsistent";
		}
		return "Unknown";
	}

	SnapshotResult SaveSnapshot(const World& Source, std::vector<uint8>& Out)
	{
		const usize Start = Out.size();

		// EVERY REFUSAL BELOW LEAVES Out AS IT WAS FOUND. A caller's buffer is
		// its own; a function that half-filled it and then said no would make
		// the caller's next decision - append, write, retry - a decision about
		// bytes nobody meant. Phase 16 task 16.02.
		const auto Refuse = [&Out, Start](SnapshotResult Why)
		{
			Out.resize(Start);
			return Why;
		};

		// The world must not be mid-dispatch. This was a VAELEN_CHECKF, which
		// Assert.h compiles to ((void)0) under NDEBUG and under
		// UE_BUILD_SHIPPING - so in a player's build it was no check at all and
		// the pending list would have been read while it was being drained.
		if (Source.Events().IsDispatching())
		{
			return Refuse(SnapshotResult::Inconsistent);
		}

		MemoryWriter Ar(Out);
		char MagicBytes[8];
		std::memcpy(MagicBytes, Magic, 8);
		Ar.SerializeBytes(MagicBytes, 8);
		uint32 Version = VAELEN_SAVE_FORMAT_VERSION;
		uint32 Flags = 0;
		Hash64 Layout = HashCombine(Source.Types().LayoutDigest(), Source.Map().LayoutDigest());
		uint64 Seed = Source.Config().Seed;
		Ar << Version << Flags << Layout << Seed;
		// The body routine is symmetric and takes a mutable world; saving does
		// not modify it (every write path only reads).
		World& Mutable = const_cast<World&>(Source);
		const SnapshotResult Body = SerializeBody(Ar, Mutable);

		// THE DEFECT THIS FUNCTION SHIPPED WITH, AND WHY IT WAS INVISIBLE.
		// The line below used to read:
		//
		//     [[maybe_unused]] const SnapshotResult Body = SerializeBody(...);
		//     VAELEN_CHECKF(Body == SnapshotResult::Ok, "...");
		//
		// and the function returned void. With asserts on, a failing body
		// stopped the process and a developer saw it. With asserts OFF - which
		// Assert.h:33-43 means for NDEBUG and for UE_BUILD_SHIPPING and
		// UE_BUILD_TEST, that is, every build a player ever runs - the macro is
		// ((void)0), execution fell through, and the two lines after it hashed
		// the SHORT bytes and appended a trailer over them. The file was then
		// well-formed, wrong, and validated on load, because the trailer agreed
		// with the truncated body it was computed from.
		//
		// A caller could not have noticed: there was no return value, and the
		// image carries no count of what it ought to contain. Phase 16's
		// planning found it by reading; Snapshot.SaveRefusesRatherThanLies
		// measures it, built with NDEBUG on purpose.
		if (Body != SnapshotResult::Ok)
		{
			return Refuse(Body);
		}

		Hash64 Digest = HashBytes(reinterpret_cast<const char*>(Out.data() + Start), Out.size() - Start);
		Ar << Digest;
		// The writer itself can run out of room, and it says so through its own
		// error flag rather than through the body's result.
		if (Ar.HasError())
		{
			return Refuse(SnapshotResult::Truncated);
		}
		return SnapshotResult::Ok;
	}

	SnapshotResult LoadSnapshot(World& Target, const uint8* Bytes, usize Size)
	{
		if (Bytes == nullptr || Size < 8 + 4 + 4 + 8 + 8 + 8)
		{
			return SnapshotResult::Truncated;
		}
		// Trailer digest first: a corrupt image is rejected before any state changes.
		Hash64 Expected = 0;
		std::memcpy(&Expected, Bytes + Size - 8, 8);
		if (HashBytes(reinterpret_cast<const char*>(Bytes), Size - 8) != Expected)
		{
			return SnapshotResult::Corrupt;
		}
		MemoryReader Ar(Bytes, Size - 8);
		char MagicBytes[8] = {};
		Ar.SerializeBytes(MagicBytes, 8);
		if (std::memcmp(MagicBytes, Magic, 8) != 0)
		{
			return SnapshotResult::BadMagic;
		}
		uint32 Version = 0;
		uint32 Flags = 0;
		Hash64 Layout = 0;
		uint64 Seed = 0;
		Ar << Version << Flags << Layout << Seed;
		if (Version != VAELEN_SAVE_FORMAT_VERSION)
		{
			return SnapshotResult::VersionMismatch;
		}
		if (Layout != HashCombine(Target.Types().LayoutDigest(), Target.Map().LayoutDigest()))
		{
			return SnapshotResult::LayoutMismatch;
		}
		if (Seed != Target.Config().Seed)
		{
			// The seed is part of the world's identity; derived streams depend on it.
			return SnapshotResult::LayoutMismatch;
		}
		const SnapshotResult Body = SerializeBody(Ar, Target);
		if (Body != SnapshotResult::Ok)
		{
			return Body;
		}
		if (!Ar.AtEnd())
		{
			return SnapshotResult::Corrupt;
		}
		return SnapshotResult::Ok;
	}

	Hash64 ComputeStateDigest(const World& Source)
	{
		std::vector<uint8> Bytes;
		// 16.02 gave SaveSnapshot the power to refuse, and a refusal truncates
		// the buffer back to the size it was handed - here, to empty. Reading a
		// trailer out of THAT is what the line below used to do unconditionally:
		// on an empty vector data() may be null, and null + 0 - 8 is undefined
		// before the memcpy is ever reached. Making the save fallible therefore
		// made this call site worse, not better, and it is the fix's job to say
		// so rather than the next crash report's.
		//
		// A refused save has no digest and zero is the value that says so. It is
		// not a world's digest by construction: a real trailer is a hash over at
		// least the forty bytes of header that always precede it.
		if (SaveSnapshot(Source, Bytes) != SnapshotResult::Ok || Bytes.size() < 8)
		{
			return 0;
		}
		Hash64 Digest = 0;
		std::memcpy(&Digest, Bytes.data() + Bytes.size() - 8, 8);
		return Digest;
	}
} // namespace Vaelen
