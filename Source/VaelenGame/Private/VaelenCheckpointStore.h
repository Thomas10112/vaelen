// VAELEN - VaelenGame
// Phase 16 task 16.14: Run::ICheckpointStore over the engine's file manager.
//
// The engine twin of Tools/Store/StdioCheckpointStore.h, and the same shape
// ON PURPOSE: a write lands in `<name>.writing` in the SAME directory and is
// moved into place whole, so a disk that fills halfway through has refused the
// new save and kept the old one - the promise Run::StoreResult::DiskFull
// makes. Listing reads the directory (17.03: a store that lists what it wrote
// this process lists nothing for a player who reopened the game), and the
// digest it reports is the image trailer through Run::ImageTrailer, the one
// reader.
//
// Private, because it names the kernel's store and its checkpoint, which the
// UI may not: Tools/check_ui_fence.py reads Public and deliberately not
// Private, and this file is one reason that seam is where it is.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-24, not yet built by UnrealBuildTool nor run. The sitting of 16.14
// is what turns this line; Docs/ENGINE_HANDOFF.md says what it types.
#pragma once

#include "CoreMinimal.h"
#include "Vaelen/Run/Store.h"

#include <vector>

class FVaelenCheckpointStore final : public Vaelen::Run::ICheckpointStore
{
public:
	/// The directory, which need not exist yet: it is listed as empty until
	/// the first write makes it.
	explicit FVaelenCheckpointStore(FString InDirectory);

	Vaelen::Run::StoreResult Write(const char* Name, const Vaelen::uint8* Bytes, Vaelen::usize Size) override;
	Vaelen::Run::StoreResult Read(const char* Name, std::vector<Vaelen::uint8>& Out) override;
	std::vector<Vaelen::Run::StoreEntry> List() override;
	Vaelen::Run::StoreResult Forget(const char* Name) override;

	const FString& Where() const { return Directory; }

private:
	FString PathOf(const char* Name) const;

	FString Directory;
};
