# VaelenPolitics — engine-agnostic politics module (Phase 07).
# Every file listed here is ALSO compiled by UBT (Source/VaelenPolitics/*.Build.cs).
# VaelenPoliticsModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_POLITICS_PUBLIC_HEADERS
  Public/Vaelen/Politics/PoliticsApi.h
  Public/Vaelen/Politics/Polities.h
  Public/Vaelen/Politics/Law.h
  Public/Vaelen/Politics/Reach.h
  Public/Vaelen/Politics/Succession.h
  Public/Vaelen/Politics/Factions.h
)

set(VAELEN_POLITICS_SOURCES
  Private/Polities.cpp
  Private/Law.cpp
  Private/Reach.cpp
  Private/Succession.cpp
  Private/Factions.cpp
)

add_library(VaelenPolitics STATIC ${VAELEN_POLITICS_SOURCES} ${VAELEN_POLITICS_PUBLIC_HEADERS})
add_library(Vaelen::Politics ALIAS VaelenPolitics)

target_include_directories(VaelenPolitics
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenPolitics PUBLIC Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenPolitics PUBLIC cxx_std_20)
