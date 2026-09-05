// VAELEN - VaelenSim
// Export macro of the VaelenSim module (same scheme as VAELEN_CORE_API).
//
// STATUS: VALIDATED (Phase 01)
#pragma once

#if defined(VAELEN_SIM_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_SIM_API __declspec(dllexport)
#	else
#		define VAELEN_SIM_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_SIM_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_SIM_API __declspec(dllimport)
#	else
#		define VAELEN_SIM_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_SIM_API
#endif
