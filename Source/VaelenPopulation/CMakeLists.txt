# VaelenPopulation — engine-agnostic population module (Phase 04).
# Every file listed here is ALSO compiled by UBT (Source/VaelenPopulation/*.Build.cs).
# VaelenPopulationModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_POPULATION_PUBLIC_HEADERS
  Public/Vaelen/Population/PopulationApi.h
  Public/Vaelen/Population/Persons.h
)

set(VAELEN_POPULATION_SOURCES
  Private/Persons.cpp
)

add_library(VaelenPopulation STATIC ${VAELEN_POPULATION_SOURCES} ${VAELEN_POPULATION_PUBLIC_HEADERS})
add_library(Vaelen::Population ALIAS VaelenPopulation)

target_include_directories(VaelenPopulation
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenPopulation PUBLIC Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenPopulation PUBLIC cxx_std_20)
