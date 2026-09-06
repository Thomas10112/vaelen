// VAELEN - VaelenPopulation
// Export macro of the VaelenPopulation module (same scheme as VAELEN_SIM_API).
//
// STATUS: VALIDATED (Phase 04)
#pragma once

#if defined(VAELEN_POPULATION_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_POPULATION_API __declspec(dllexport)
#	else
#		define VAELEN_POPULATION_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_POPULATION_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_POPULATION_API __declspec(dllimport)
#	else
#		define VAELEN_POPULATION_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_POPULATION_API
#endif
