# VaelenRun tests: Tests/Run/Test_<Suite>.cpp -> CTest entry Run.<Suite>.
file(GLOB VAELEN_RUN_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenRunTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_RUN_TEST_SOURCES})

target_include_directories(VaelenRunTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenRunTests PRIVATE Vaelen::Run Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_RUN_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_RUN_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Run.${SuiteName} COMMAND VaelenRunTests --suite ${SuiteName})
  list(APPEND VAELEN_RUN_TEST_NAMES Run.${SuiteName})
endforeach()

add_test(NAME Run.Registry COMMAND VaelenRunTests --check-registry)
add_test(NAME Run.Shuffled COMMAND VaelenRunTests --shuffle 1)
list(APPEND VAELEN_RUN_TEST_NAMES Run.Registry Run.Shuffled)

set_tests_properties(${VAELEN_RUN_TEST_NAMES} PROPERTIES TIMEOUT 1800)
set_tests_properties(Run.Shuffled PROPERTIES TIMEOUT 5400)
# Run.Aelvor holds the 128/120 world of the frozen pair and a day measured at
# 256 with the colony; it is the long pole of this module after Shuffled.
set_tests_properties(Run.Aelvor PROPERTIES TIMEOUT 3600)

# ── 14.10: a month played by hand, replayed by no hand ───────────────────────
# Tools/Atlas replays the checked-in stream into a fresh world of the STREAM's
# own header and prints the two LogVaelenPlay lines of 14.08, byte for byte
# after the prefix. The four digests below are pinned by pinning the whole
# line, which pins the days, the intents, the three queue counters, the name,
# the person and the region with them.
#
# The stream is a STAND-IN and says so in its name: written by
# `VaelenAtlas --stand`, not by F9 in the editor. Tests/Run/Streams/README.md
# is the whole of that story. When the owner's month lands it is checked in
# beside this one, the four lines below are re-pinned from it, and the
# stand-in goes.
add_test(NAME Replay.Played
  COMMAND ${CMAKE_COMMAND}
          -DATLAS=$<TARGET_FILE:VaelenAtlas>
          -DSTREAM=${PROJECT_SOURCE_DIR}/Tests/Run/Streams/aelvor256-stand-in-2026-09-15.stream
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/replay-played.json
          -DSAME=replay:\ 32\ of\ 32\ answered\ identically,\ 30\ of\ 30\ days
          -DPLAYED=LogVaelenPlay:\ AELVOR\ 256\ seed\ 41454c564f52:\ played\ Dukem\ (person\ 15019,\ region\ 31)\ 30\ days,\ 32\ intents\ (30\ taken,\ 2\ refused\ by\ the\ world,\ 0\ dropped\ at\ the\ door),\ state\ cb69b72505aaa97d,\ log\ 34950d9a09dff0c4,\ life\ 51e2309a61765ff0,\ panel\ f4014af16e5e60f0
          -DVERBS=LogVaelenPlay:\ verbs\ work\ 8\ rest\ 9\ eat\ 9\ wait\ 1\ speak\ 2\ give\ 1\ take\ 1\ move\ 1
          -P ${PROJECT_SOURCE_DIR}/Tests/Run/ReplayPlayed.cmake)
# Its own line: line 25 applies 1800 to ${VAELEN_RUN_TEST_NAMES}, which is the
# VaelenRunTests suites, and this is not one of them. 1800 is the row's number
# and about 370x the 4.76 s a release replay measures here - the debug legs
# generate the same 256/100 world with assertions on, which is the multiple
# nobody had measured when the row was written.
set_tests_properties(Replay.Played PROPERTIES TIMEOUT 1800)

# ── Scheduling ───────────────────────────────────────────────────────────────
# See Tests/View/CMakeLists.txt: ctest orders by descending COST, and the long
# poles are declared so a leg's wall clock is not set by whichever one starts
# last. The numbers are relative, not seconds.
set_tests_properties(Run.Shuffled PROPERTIES COST 10000)
set_tests_properties(Run.Aelvor PROPERTIES COST 6000)
set_tests_properties(Replay.Played PROPERTIES COST 4000)
set_tests_properties(Run.Door PROPERTIES COST 3000)
