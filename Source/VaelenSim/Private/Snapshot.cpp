// VAELEN - VaelenSim
// Snapshot save/load through one symmetric serialisation routine.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_Snapshot.cpp
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Version.h"
#include "Vaelen/Sim/Archive.h"
#include "Vaelen/Sim/World.h"

#include <cstdio>
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

		/// WHERE THE PARTICULARS GO. SerializeBody is one long function with a
		/// dozen exits and it returns a bare enum to ninety call sites; giving
		/// every one of them an out-parameter to thread would be a change far
		/// larger than the defect. This holds the last refusal's detail, and
		/// DiagnoseLoad reads it immediately after the call that produced it.
		/// It is only ever written on a refusal and only ever read once.
		thread_local SnapshotDiagnosis LastRefusal;

		void Diagnose(SnapshotResult Why, const char* Type, uint32 ImageSize, uint32 WorldSize) noexcept
		{
			LastRefusal = SnapshotDiagnosis{};
			LastRefusal.Result = Why;
			LastRefusal.Type = Type;
			LastRefusal.ImageSize = ImageSize;
			LastRefusal.WorldSize = WorldSize;
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
				if (Ar.IsLoading() && Ar.HasError())
				{
					return SnapshotResult::Truncated;
				}

				if (Ar.IsLoading())
				{
					// LOADING IS DRIVEN BY THE IMAGE'S RECORDS, not by this
					// world's type ids. That is the whole of "reconcile by
					// name": the image says which types it holds, and each one
					// is looked up in this registry by NameHash. A type that
					// moved to another id loads into the right pool; a type that
					// is not here at all is named as missing rather than
					// reported as a count that did not match.
					//
					// The old code walked the WORLD's types and demanded
					// TypeId == Id, so any reordering refused a readable save,
					// and an eager PoolCount comparison answered before anything
					// could say WHICH type was involved.
					for (uint32 Record = 0; Record < PoolCount; ++Record)
					{
						uint16 TypeId = 0;
						Hash64 NameHash = 0;
						uint32 ElementSize = 0;
						Ar << TypeId << NameHash << ElementSize;
						if (Ar.HasError())
						{
							return SnapshotResult::Truncated;
						}

						const uint32 Known = W.Types().Count();
						uint32 Found = Known;
						for (uint32 Id = 0; Id < Known; ++Id)
						{
							if (W.Types().GetInfo(static_cast<uint16>(Id)).NameHash == NameHash)
							{
								Found = Id;
								break;
							}
						}
						if (Found == Known)
						{
							// The image holds a type this world has never heard
							// of. From the IMAGE's side a type was added.
							Diagnose(SnapshotResult::TypeAdded, nullptr, ElementSize, 0u);
							return SnapshotResult::TypeAdded;
						}
						IComponentPool* Into = W.Components().GetPoolBase(static_cast<ComponentTypeId>(Found));
						if (Into == nullptr)
						{
							// REGISTERED BUT WITH NO STORE, which is a different
							// fault from "no such type" and wants a different
							// fix from whoever wired this world.
							Diagnose(SnapshotResult::PoolMissing, W.Types().GetInfo(static_cast<uint16>(Found)).Name,
									 ElementSize, 0u);
							return SnapshotResult::PoolMissing;
						}
						if (ElementSize != Into->ElementSize())
						{
							Diagnose(SnapshotResult::TypeResized, W.Types().GetInfo(static_cast<uint16>(Found)).Name,
									 ElementSize, Into->ElementSize());
							return SnapshotResult::TypeResized;
						}
						if (!Into->Serialize(Ar))
						{
							return Ar.HasError() ? SnapshotResult::Truncated : SnapshotResult::Inconsistent;
						}
					}
					// A type THIS world has that the image does not. Counted
					// rather than searched: the image's records have all been
					// consumed, so any surplus here is this build's.
					if (W.Components().PoolCount() > PoolCount)
					{
						Diagnose(SnapshotResult::TypeRemoved, nullptr, PoolCount, W.Components().PoolCount());
						return SnapshotResult::TypeRemoved;
					}
				}
				else
				{
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
						if (!Pool->Serialize(Ar))
						{
							return SnapshotResult::Inconsistent;
						}
						++Written;
					}
				}
			}

			// World map (format version 2). A map that will not take the
			// image's own layers IS the world being shaped differently, and
			// that is where WorldShapeDiffers belongs - not on a folded digest
			// that cannot tell one difference from another.
			if (!W.Map().Serialize(Ar))
			{
				return Ar.HasError() ? SnapshotResult::Truncated : SnapshotResult::WorldShapeDiffers;
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
		case SnapshotResult::NotASave:
			return "NotASave";
		case SnapshotResult::FormatTooNew:
			return "FormatTooNew";
		case SnapshotResult::FormatTooOld:
			return "FormatTooOld";
		case SnapshotResult::UnknownRequiredFlag:
			return "UnknownRequiredFlag";
		case SnapshotResult::SeedMismatch:
			return "SeedMismatch";
		case SnapshotResult::WorldShapeDiffers:
			return "WorldShapeDiffers";
		case SnapshotResult::TypeAdded:
			return "TypeAdded";
		case SnapshotResult::TypeRemoved:
			return "TypeRemoved";
		case SnapshotResult::TypeResized:
			return "TypeResized";
		case SnapshotResult::TypeRenamed:
			return "TypeRenamed";
		case SnapshotResult::TypesReordered:
			return "TypesReordered";
		case SnapshotResult::PoolMissing:
			return "PoolMissing";
		case SnapshotResult::Truncated:
			return "Truncated";
		case SnapshotResult::Corrupt:
			return "Corrupt";
		case SnapshotResult::Inconsistent:
			return "Inconsistent";
		case SnapshotResult::RollbackFailed:
			return "RollbackFailed";
		}
		return "Unknown";
	}

	usize SnapshotDiagnosis::Describe(char* Out, usize Room) const noexcept
	{
		if (Out == nullptr || Room == 0)
		{
			return 0;
		}
		const char* Named = Type != nullptr ? Type : "a component type";
		int Wrote = 0;
		switch (Result)
		{
		case SnapshotResult::Ok:
			Wrote = std::snprintf(Out, Room, "the save loaded");
			break;
		case SnapshotResult::NotASave:
			Wrote = std::snprintf(Out, Room, "this file is not a VAELEN save");
			break;
		case SnapshotResult::FormatTooNew:
			Wrote = std::snprintf(Out, Room,
								  "this save is from a newer version of the game (save format %u, this build reads %u)",
								  ImageFormat, WorldFormat);
			break;
		case SnapshotResult::FormatTooOld:
			Wrote = std::snprintf(
				Out, Room, "this save is from an older version of the game (save format %u, this build reads %u)",
				ImageFormat, WorldFormat);
			break;
		case SnapshotResult::UnknownRequiredFlag:
			Wrote = std::snprintf(Out, Room, "this save uses something this build does not understand");
			break;
		case SnapshotResult::SeedMismatch:
			Wrote = std::snprintf(Out, Room, "this save is of a different world");
			break;
		case SnapshotResult::WorldShapeDiffers:
			Wrote = std::snprintf(Out, Room, "this save's world is not shaped like this one");
			break;
		case SnapshotResult::TypeAdded:
			Wrote = std::snprintf(Out, Room, "the save has '%s' and this build does not", Named);
			break;
		case SnapshotResult::TypeRemoved:
			Wrote = std::snprintf(Out, Room, "this build has '%s' and the save does not", Named);
			break;
		case SnapshotResult::TypeResized:
			Wrote = std::snprintf(Out, Room, "'%s' is %u bytes in the save and %u in this build", Named, ImageSize,
								  WorldSize);
			break;
		case SnapshotResult::TypeRenamed:
			Wrote = std::snprintf(Out, Room, "'%s' is under another name in the save", Named);
			break;
		case SnapshotResult::TypesReordered:
			Wrote = std::snprintf(Out, Room, "the save's components are in an order this build cannot resolve");
			break;
		case SnapshotResult::PoolMissing:
			Wrote = std::snprintf(Out, Room, "this build has no store for '%s'", Named);
			break;
		case SnapshotResult::Truncated:
			Wrote = std::snprintf(Out, Room, "this save is incomplete");
			break;
		case SnapshotResult::Corrupt:
			Wrote = std::snprintf(Out, Room, "this save is damaged");
			break;
		case SnapshotResult::Inconsistent:
			Wrote = std::snprintf(Out, Room, "this save does not describe a world that can exist");
			break;
		case SnapshotResult::RollbackFailed:
			Wrote = std::snprintf(
				Out, Room, "the save could not be loaded AND the world it was loaded over could not be put back");
			break;
		}
		return Wrote < 0 ? 0u : static_cast<usize>(Wrote);
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
			Diagnose(SnapshotResult::NotASave, nullptr, 0u, 0u);
			return SnapshotResult::NotASave;
		}
		uint32 Version = 0;
		uint32 Flags = 0;
		Hash64 Layout = 0;
		uint64 Seed = 0;
		Ar << Version << Flags << Layout << Seed;
		if (Version != VAELEN_SAVE_FORMAT_VERSION)
		{
			// WHICH DIRECTION, because the two want opposite things from the
			// person reading the message: update the game, or find an older
			// build. One `VersionMismatch` could say neither.
			const SnapshotResult Which =
				Version > VAELEN_SAVE_FORMAT_VERSION ? SnapshotResult::FormatTooNew : SnapshotResult::FormatTooOld;
			Diagnose(Which, nullptr, 0u, 0u);
			LastRefusal.ImageFormat = Version;
			LastRefusal.WorldFormat = VAELEN_SAVE_FORMAT_VERSION;
			return Which;
		}
		// THE SEED BEFORE THE SHAPE, and that order is the whole point of this
		// task. Both used to answer `LayoutMismatch`, so a player opening
		// somebody else's save was told what a player with a mismatched build
		// was told. The seed is the world's IDENTITY: if it differs, this is
		// not that game, and nothing about the type registry is worth saying.
		if (Seed != Target.Config().Seed)
		{
			Diagnose(SnapshotResult::SeedMismatch, nullptr, 0u, 0u);
			return SnapshotResult::SeedMismatch;
		}
		// THE LAYOUT DIGEST IS NO LONGER A REFUSAL, and that is the change that
		// makes the rest of 16.08 worth anything.
		//
		// It is one number folded from every type's name and size. It can say
		// THAT two worlds differ and never WHICH WAY, so refusing here meant
		// every type cause arrived as one word - which is the defect this task
		// exists for. Measured after the first attempt at this task, which left
		// the gate in place: a type added, removed, reordered, resized and
		// renamed all still answered identically. Renaming LayoutMismatch to
		// WorldShapeDiffers had changed the spelling of the problem.
		//
		// So the digest is not consulted. SerializeBody reconciles the pools by
		// name and names the cause it finds, and a difference it CAN resolve -
		// the same types in another order - now loads instead of refusing.
		//
		// This is only safe because of 16.03: the body may write into Target
		// and still refuse, and the rollback puts the world back. The two tasks
		// were planned in this order for a reason that is only visible here.
		//
		// It is still WORTH KNOWING, though, and kept for one job below: the
		// body can only describe the component types, so a difference in the
		// MAP's layers reaches it as an archive that ran out - and answering
		// "this save is incomplete" about a complete save is worse than the one
		// word it replaced. Where the body has nothing better to say and the
		// digests disagree, the digest's one fact is the honest answer.
		const bool LayoutDiffers = Layout != HashCombine(Target.Types().LayoutDigest(), Target.Map().LayoutDigest());
		// EVERYTHING ABOVE THIS LINE READS AND DOES NOT WRITE. A bad trailer,
		// magic, version, layout or seed is refused without one byte of Target
		// being touched, and those are the common refusals - a file from
		// another build, another world, another save format. They stay free.
		//
		// SerializeBody is where the writing starts, and it can still refuse
		// HALFWAY THROUGH. It applies the image section by section - the clock,
		// then the streams, then the ids, then the entities, then each
		// component pool, then the map - committing each to Target before the
		// next is even read. Pools and the map are worse than the rest: they
		// deserialise IN PLACE, so there is no local to throw away. A refusal
		// at the map leaves every pool already overwritten.
		//
		// Measured rather than argued, at four cut points of a resealed image:
		// the load refuses honestly with Truncated every time, and the target
		// is left on a digest that is neither the one it had nor the one in the
		// image. A CHIMERA. That is defect 2 of Phase 16 and this is its fix.
		//
		// The world is kept first, and put back if the body refuses. The
		// keeping uses the same writer the load consumes, so the two halves
		// test each other by construction, and it costs nothing on the refusals
		// above. Measured: 1.3 MB in 8.7 ms, restored exactly.
		std::vector<uint8> Rollback;
		const SnapshotResult Kept = SaveSnapshot(Target, Rollback);
		if (Kept != SnapshotResult::Ok)
		{
			// A world that cannot be saved cannot be put back, so it must not
			// be overwritten either. Refusing here is what keeps the promise
			// that every refusal leaves the target alone.
			return Kept;
		}

		SnapshotResult Body = SerializeBody(Ar, Target);
		if (Body == SnapshotResult::Truncated && LayoutDiffers)
		{
			// The image is not short; this world is shaped differently, and the
			// body had no way to say so. Measured: a world missing one map
			// LAYER reached here as Truncated.
			Body = SnapshotResult::WorldShapeDiffers;
			Diagnose(SnapshotResult::WorldShapeDiffers, nullptr, 0u, 0u);
		}
		const bool Unspent = Body == SnapshotResult::Ok && !Ar.AtEnd();
		if (Body != SnapshotResult::Ok || Unspent)
		{
			// Put back the bytes this build wrote from this world a moment ago.
			// Not through LoadSnapshot: that would take a rollback of the
			// half-written world, which is the thing being discarded.
			MemoryReader Undo(Rollback.data(), Rollback.size() - 8);
			char UndoMagic[8] = {};
			Undo.SerializeBytes(UndoMagic, 8);
			uint32 UndoVersion = 0;
			uint32 UndoFlags = 0;
			Hash64 UndoLayout = 0;
			uint64 UndoSeed = 0;
			Undo << UndoVersion << UndoFlags << UndoLayout << UndoSeed;
			if (SerializeBody(Undo, Target) != SnapshotResult::Ok)
			{
				// Nothing in this file can rescue the world now, and saying
				// Truncated would tell the caller their game is fine when it
				// is not. This is the one result that means otherwise.
				return SnapshotResult::RollbackFailed;
			}
			return Unspent ? SnapshotResult::Corrupt : Body;
		}
		return SnapshotResult::Ok;
	}

	SnapshotDiagnosis DiagnoseLoad(World& Target, const uint8* Bytes, usize Size)
	{
		// The bare result is what ninety call sites ask for; this is the same
		// call answering at more length. LastRefusal is written by whichever
		// exit produced the result and read here, immediately, before anything
		// else can load.
		Diagnose(SnapshotResult::Ok, nullptr, 0u, 0u);
		const SnapshotResult R = LoadSnapshot(Target, Bytes, Size);
		SnapshotDiagnosis Out = LastRefusal;
		if (Out.Result != R)
		{
			// An exit that did not record particulars - Truncated, Corrupt and
			// the rest, which have nothing to name. The result is still the
			// truth; there is simply no type to point at.
			Out = SnapshotDiagnosis{};
			Out.Result = R;
		}
		return Out;
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
