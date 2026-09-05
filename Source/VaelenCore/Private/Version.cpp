// VAELEN - VaelenCore
#include "Vaelen/Core/Version.h"

#define VAELEN_STR2(x) #x
#define VAELEN_STR(x) VAELEN_STR2(x)

namespace Vaelen
{
	ProjectVersion GetProjectVersion() noexcept
	{
		return ProjectVersion{};
	}

	const char* GetProjectVersionString() noexcept
	{
		return VAELEN_STR(VAELEN_VERSION_MAJOR) "." VAELEN_STR(VAELEN_VERSION_MINOR) "." VAELEN_STR(VAELEN_VERSION_PATCH);
	}
} // namespace Vaelen
