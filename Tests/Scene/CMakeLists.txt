# VaelenScene tests: Tests/Scene/Test_<Suite>.cpp -> CTest entry Scene.<Suite> (Phase 19, ADR-0156).
file(GLOB VAELEN_SCENE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenSceneTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_SCENE_TEST_SOURCES})

target_include_directories(VaelenSceneTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
# Vaelen::Run only to make a real map to build from; the scene itself links View and Core.
target_link_libraries(VaelenSceneTests PRIVATE Vaelen::Scene Vaelen::Run Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_SCENE_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_SCENE_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Scene.${SuiteName} COMMAND VaelenSceneTests --suite ${SuiteName})
  list(APPEND VAELEN_SCENE_TEST_NAMES Scene.${SuiteName})
endforeach()

add_test(NAME Scene.Registry COMMAND VaelenSceneTests --check-registry)
list(APPEND VAELEN_SCENE_TEST_NAMES Scene.Registry)

set_tests_properties(${VAELEN_SCENE_TEST_NAMES} PROPERTIES TIMEOUT 1800)
# The 256 map and two whole-map meshes: the long pole of this directory.
set_tests_properties(Scene.Terrain PROPERTIES COST 1500)
