// VAELEN - VaelenCore
// Project and data-format versions.
//
// STATUS: VALIDATED (Phase 00) - unit/deterministic/edge tests in Tests/Core;
//         integration and long-duration tests deferred to Phase 01 (ROADMAP 01.07, 01.08).
//
// Two independent version lines:
//   - Project version: what humans see (semantic version).
//   - Save format version: bumped ONLY when the persisted world state layout
//     changes; Persistence/Migration keys its upgraders on this number.
#pragma once

#include "Vaelen/Core/CoreTypes.h"

#define VAELEN_VERSION_MAJOR 0
#define VAELEN_VERSION_MINOR 0
#define VAELEN_VERSION_PATCH 1

/// Bump when the on-disk world state layout changes. Never reuse a number.
#define VAELEN_SAVE_FORMAT_VERSION 3 /* 2: WorldMap section (02.01); 3: 32 world-gen parameters (02.03) */

namespace Vaelen
{
	struct ProjectVersion
	{
		uint16 Major = VAELEN_VERSION_MAJOR;
		uint16 Minor = VAELEN_VERSION_MINOR;
		uint16 Patch = VAELEN_VERSION_PATCH;

		constexpr bool operator==(const ProjectVersion&) const = default;
	};

	/// Returns the compiled-in project version.
	VAELEN_CORE_API ProjectVersion GetProjectVersion() noexcept;

	/// Returns the project version as "MAJOR.MINOR.PATCH" (static storage).
	VAELEN_CORE_API const char* GetProjectVersionString() noexcept;

	/// Returns the compiled-in save format version. (constexpr functions are
	/// inline and never carry the export macro.)
	constexpr uint32 GetSaveFormatVersion() noexcept
	{
		return VAELEN_SAVE_FORMAT_VERSION;
	}
} // namespace Vaelen
