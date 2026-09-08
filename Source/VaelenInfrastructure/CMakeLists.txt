# VaelenInfrastructure — engine-agnostic infrastructure module (Phase 09).
# Every file listed here is ALSO compiled by UBT (Source/VaelenInfrastructure/*.Build.cs).
# VaelenInfrastructureModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_INFRASTRUCTURE_PUBLIC_HEADERS
  Public/Vaelen/Infrastructure/InfrastructureApi.h
  Public/Vaelen/Infrastructure/Buildings.h
  Public/Vaelen/Infrastructure/Works.h
)

set(VAELEN_INFRASTRUCTURE_SOURCES
  Private/Buildings.cpp
  Private/Works.cpp
)

add_library(VaelenInfrastructure STATIC ${VAELEN_INFRASTRUCTURE_SOURCES} ${VAELEN_INFRASTRUCTURE_PUBLIC_HEADERS})
add_library(Vaelen::Infrastructure ALIAS VaelenInfrastructure)

target_include_directories(VaelenInfrastructure
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenInfrastructure PUBLIC Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenInfrastructure PUBLIC cxx_std_20)
