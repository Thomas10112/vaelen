// VAELEN - VaelenSim tests
// Snapshot: round trip of every state block, byte-identical images for
// identical worlds, explicit rejection of wrong version / magic / layout /
// truncation / corruption, empty and large worlds, continuation after restore.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Version.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <cstring>
#include <memory>
#include <vector>

using namespace Vaelen;

namespace
{
	struct Position
	{
		int32 X = 0;
		int32 Y = 0;
	};
	struct Wealth
	{
		uint64 Coins = 0;
	};
	struct Pulse
	{
		uint32 Count = 0;
	};
	constexpr EventType<Pulse> PulseEvent = MakeEventType<Pulse>("Pulse");

	/// A system that touches every state block: components, random draws,
	/// entity creation/destruction, events.
	class Life final : public ISystem
	{
	public:
		Life(ComponentType<Position> InPos, ComponentType<Wealth> InWealth) : Pos(InPos), WealthType(InWealth) {}
		const char* GetName() const noexcept override { return "Life"; }
		void Tick(TickContext& Context) override
		{
			ComponentPool<Position>& Positions = Context.Components->GetPool(Pos);
			ComponentPool<Wealth>& Wealths = Context.Components->GetPool(WealthType);
			Positions.ForEach(
				[&](EntityHandle, Position& P)
				{
					P.X += static_cast<int32>(Context.Random->Below(3)) - 1;
					P.Y += static_cast<int32>(Context.Random->Below(3)) - 1;
				});
			Wealths.ForEach([&](EntityHandle, Wealth& W) { W.Coins += Context.Random->Below(10); });
			if (Context.Random->Chance(0.3))
			{
				const EntityHandle H = Context.Entities->Create(*Ids, IdKind::Entity);
				Positions.Add(H, Position{static_cast<int32>(Context.Tick), 0});
				if (Context.Random->Chance(0.5))
				{
					Wealths.Add(H, Wealth{Context.Tick});
				}
			}
			if (Positions.Size() > 0 && Context.Random->Chance(0.2))
			{
				// Systems hold no state of their own: the victim comes from the pool.
				const EntityHandle H =
					Positions.GetState().Entities[static_cast<usize>(Context.Random->Below(Positions.Size()))];
				Context.Components->RemoveAll(H);
				Context.Entities->Destroy(H);
			}
			if (Context.Events != nullptr && Context.Random->Chance(0.4))
			{
				Context.Events->Publish(Context.Tick, PulseEvent, Pulse{static_cast<uint32>(Context.Tick)});
			}
		}
		ComponentType<Position> Pos;
		ComponentType<Wealth> WealthType;
		IdAllocator* Ids = nullptr;
	};

	class Counter final : public IEventListener
	{
	public:
		const char* GetListenerName() const noexcept override { return "Counter"; }
		void OnEvent(const Event&) override { ++Received; }
		uint32 Received = 0;
	};

	template <typename T>
	bool SamePool(const ComponentPool<T>& L, const ComponentPool<T>& R)
	{
		const typename ComponentPool<T>::State A = L.GetState();
		const typename ComponentPool<T>::State B = R.GetState();
		return A.Entities == B.Entities && A.Data.size() == B.Data.size() &&
			   std::memcmp(A.Data.data(), B.Data.data(), A.Data.size() * sizeof(T)) == 0;
	}

	/// The "setup code": identical on the saving and the loading side.
	struct TestWorld
	{
		explicit TestWorld(uint64 Seed, bool WithWealth = true) : Instance(MakeConfig(Seed))
		{
			PosType = Instance.Types().Register<Position>("Position");
			Instance.Components().CreatePool(PosType);
			if (WithWealth)
			{
				WealthType = Instance.Types().Register<Wealth>("Wealth");
				Instance.Components().CreatePool(WealthType);
			}
			System = std::make_unique<Life>(PosType, WealthType);
			System->Ids = &Instance.Ids();
			Instance.Systems().Add(System.get());
			Instance.Events().Subscribe(PulseEvent.TypeHash, &Listener);
			Instance.Build();
		}
		static WorldConfig MakeConfig(uint64 Seed)
		{
			WorldConfig Config;
			Config.Seed = Seed;
			return Config;
		}
		void Populate(uint32 Count)
		{
			for (uint32 i = 0; i < Count; ++i)
			{
				const EntityHandle H = Instance.CreateEntity(IdKind::Entity);
				Instance.Components().GetPool(PosType).Add(H, Position{static_cast<int32>(i), -static_cast<int32>(i)});
				if (i % 2 == 0 && WealthType.IsValid())
				{
					Instance.Components().GetPool(WealthType).Add(H, Wealth{i * 100});
				}
			}
		}
		std::vector<uint8> Save() const
		{
			std::vector<uint8> Bytes;
			SaveSnapshot(Instance, Bytes);
			return Bytes;
		}

		World Instance;
		ComponentType<Position> PosType;
		ComponentType<Wealth> WealthType;
		std::unique_ptr<Life> System;
		Counter Listener;
	};
} // namespace

VAELEN_TEST(Snapshot, RoundTripRestoresEveryStateBlock)
{
	TestWorld A(1234);
	A.Populate(10);
	A.Instance.TickMany(50);
	// Leave undelivered events in the bus, and a destroyed entity in the registry.
	A.Instance.Events().Publish(A.Instance.Now(), PulseEvent, Pulse{7});
	A.Instance.DestroyEntity(A.Instance.Entities().Find(A.Instance.Entities().GetId(EntityHandle::Make(0, 1))));
	const std::vector<uint8> Image = A.Save();

	TestWorld B(1234);
	VT_REQUIRE_EQ(static_cast<int>(LoadSnapshot(B.Instance, Image.data(), Image.size())),
				  static_cast<int>(SnapshotResult::Ok));

	VT_CHECK_EQ(B.Instance.Now(), A.Instance.Now());
	VT_CHECK(B.Instance.RootStream().GetState() == A.Instance.RootStream().GetState());
	VT_CHECK(B.Instance.Ids().GetState() == A.Instance.Ids().GetState());
	VT_CHECK(B.Instance.Entities().GetState() == A.Instance.Entities().GetState());
	VT_CHECK(SamePool(B.Instance.Components().GetPool(B.PosType), A.Instance.Components().GetPool(A.PosType)));
	VT_CHECK(SamePool(B.Instance.Components().GetPool(B.WealthType), A.Instance.Components().GetPool(A.WealthType)));
	VT_CHECK(B.Instance.Events().GetPending() == A.Instance.Events().GetPending());
	VT_CHECK(B.Instance.Events().PendingCount() >= uint64{1});
	VT_CHECK_EQ(B.Instance.Log().Count(), A.Instance.Log().Count());
	VT_CHECK_EQ(B.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(ComputeStateDigest(B.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK(B.Save() == Image);
}

VAELEN_TEST(Snapshot, RestoredWorldContinuesExactlyLikeTheOriginal)
{
	TestWorld Reference(77);
	Reference.Populate(8);
	Reference.Instance.TickMany(300);
	const Hash64 ReferenceDigest = ComputeStateDigest(Reference.Instance);

	TestWorld Saved(77);
	Saved.Populate(8);
	Saved.Instance.TickMany(120);
	const std::vector<uint8> Image = Saved.Save();
	Saved.Instance.TickMany(999); // the saving world moves on; irrelevant to the restore

	TestWorld Restored(77);
	VT_REQUIRE_EQ(static_cast<int>(LoadSnapshot(Restored.Instance, Image.data(), Image.size())),
				  static_cast<int>(SnapshotResult::Ok));
	Restored.Instance.TickMany(180);
	VT_CHECK_EQ(Restored.Instance.Now(), Reference.Instance.Now());
	VT_CHECK_EQ(ComputeStateDigest(Restored.Instance), ReferenceDigest);
	VT_CHECK_EQ(Restored.Instance.Log().Digest(), Reference.Instance.Log().Digest());
	VT_CHECK_EQ(Restored.Instance.Log().Count(), Reference.Instance.Log().Count());
}

VAELEN_TEST(Snapshot, IdenticalWorldsProduceByteIdenticalImages)
{
	TestWorld A(5);
	TestWorld B(5);
	A.Populate(30);
	B.Populate(30);
	A.Instance.TickMany(100);
	B.Instance.TickMany(100);
	VT_CHECK(A.Save() == B.Save());
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));

	TestWorld C(6);
	C.Populate(30);
	C.Instance.TickMany(100);
	VT_CHECK(A.Save() != C.Save());
}

VAELEN_TEST(Snapshot, EmptyWorldRoundTrips)
{
	TestWorld A(1);
	const std::vector<uint8> Image = A.Save();
	TestWorld B(1);
	VT_CHECK_EQ(static_cast<int>(LoadSnapshot(B.Instance, Image.data(), Image.size())),
				static_cast<int>(SnapshotResult::Ok));
	VT_CHECK(B.Save() == Image);
	VT_CHECK_EQ(B.Instance.Entities().GetAliveCount(), 0u);
	B.Instance.TickMany(10);
	A.Instance.TickMany(10);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
}

VAELEN_TEST(Snapshot, WrongVersionIsRejectedExplicitly)
{
	TestWorld A(3);
	A.Populate(3);
	std::vector<uint8> Image = A.Save();
	// Header: magic[8] then the version u32.
	uint32 Version = 0;
	std::memcpy(&Version, Image.data() + 8, 4);
	VT_CHECK_EQ(Version, static_cast<uint32>(VAELEN_SAVE_FORMAT_VERSION));
	Version += 1;
	std::memcpy(Image.data() + 8, &Version, 4);
	// Recompute the trailer so only the version differs.
	const Hash64 Digest = HashBytes(reinterpret_cast<const char*>(Image.data()), Image.size() - 8);
	std::memcpy(Image.data() + Image.size() - 8, &Digest, 8);

	TestWorld B(3);
	VT_CHECK_EQ(static_cast<int>(LoadSnapshot(B.Instance, Image.data(), Image.size())),
				static_cast<int>(SnapshotResult::VersionMismatch));
	VT_CHECK_STREQ(SnapshotResultToString(SnapshotResult::VersionMismatch), "VersionMismatch");
}

VAELEN_TEST(Snapshot, BadMagicTruncationAndCorruptionAreRejected)
{
	TestWorld A(4);
	A.Populate(5);
	A.Instance.TickMany(20);
	const std::vector<uint8> Image = A.Save();

	auto Load = [](const std::vector<uint8>& Bytes)
	{
		TestWorld B(4);
		return LoadSnapshot(B.Instance, Bytes.data(), Bytes.size());
	};
	auto Reseal = [](std::vector<uint8>& Bytes)
	{
		const Hash64 Digest = HashBytes(reinterpret_cast<const char*>(Bytes.data()), Bytes.size() - 8);
		std::memcpy(Bytes.data() + Bytes.size() - 8, &Digest, 8);
	};

	// A flipped byte anywhere: the trailer digest catches it before any state changes.
	for (usize Offset : {usize{0}, usize{8}, usize{40}, Image.size() / 2, Image.size() - 9})
	{
		std::vector<uint8> Bad = Image;
		Bad[Offset] ^= 0x5a;
		VT_CHECK_MSG(Load(Bad) == SnapshotResult::Corrupt, "flipped byte at %zu must be Corrupt", Offset);
	}
	{
		std::vector<uint8> Bad = Image;
		uint8* Bytes = Bad.data();
		VT_REQUIRE(Bytes != nullptr); // keeps gcc -O2 -Wnull-dereference honest
		Bytes[0] = 'X';
		Reseal(Bad);
		VT_CHECK(Load(Bad) == SnapshotResult::BadMagic);
	}
	{
		std::vector<uint8> Bad(Image.begin(), Image.begin() + 20);
		VT_CHECK(Load(Bad) == SnapshotResult::Truncated);
		VT_CHECK(LoadSnapshot(A.Instance, nullptr, 0) == SnapshotResult::Truncated);
	}
	{
		// Cut inside the body, resealed: the digest passes, the body runs out.
		std::vector<uint8> Bad(Image.begin(), Image.begin() + static_cast<std::ptrdiff_t>(Image.size() - 100));
		Reseal(Bad);
		VT_CHECK(Load(Bad) == SnapshotResult::Truncated);
	}
	{
		// Trailing garbage, resealed: rejected as Corrupt.
		std::vector<uint8> Bad = Image;
		Bad.insert(Bad.end() - 8, 4, uint8{0});
		Reseal(Bad);
		VT_CHECK(Load(Bad) == SnapshotResult::Corrupt);
	}
	{
		// An absurd element count with a valid digest (the log's byte count, the
		// last u64 before the log bytes) must be refused before any allocation.
		std::vector<uint8> Bad = Image;
		const usize LogBytes = static_cast<usize>(A.Instance.Log().Count()) * sizeof(Event) + 16;
		const usize CountOffset = Bad.size() - 8 - LogBytes - 8;
		uint64 Absurd = uint64{1} << 40;
		std::memcpy(Bad.data() + CountOffset, &Absurd, 8);
		Reseal(Bad);
		VT_CHECK(Load(Bad) == SnapshotResult::Truncated);
	}
	{
		// Different seed: identity mismatch.
		TestWorld Other(5);
		VT_CHECK(LoadSnapshot(Other.Instance, Image.data(), Image.size()) == SnapshotResult::LayoutMismatch);
	}
}

VAELEN_TEST(Snapshot, DifferentComponentLayoutIsRejected)
{
	TestWorld A(8);
	A.Populate(4);
	const std::vector<uint8> Image = A.Save();
	TestWorld B(8, /*WithWealth=*/false);
	VT_CHECK(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::LayoutMismatch);

	// Same types registered but one pool never created: MissingPool.
	WorldConfig Config;
	Config.Seed = 8;
	World C(Config);
	C.Components().CreatePool(C.Types().Register<Position>("Position"));
	C.Types().Register<Wealth>("Wealth");
	C.Build();
	VT_CHECK(LoadSnapshot(C, Image.data(), Image.size()) == SnapshotResult::MissingPool);
}

VAELEN_TEST(Snapshot, LargeWorldRoundTrips)
{
	TestWorld A(2024);
	A.Populate(50000);
	A.Instance.TickMany(20);
	const std::vector<uint8> Image = A.Save();
	VT_CHECK(Image.size() > 50000u * 8u);

	TestWorld B(2024);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(B.Instance.Entities().GetState() == A.Instance.Entities().GetState());
	VT_CHECK_EQ(B.Instance.Components().GetPool(B.PosType).Size(), A.Instance.Components().GetPool(A.PosType).Size());
	VT_CHECK(B.Save() == Image);
}
