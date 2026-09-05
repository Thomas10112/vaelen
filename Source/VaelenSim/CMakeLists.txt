# VaelenSim — engine-agnostic simulation module (Phase 01).
# Every file listed here is ALSO compiled by UBT (Source/VaelenSim/*.Build.cs).
# VaelenSimModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_SIM_PUBLIC_HEADERS
  Public/Vaelen/Sim/SimApi.h
  Public/Vaelen/Sim/EntityHandle.h
  Public/Vaelen/Sim/EntityRegistry.h
  Public/Vaelen/Sim/ComponentType.h
  Public/Vaelen/Sim/ComponentPool.h
  Public/Vaelen/Sim/ComponentStore.h
  Public/Vaelen/Sim/SimClock.h
  Public/Vaelen/Sim/System.h
  Public/Vaelen/Sim/Event.h
  Public/Vaelen/Sim/EventBus.h
  Public/Vaelen/Sim/PlainData.h
  Public/Vaelen/Sim/Archive.h
  Public/Vaelen/Sim/World.h
  Public/Vaelen/Sim/Snapshot.h
  Public/Vaelen/Sim/TileGrid.h
  Public/Vaelen/Sim/WorldMap.h
  Public/Vaelen/Sim/FixedPoint.h
  Public/Vaelen/Sim/Noise.h
)

set(VAELEN_SIM_SOURCES
  Private/EntityRegistry.cpp
  Private/ComponentType.cpp
  Private/ComponentStore.cpp
  Private/Scheduler.cpp
  Private/EventBus.cpp
  Private/Archive.cpp
  Private/World.cpp
  Private/Snapshot.cpp
  Private/WorldMap.cpp
  Private/Noise.cpp
)

add_library(VaelenSim STATIC ${VAELEN_SIM_SOURCES} ${VAELEN_SIM_PUBLIC_HEADERS})
add_library(Vaelen::Sim ALIAS VaelenSim)

target_include_directories(VaelenSim
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenSim PUBLIC Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenSim PUBLIC cxx_std_20)
