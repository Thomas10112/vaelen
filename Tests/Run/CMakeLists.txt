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
# THE STREAM IS A MONTH SOMEBODY PLAYED. Eighty-three days at the keyboard in
# UE 5.6 on 2026-09-16, written out by Vaelen.Stream.Write. The three lines
# below are what the ENGINE printed that day, and this entry passes only when a
# headless replay prints the same bytes - which is the whole claim of ADR-0136
# and the reason Phase 14 has a gate at all. The stand-in that stood here until
# the month arrived is gone; Tests/Run/Streams/README.md keeps its story.
add_test(NAME Replay.Played
  COMMAND ${CMAKE_COMMAND}
          -DATLAS=$<TARGET_FILE:VaelenAtlas>
          -DSTREAM=${PROJECT_SOURCE_DIR}/Tests/Run/Streams/aelvor256-2026-09-16.stream
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/replay-played.json
          # QUOTED, every one of them. An unquoted CMake argument may not
          # contain a parenthesis: it ends the argument there. Written with
          # backslash-escaped spaces and bare ( ), this line reached the driver
          # as EIGHT arguments of which only the first was a -D assignment, so
          # PLAYED was "...played Dukem" and the four digests were pinned by
          # nothing at all. The test passed on a fabricated line. Found by the
          # 14.10 review; the driver now also refuses an expectation too short
          # to contain a digest, so the same mistake cannot be silent twice.
          "-DSAME=replay: 99 of 99 answered identically, 83 of 83 days"
          "-DPLAYED=LogVaelenPlay: AELVOR 256 seed 41454c564f52: played Dukem (person 15019, region 37) 83 days, 99 intents (76 taken, 23 refused by the world, 0 dropped at the door), state 2ffed5236a593c1b, log 1fdd4211695649bc, life 654e2de6455d8b7a, panel 7de2c5faf3cc1813"
          "-DVERBS=LogVaelenPlay: verbs work 34 rest 1 eat 2 wait 2 speak 5 give 2 take 26 move 27"
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
