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
