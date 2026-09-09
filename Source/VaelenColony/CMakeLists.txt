# VaelenColony — engine-agnostic colony module (Phase 11).
# Every file listed here is ALSO compiled by UBT (Source/VaelenColony/*.Build.cs).
# VaelenColonyModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_COLONY_PUBLIC_HEADERS
  Public/Vaelen/Colony/ColonyApi.h
  Public/Vaelen/Colony/Mining.h
)

set(VAELEN_COLONY_SOURCES
  Private/Mining.cpp
)

add_library(VaelenColony STATIC ${VAELEN_COLONY_SOURCES} ${VAELEN_COLONY_PUBLIC_HEADERS})
add_library(Vaelen::Colony ALIAS VaelenColony)

target_include_directories(VaelenColony
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenColony PUBLIC Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenColony PUBLIC cxx_std_20)
