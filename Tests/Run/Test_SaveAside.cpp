// VAELEN - VaelenRun tests
// Phase 16 task 16.15: the last good save keeps its NAME.
//
// 16.07 promised that a write which fails leaves the previous checkpoint
// whole, and Run.Store holds the stdio store to it for a disk that fills while
// the temporary is written. The 19.10 audit read the ENGINE'S replace and found
// the other half of the promise unkept: FFileManagerGeneric::Move with Replace
// is a delete of the old file and then a rename, so a rename that fails after
// that delete leaves the old save nowhere and the new one under `.writing` -
// whole, both of them, and neither under the name the player gave. The bytes
// were safe; the NAME was not. (The C library's rename on Windows refuses an
// existing destination outright, which is the same window by another door.)
//
// Since 16.15 every store of Run::ICheckpointStore sets the old save ASIDE as
// `<name>.previous` before the new one is moved into place, forgets the aside
// afterwards, and gives the aside back under its own name when the name is
// missing (Read, and List through it). This file holds the stdio store to
// that, on real files, and keeps the pre-fix behaviour beside it as a store
// that replaces the way the engine did, so that the defect stays measurable
// after the code that had it is gone (as Test_StoreColdProcess.cpp does).
//
// STATUS: PROTOTYPE (Phase 16 task 16.15)
#include "VaelenTest.h"

#include "StdioCheckpointStore.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Store.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;
using VaelenHost::StdioCheckpointStore;

namespace
{
	/// A directory of this test's own under VAELEN_STORE_DIR, so that Run.Store
	/// and the cold listing cannot count its files nor it theirs.
	std::string Somewhere()
	{
		return std::string(VAELEN_STORE_DIR) + "/aside/";
	}

	bool MakeTheDirectory()
	{
		const std::string Path = Somewhere();
		std::FILE* Probe = std::fopen((Path + ".probe").c_str(), "wb");
		if (Probe == nullptr)
		{
			return false;
		}
		std::fclose(Probe);
		std::remove((Path + ".probe").c_str());
		return true;
	}

	bool IsThere(const std::string& Path)
	{
		std::FILE* F = std::fopen(Path.c_str(), "rb");
		if (F == nullptr)
		{
			return false;
		}
		std::fclose(F);
		return true;
	}

	bool PutFile(const std::string& Path, const std::vector<uint8>& Bytes)
	{
		std::FILE* F = std::fopen(Path.c_str(), "wb");
		if (F == nullptr)
		{
			return false;
		}
		const usize Wrote = Bytes.empty() ? 0u : std::fwrite(Bytes.data(), 1, Bytes.size(), F);
		return std::fclose(F) == 0 && Wrote == Bytes.size();
	}

	/// Every file a name can stand behind, gone: the test's own sweep, since a
	/// store's Forget is one of the things under test here.
	void Scrub(const char* Name)
	{
		const std::string Path = Somewhere() + Name;
		std::remove(Path.c_str());
		std::remove((Path + PreviousSuffix).c_str());
		std::remove((Path + WritingSuffix).c_str());
	}

	struct Made
	{
		std::vector<uint8> Bytes;
		uint64 Tick = 0;
	};

	/// Two containers a listing can tell apart: a longer life is a later tick.
	Made AContainer(uint32 Years)
	{
		Made M;
		Options O;
		O.Size = 16u;
		O.PreHistory = 4u;
		O.Years = Years;
		Aelvor A(O);
		if (!A.Begin() || BuildCheckpoint(A, M.Bytes) != CheckpointResult::Ok)
		{
			M.Bytes.clear();
			return M;
		}
		M.Tick = A.Instance().Clock().Now();
		return M;
	}

	bool Same(const std::vector<uint8>& A, const std::vector<uint8>& B)
	{
		return A.size() == B.size() && (A.empty() || std::memcmp(A.data(), B.data(), A.size()) == 0);
	}

	const StoreEntry* Named(const std::vector<StoreEntry>& Entries, const char* Name)
	{
		for (const StoreEntry& E : Entries)
		{
			if (E.Name == Name)
			{
				return &E;
			}
		}
		return nullptr;
	}

	/// THE FAILURE NO DISK WILL ARRANGE: the rename that puts the new save in
	/// its place fails, every time, after the old one was set aside. Everything
	/// else is the real store.
	class AStoreWhoseReplaceFails final : public StdioCheckpointStore
	{
	public:
		using StdioCheckpointStore::StdioCheckpointStore;

	protected:
		bool Rename(const std::string& From, const std::string& To) override
		{
			const std::string Temp(WritingSuffix);
			const bool IsTheReplace =
				From.size() > Temp.size() && From.compare(From.size() - Temp.size(), Temp.size(), Temp) == 0;
			if (IsTheReplace)
			{
				++Refused;
				return false;
			}
			return StdioCheckpointStore::Rename(From, To);
		}

	public:
		int Refused = 0;
	};

	/// THE PRE-FIX ARM: a store that replaces the way the engine's Move does -
	/// the old file DELETED, then the temporary renamed over its name - with
	/// the same rename failing. What 16.14's engine store did until 16.15,
	/// kept here so the defect is measured and not remembered.
	class AStoreThatReplacesLikeTheEngineDid final : public StdioCheckpointStore
	{
	public:
		explicit AStoreThatReplacesLikeTheEngineDid(std::string InDirectory)
			: StdioCheckpointStore(InDirectory), Where(std::move(InDirectory))
		{
		}

		StoreResult Write(const char* Name, const uint8* Bytes, usize Size) override
		{
			if (!IsUsableCheckpointName(Name))
			{
				return StoreResult::BadName;
			}
			const std::string Final = Where + Name;
			const std::string Temp = Final + WritingSuffix;
			std::vector<uint8> Staged(Bytes, Bytes + Size);
			if (!PutFile(Temp, Staged))
			{
				return StoreResult::DiskFull;
			}
			// The engine's Move with Replace, spelled out: delete, then rename.
			std::remove(Final.c_str());
			if (!Rename(Temp, Final))
			{
				// 19.10's arm: the temporary is kept. The old save is gone.
				return StoreResult::CannotWrite;
			}
			return StoreResult::Ok;
		}

		StoreResult Read(const char* Name, std::vector<uint8>& Out) override
		{
			// As 16.14 read: the name, and nothing else.
			if (!IsUsableCheckpointName(Name))
			{
				return StoreResult::BadName;
			}
			const std::string Path = Where + Name;
			std::FILE* F = std::fopen(Path.c_str(), "rb");
			if (F == nullptr)
			{
				return StoreResult::NotFound;
			}
			std::fclose(F);
			return StdioCheckpointStore::Read(Name, Out);
		}

	protected:
		bool Rename(const std::string&, const std::string&) override { return false; }

	private:
		std::string Where;
	};
} // namespace

VAELEN_TEST(SaveAside, ThePreviousNameIsHiddenLikeTheWritingOne)
{
	// CHECKED IN THE KERNEL, as the `.writing` suffix is: no listing shows an
	// aside as a second save, and no caller can write over one by name.
	VT_CHECK_MSG(!IsUsableCheckpointName("life.previous"), "a save set aside");
	VT_CHECK_MSG(!IsUsableCheckpointName(".previous"), "the bare suffix (a leading dot besides)");
	VT_CHECK_MSG(IsUsableCheckpointName("previous-life"), "a name that merely contains the word");
	VT_CHECK_MSG(IsUsableCheckpointName("life.previous.one"), "the suffix anywhere but the end");
	VT_CHECK_MSG(IsUsableCheckpointName("life"), "and the name itself, still");

	VT_REQUIRE(MakeTheDirectory());
	StdioCheckpointStore Store(Somewhere());
	std::vector<uint8> Out;
	VT_CHECK(Store.Write("life.previous", reinterpret_cast<const uint8*>("x"), 1u) == StoreResult::BadName);
	VT_CHECK(Store.Read("life.previous", Out) == StoreResult::BadName);
	VT_CHECK(Store.Forget("life.previous") == StoreResult::BadName);
}

VAELEN_TEST(SaveAside, AWriteOverAnExistingSaveLeavesTheNewOneAndNothingElse)
{
	// THE CONTROL for everything below: the ordinary replace. The new bytes
	// are under the name, the aside was forgotten, the temporary too, and
	// the listing has one save with the NEW tick.
	VT_REQUIRE(MakeTheDirectory());
	Scrub("plain");
	const Made Old = AContainer(2u);
	const Made New = AContainer(4u);
	VT_REQUIRE(!Old.Bytes.empty() && !New.Bytes.empty());
	VT_REQUIRE(Old.Tick != New.Tick);

	StdioCheckpointStore Store(Somewhere());
	VT_CHECK(Store.Write("plain", Old.Bytes.data(), Old.Bytes.size()) == StoreResult::Ok);
	VT_CHECK(Store.Write("plain", New.Bytes.data(), New.Bytes.size()) == StoreResult::Ok);

	std::vector<uint8> Back;
	VT_CHECK(Store.Read("plain", Back) == StoreResult::Ok);
	VT_CHECK_MSG(Same(Back, New.Bytes), "the second write's bytes are what the name holds");
	VT_CHECK_MSG(!IsThere(Somewhere() + "plain" + PreviousSuffix), "the aside was forgotten after the replace");
	VT_CHECK_MSG(!IsThere(Somewhere() + "plain" + WritingSuffix), "and the temporary is gone");
	const std::vector<StoreEntry> Listed = Store.List();
	const StoreEntry* Entry = Named(Listed, "plain");
	VT_CHECK_MSG(Listed.size() == 1u, "%zu listed, wanted the one save", Listed.size());
	VT_CHECK_MSG(Entry != nullptr && Entry->Tick == New.Tick, "and it is the new one");
	VT_CHECK(Store.Forget("plain") == StoreResult::Ok);
	VT_CHECK(Store.Read("plain", Back) == StoreResult::NotFound);
}

VAELEN_TEST(SaveAside, AReplaceThatFailsHalfwayKeepsBothAndTheOldOneAnswersToItsName)
{
	// THE SHARP ARM. The rename that puts the new save in place fails after
	// the old one was set aside: the write says CannotWrite, the old save is
	// whole under `.previous`, the new one whole under `.writing`, and a
	// plain store asked for the name gives the OLD bytes back - and puts them
	// back under the name while it is at it.
	VT_REQUIRE(MakeTheDirectory());
	Scrub("held");
	const Made Old = AContainer(2u);
	const Made New = AContainer(4u);
	VT_REQUIRE(!Old.Bytes.empty() && !New.Bytes.empty());

	AStoreWhoseReplaceFails Failing(Somewhere());
	// The first write of a name has nothing to set aside and STILL fails
	// here: the replace is the same rename. Nothing is under the name, the
	// temporary is kept - and nothing was lost, since nothing was there.
	const StoreResult First = Failing.Write("held", Old.Bytes.data(), Old.Bytes.size());
	VT_CHECK_MSG(First == StoreResult::CannotWrite, "%s", StoreResultToString(First));
	VT_CHECK(Failing.Refused == 1);
	VT_CHECK(!IsThere(Somewhere() + "held"));
	VT_CHECK(IsThere(Somewhere() + "held" + WritingSuffix));
	Scrub("held");

	// Now with a save in place: a plain store puts the old one there first.
	StdioCheckpointStore Plain(Somewhere());
	VT_REQUIRE(Plain.Write("held", Old.Bytes.data(), Old.Bytes.size()) == StoreResult::Ok);
	const StoreResult Second = Failing.Write("held", New.Bytes.data(), New.Bytes.size());
	VT_CHECK_MSG(Second == StoreResult::CannotWrite, "%s", StoreResultToString(Second));
	VT_CHECK(Failing.Refused == 2);
	VT_CHECK_MSG(!IsThere(Somewhere() + "held"), "the name is empty: this is the window");
	VT_CHECK_MSG(IsThere(Somewhere() + "held" + PreviousSuffix), "the old save is whole under the aside");
	VT_CHECK_MSG(IsThere(Somewhere() + "held" + WritingSuffix), "and the new one under the temporary");

	// THE LISTING, from a store that saw none of it: one save, named as the
	// player named it, with the OLD tick.
	StdioCheckpointStore Cold(Somewhere());
	const std::vector<StoreEntry> Listed = Cold.List();
	VT_CHECK_MSG(Listed.size() == 1u, "%zu listed in the window, wanted 1", Listed.size());
	const StoreEntry* Entry = Named(Listed, "held");
	VT_CHECK_MSG(Entry != nullptr, "the save in the window is listed under its own name");
	VT_CHECK_MSG(Entry != nullptr && Entry->Tick == Old.Tick, "and it is the old one");
	VT_CHECK_MSG(Named(Listed, "held.previous") == nullptr, "the aside is not a second save");

	// Listing read it, so the name is back; read it again through another
	// cold store to be sure that is what happened on the disk.
	VT_CHECK_MSG(IsThere(Somewhere() + "held"), "reading restored the name");
	VT_CHECK_MSG(!IsThere(Somewhere() + "held" + PreviousSuffix), "and the aside is spent");
	StdioCheckpointStore Colder(Somewhere());
	std::vector<uint8> Back;
	VT_CHECK(Colder.Read("held", Back) == StoreResult::Ok);
	VT_CHECK_MSG(Same(Back, Old.Bytes), "the old bytes, byte for byte");
	CheckpointView View;
	VT_CHECK_MSG(ReadCheckpoint(Back.data(), Back.size(), View).Result == CheckpointResult::Ok,
				 "and still a container");

	// And the next write of the name goes through: the stale temporary is
	// overwritten by the staging, the restored save set aside and forgotten.
	VT_CHECK(Colder.Write("held", New.Bytes.data(), New.Bytes.size()) == StoreResult::Ok);
	VT_CHECK(Colder.Read("held", Back) == StoreResult::Ok);
	VT_CHECK(Same(Back, New.Bytes));
	VT_CHECK(!IsThere(Somewhere() + "held" + PreviousSuffix));
	VT_CHECK(!IsThere(Somewhere() + "held" + WritingSuffix));
	Scrub("held");
}

VAELEN_TEST(SaveAside, WhatAnInterruptedReplaceLeavesOnTheDiskIsReadBackWithoutAStore)
{
	// The window as a CRASH leaves it, laid out by hand with no store object
	// involved - the aside renamed, the temporary whole, the name absent - so
	// the restore is shown to depend on the disk alone and not on the object
	// that failed.
	VT_REQUIRE(MakeTheDirectory());
	Scrub("crash");
	const Made Old = AContainer(2u);
	const Made New = AContainer(4u);
	VT_REQUIRE(!Old.Bytes.empty() && !New.Bytes.empty());
	VT_REQUIRE(PutFile(Somewhere() + "crash" + PreviousSuffix, Old.Bytes));
	VT_REQUIRE(PutFile(Somewhere() + "crash" + WritingSuffix, New.Bytes));

	StdioCheckpointStore Store(Somewhere());
	std::vector<uint8> Back;
	VT_CHECK(Store.Read("crash", Back) == StoreResult::Ok);
	VT_CHECK_MSG(Same(Back, Old.Bytes), "the last good save, under its name");
	VT_CHECK(IsThere(Somewhere() + "crash"));
	VT_CHECK(!IsThere(Somewhere() + "crash" + PreviousSuffix));
	VT_CHECK_MSG(IsThere(Somewhere() + "crash" + WritingSuffix),
				 "the temporary is nobody's to remove but Forget's - it is the newer save, hidden");

	// THE CONTROL: an aside whose name IS there is left alone, and the name
	// wins - a write in the middle of its replace, or one that failed to
	// forget its aside, and in neither case a second save.
	VT_REQUIRE(PutFile(Somewhere() + "crash" + PreviousSuffix, New.Bytes));
	VT_CHECK(Store.Read("crash", Back) == StoreResult::Ok);
	VT_CHECK_MSG(Same(Back, Old.Bytes), "the name's bytes and not the aside's");
	VT_CHECK_MSG(IsThere(Somewhere() + "crash" + PreviousSuffix), "and the aside was not touched");
	VT_CHECK_MSG(Store.List().size() == 1u, "one save listed, not two");
	Scrub("crash");
}

VAELEN_TEST(SaveAside, ForgetTakesTheAsideAndTheTemporaryToo)
{
	// A forgotten save must not come back through the restore: Forget takes
	// the name, the aside and the temporary, and says NotFound only when
	// none of the three was there.
	VT_REQUIRE(MakeTheDirectory());
	Scrub("gone");
	const Made Old = AContainer(2u);
	VT_REQUIRE(!Old.Bytes.empty());
	VT_REQUIRE(PutFile(Somewhere() + "gone" + PreviousSuffix, Old.Bytes));
	VT_REQUIRE(PutFile(Somewhere() + "gone" + WritingSuffix, Old.Bytes));

	StdioCheckpointStore Store(Somewhere());
	VT_CHECK_MSG(Store.Forget("gone") == StoreResult::Ok, "a save that exists only as its aside is forgettable");
	VT_CHECK(!IsThere(Somewhere() + "gone" + PreviousSuffix));
	VT_CHECK(!IsThere(Somewhere() + "gone" + WritingSuffix));
	std::vector<uint8> Back;
	VT_CHECK_MSG(Store.Read("gone", Back) == StoreResult::NotFound, "and does not come back");
	VT_CHECK(Store.List().empty());
	VT_CHECK(Store.Forget("gone") == StoreResult::NotFound);
}

VAELEN_TEST(SaveAside, TheStoreThatReplacedLikeTheEngineLostTheName)
{
	// THE PRE-FIX ARM, measured: delete then rename, the rename failing. The
	// old save is gone from its name, the new one sits under `.writing`, and
	// the store that wrote both answers NotFound for the name - the bytes
	// whole on the disk and no way to ask for them. That is the 19.10
	// audit's finding, and the reason the aside exists. If this arm ever
	// passes, the model of the old behaviour is wrong, not the world.
	VT_REQUIRE(MakeTheDirectory());
	Scrub("lost");
	const Made Old = AContainer(2u);
	const Made New = AContainer(4u);
	VT_REQUIRE(!Old.Bytes.empty() && !New.Bytes.empty());
	VT_REQUIRE(PutFile(Somewhere() + "lost", Old.Bytes));

	AStoreThatReplacesLikeTheEngineDid AsItWas(Somewhere());
	VT_CHECK(AsItWas.Write("lost", New.Bytes.data(), New.Bytes.size()) == StoreResult::CannotWrite);
	std::vector<uint8> Back;
	VT_CHECK_MSG(AsItWas.Read("lost", Back) == StoreResult::NotFound,
				 "the pre-fix store found the name; if it now does, this arm is testing nothing");
	VT_CHECK_MSG(!IsThere(Somewhere() + "lost"), "nothing under the name");
	VT_CHECK_MSG(IsThere(Somewhere() + "lost" + WritingSuffix), "the new save whole under the temporary");
	VT_CHECK_MSG(!IsThere(Somewhere() + "lost" + PreviousSuffix), "and no aside, since it never set one");

	// AND THE FIXED STORE OVER THE SAME DISK finds nothing either - the old
	// save was DELETED, not set aside; nothing can be restored that was
	// never kept. The fix is in the write, and this shows it is not in the
	// read.
	StdioCheckpointStore Fixed(Somewhere());
	VT_CHECK(Fixed.Read("lost", Back) == StoreResult::NotFound);
	Scrub("lost");
}
