# VaelenMilitary — engine-agnostic military module (Phase 08).
# Every file listed here is ALSO compiled by UBT (Source/VaelenMilitary/*.Build.cs).
# VaelenMilitaryModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_MILITARY_PUBLIC_HEADERS
  Public/Vaelen/Military/MilitaryApi.h
  Public/Vaelen/Military/Armies.h
)

set(VAELEN_MILITARY_SOURCES
  Private/Armies.cpp
)

add_library(VaelenMilitary STATIC ${VAELEN_MILITARY_SOURCES} ${VAELEN_MILITARY_PUBLIC_HEADERS})
add_library(Vaelen::Military ALIAS VaelenMilitary)

target_include_directories(VaelenMilitary
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenMilitary PUBLIC Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenMilitary PUBLIC cxx_std_20)
