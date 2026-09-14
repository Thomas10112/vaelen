# VaelenPlayer tests: Tests/Player/Test_<Suite>.cpp -> CTest entry Player.<Suite>.
file(GLOB VAELEN_PLAYER_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenPlayerTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_PLAYER_TEST_SOURCES})

target_include_directories(VaelenPlayerTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenPlayerTests PRIVATE Vaelen::Player Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_PLAYER_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_PLAYER_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Player.${SuiteName} COMMAND VaelenPlayerTests --suite ${SuiteName})
  list(APPEND VAELEN_PLAYER_TEST_NAMES Player.${SuiteName})
endforeach()

# 14.01's probe: Intent.h and Stream.h compiled with ONLY Core and Player public
# headers on the include path and linked to NO LIBRARY, because a linked
# library's PUBLIC include directories propagate and would quietly hand this
# file every kernel header. The one link it has is vaelen_build_flags, an
# INTERFACE target of compile options and definitions with no include
# directory of its own (CMakeLists.txt:36), so the probe is held to the same
# warnings-as-errors as the module without widening its include set by one
# directory. Any other target_link_libraries here would defeat the fence. The
# probe is the fence; compiling it is the test. If either leaf ever includes
# past those two directories, this stops building and every CI leg is red at
# the Build step, not the Test step.
add_executable(VaelenPlayerIntentLeafProbe ${CMAKE_CURRENT_SOURCE_DIR}/Probe_IntentLeaf.cpp)
target_include_directories(VaelenPlayerIntentLeafProbe PRIVATE
  ${PROJECT_SOURCE_DIR}/Source/VaelenCore/Public
  ${PROJECT_SOURCE_DIR}/Source/VaelenPlayer/Public)
target_link_libraries(VaelenPlayerIntentLeafProbe PRIVATE vaelen_build_flags)
add_test(NAME Player.IntentLeaf COMMAND VaelenPlayerIntentLeafProbe)
list(APPEND VAELEN_PLAYER_TEST_NAMES Player.IntentLeaf)

add_test(NAME Player.Registry COMMAND VaelenPlayerTests --check-registry)
add_test(NAME Player.Shuffled COMMAND VaelenPlayerTests --shuffle 1)
list(APPEND VAELEN_PLAYER_TEST_NAMES Player.Registry Player.Shuffled)

set_tests_properties(${VAELEN_PLAYER_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Player.Shuffled PROPERTIES TIMEOUT 5400)
# The phase gate lives a lifetime at 256 and then replays it, twice over.
set_tests_properties(Player.PlayerGate PROPERTIES TIMEOUT 5400)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Player.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Player.Shuffled PROPERTIES COST 10000)
set_tests_properties(Player.PlayerGate PROPERTIES COST 5000)
