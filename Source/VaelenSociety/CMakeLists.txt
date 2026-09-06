# VaelenSociety — engine-agnostic society module (Phase 05).
# Every file listed here is ALSO compiled by UBT (Source/VaelenSociety/*.Build.cs).
# VaelenSocietyModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_SOCIETY_PUBLIC_HEADERS
  Public/Vaelen/Society/SocietyApi.h
  Public/Vaelen/Society/Organizations.h
  Public/Vaelen/Society/Standing.h
)

set(VAELEN_SOCIETY_SOURCES
  Private/Organizations.cpp
  Private/Standing.cpp
)

add_library(VaelenSociety STATIC ${VAELEN_SOCIETY_SOURCES} ${VAELEN_SOCIETY_PUBLIC_HEADERS})
add_library(Vaelen::Society ALIAS VaelenSociety)

target_include_directories(VaelenSociety
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenSociety PUBLIC Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenSociety PUBLIC cxx_std_20)
