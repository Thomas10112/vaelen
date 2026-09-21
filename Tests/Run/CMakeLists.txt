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

# VAELEN - Phase 15 task 15.10, HEADLESS HALF.
#
# A stand-in WALK, written by VaelenAtlas --walk and replayed here. Not a month
# somebody played: a hundred days with the streaming cadence on, four takings,
# and a Looked record for every day - the shape the engine half will record for
# real, exercised and green before a human is available. 14.10's stand-in did
# that job twice over, and its refusal to write a month without a Move is how
# ADR-0139's defect was found; this one refused on its first run too, and what
# it refused turned out to be a clause no implementation could meet.
#
# --stream is passed through MORE: the cadence a world decides its detail on is
# the host's configuration, like --want-bound, so a replay is told it rather
# than reading it. Replay it without and the digests say so.
#
# THESE EXPECTATIONS ARE NOT THE REPLAY'S OWN OUTPUT ANY MORE. The first version
# of this entry took them from a replay, so it pinned the replay against itself
# and proved only that the replay is stable; a recording that diverged from it
# would have been written and this test would have been green over the top. Two
# real defects were under that, and the Phase 15 review found them. --walk now
# replays its own walk into a fresh world BEFORE writing a byte and refuses to
# write one whose state digest differs, so what is pinned here is a line the
# recording and the replay both produce.
#
# The walk is recorded with Attention::Most = 2 on purpose. The first one used
# 4, which is LodRules::MaxDetailed, which is what a replay that DROPPED the
# budget fell back to - so it round-tripped by numeric coincidence. A budget the
# default cannot stand in for is the whole point of pinning it.
add_test(NAME Replay.Walked
  COMMAND ${CMAKE_COMMAND}
          -DATLAS=$<TARGET_FILE:VaelenAtlas>
          -DSTREAM=${PROJECT_SOURCE_DIR}/Tests/Run/Streams/aelvor128-walk-2026-09-17.stream
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/replay-walked.json
          -DMORE=--stream
          # QUOTED, for the reason Replay.Played gives above.
          "-DSAME=replay: 0 of 0 answered identically, 100 of 100 days"
          "-DPLAYED=LogVaelenPlay: AELVOR 128 seed 41454c564f52: played person 4171 (person 4171, region 9) 100 days, 0 intents (0 taken, 0 refused by the world, 0 dropped at the door), state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd, panel fe2920e707995254"
          "-DVERBS=LogVaelenPlay: verbs work 0 rest 0 eat 0 wait 0 speak 0 give 0 take 0 move 0"
          -P ${CMAKE_CURRENT_LIST_DIR}/ReplayPlayed.cmake)
set_tests_properties(Replay.Walked PROPERTIES TIMEOUT 1800)

# 15.10's GATE, asked of the checked-in walk.
#
# Replay.Walked above pins what the walk replays TO; this pins that the walk
# still MEETS THE PHASE GATE - that the fence still fires, that both grains
# still agree on every day of it, that no day turn still promotes twice and
# that somewhere to walk still arrives. A change that leaves the digests alone
# and quietly stops pinning any region would pass Replay.Walked and fail here,
# which is the whole reason the two are separate tests.
#
# --want-bound 0 for the reason ReplayPlayed.cmake gives: the walk was recorded
# under the engine host's rules and a replay is told them, not sent to find
# them. The streaming cadence is NOT passed, because --gate forces it: a walk
# carrying Looked records is a walk recorded with it.
#
# The six clause lines are matched WHOLE, and --expect is passed so that clause
# (b)'s other half - the four digests - is judged rather than printed. The first
# version of this checked only the verdict, and a verdict is one boolean over
# six clauses: a clause that stopped being asked would still print "kept". The
# second matched substrings, which a drifting figure would still have satisfied.
add_test(NAME Run.Gate
  COMMAND ${CMAKE_COMMAND}
          -DATLAS=$<TARGET_FILE:VaelenAtlas>
          -DSTREAM=${PROJECT_SOURCE_DIR}/Tests/Run/Streams/aelvor128-walk-2026-09-17.stream
          "-DEXPECT=state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd, panel fe2920e707995254"
          "-DA=  (a) PASS  the stream carries looks and the fence fired: 100 Looked records, 53 pins published while held"
          "-DB=  (b) PASS  every record replayed as it was recorded: 0 wrong (0 of them takings), 0 records left unreached"
          "-DBB=  (B) PASS  the four digests are the ones the host printed: state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd, panel fe2920e707995254"
          "-DC=  (c) PASS  both grains agree on every day of the walk: worst over 100 days: heads 0, slots 0, faiths 0"
          "-DD=  (d) PASS  no day turn promoted twice: worst day turn promoted 1"
          "-DE=  (e) PASS  somewhere to walk within four days of each taking: 4 takings, longest wait 1 day turn(s)"
          -P ${CMAKE_CURRENT_LIST_DIR}/Gate.cmake)
set_tests_properties(Run.Gate PROPERTIES TIMEOUT 1800)

# 15.10's GATE, MET. The walk a person actually lived.
#
# Recorded on 2026-09-21 in UE 5.6 on Win64 (MSVC 19.51) by the owner, at the
# keyboard: 142 day turns, 138 looks, 4 takings and 2 Speak intents, with the
# daily cadence on. Replayed here on gcc/Linux to the same four digests.
#
# WHAT MAKES IT DIFFERENT from aelvor128-walk-2026-09-17.stream, which is why
# both are kept: that one is written by --walk, by this project, to exercise
# the clauses. This one came out of the engine, through the camera and the
# console, from a host that had never run headlessly. It is the only stream in
# this repository that proves the ENGINE half of 15.10 rather than the kernel.
#
# It also carries what a stand-in could not invent: two Speak intents the owner
# pressed by accident while reaching for W-A-S-D, a stretch where the camera
# looked at region 0 (off the map, over the sea) before they found their
# bearings, and 25 day turns where nothing moved at all. A gate that only ever
# saw tidy input would not be a gate.
add_test(NAME Replay.Lived
  COMMAND ${CMAKE_COMMAND}
          -DATLAS=$<TARGET_FILE:VaelenAtlas>
          -DSTREAM=${PROJECT_SOURCE_DIR}/Tests/Run/Streams/aelvor128-played-2026-09-21.stream
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/replay-lived.json
          -DMORE=--stream
          "-DSAME=replay: 2 of 2 answered identically, 142 of 142 days"
          "-DPLAYED=LogVaelenPlay: AELVOR 128 seed 41454c564f52: played person 4171 (person 4171, region 9) 142 days, 2 intents (2 taken, 0 refused by the world, 0 dropped at the door), state 609253a29361ec5f, log a2839792e0837328, life 0a4babc60f6e4d90, panel c4ddbe971538c59c"
          "-DVERBS=LogVaelenPlay: verbs work 0 rest 0 eat 0 wait 0 speak 2 give 0 take 0 move 0"
          -P ${CMAKE_CURRENT_LIST_DIR}/ReplayPlayed.cmake)
set_tests_properties(Replay.Lived PROPERTIES TIMEOUT 1800)

# And the gate itself over it, which is the phase's closing clause.
add_test(NAME Run.Gate.Lived
  COMMAND ${CMAKE_COMMAND}
          -DATLAS=$<TARGET_FILE:VaelenAtlas>
          -DSTREAM=${PROJECT_SOURCE_DIR}/Tests/Run/Streams/aelvor128-played-2026-09-21.stream
          "-DEXPECT=state 609253a29361ec5f, log a2839792e0837328, life 0a4babc60f6e4d90, panel c4ddbe971538c59c"
          "-DA=  (a) PASS  the stream carries looks and the fence fired: 138 Looked records, 15 pins published while held"
          "-DB=  (b) PASS  every record replayed as it was recorded: 0 wrong (0 of them takings), 0 records left unreached"
          "-DBB=  (B) PASS  the four digests are the ones the host printed: state 609253a29361ec5f, log a2839792e0837328, life 0a4babc60f6e4d90, panel c4ddbe971538c59c"
          "-DC=  (c) PASS  both grains agree on every day of the walk: worst over 142 days: heads 0, slots 0, faiths 0"
          "-DD=  (d) PASS  no day turn promoted twice: worst day turn promoted 1"
          "-DE=  (e) PASS  somewhere to walk within four days of each taking: 4 takings, longest wait 1 day turn(s)"
          -P ${CMAKE_CURRENT_LIST_DIR}/Gate.cmake)
set_tests_properties(Run.Gate.Lived PROPERTIES TIMEOUT 1800)
