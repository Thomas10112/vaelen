// VAELEN - VaelenInfrastructure
// Export macro of the VaelenInfrastructure module (same scheme as VAELEN_MILITARY_API).
//
// STATUS: PROTOTYPE (Phase 09)
#pragma once

#if defined(VAELEN_INFRASTRUCTURE_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_INFRASTRUCTURE_API __declspec(dllexport)
#	else
#		define VAELEN_INFRASTRUCTURE_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_INFRASTRUCTURE_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_INFRASTRUCTURE_API __declspec(dllimport)
#	else
#		define VAELEN_INFRASTRUCTURE_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_INFRASTRUCTURE_API
#endif
