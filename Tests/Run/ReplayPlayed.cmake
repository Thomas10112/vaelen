# VAELEN - Phase 14 task 14.10: the month replayed by no hand.
#
# Runs Tools/Atlas over the checked-in stream and holds BOTH halves of what a
# replay must prove: the exit code, which is where RunReplay reports a stream
# of another world or any answer that differed (Main.cpp), and the three lines
# it prints, which carry the four digests and the counts.
#
# A -P driver rather than PASS_REGULAR_EXPRESSION on the add_test, because in
# CMake a passing regex passes the test ALONE and never looks at the exit code
# (the same lesson Tests/CMakeLists.txt learned at Atlas.PanelEmpty, where one
# entry became two). Here one entry holds both.
#
# Expected in: ATLAS, STREAM, OUT, and the three lines as SAME/PLAYED/VERBS.
#
# Every expectation is checked for LENGTH before it is used. The 14.10 review
# found the add_test passing -DPLAYED unquoted, so CMake cut the value at its
# first parenthesis and this script was handed "...played Dukem" - a prefix
# every wrong answer also contains. The test passed against a stub printing
# fabricated digests. A pinned test that cannot fail is worse than no test, so
# an expectation too short to hold what it claims to hold is a hard error here
# rather than a quietly weaker check.

# MORE carries whatever else the host's configuration was, unset for a stream
# that needs none. 15.10's walk needs --stream: the cadence a world decides its
# detail on is the host's, not the stream's, exactly as --want-bound is.
# 18.10: the recorded months belong to the world before Phase 18 - a stream
# carries no Options - so they are replayed into it unless the entry says
# otherwise (-DERA=--climate for a month recorded in a climate world).
if(NOT DEFINED ERA)
  set(ERA --no-climate)
endif()
# 19.09: an entry that pins a scene line asks the replay for the scene.
if(DEFINED TERRAIN OR DEFINED LAYOUT OR DEFINED SKY)
  set(SCENE --scene)
endif()
execute_process(
  COMMAND ${ATLAS} --replay ${STREAM} --panel --want-bound 0 ${ERA} ${MORE} ${SCENE} --out ${OUT}
  RESULT_VARIABLE Ran
  OUTPUT_VARIABLE Said
  ERROR_VARIABLE Wrote)

if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "the replay did not run clean (exit ${Ran}):\n${Said}\n${Wrote}")
endif()

# Windows writes \r\n on a text stdout; the expected lines below have neither.
string(REPLACE "\r" "" Said "${Said}")

# The digests are 16 hex characters each and the played line carries four of
# them, so a PLAYED shorter than this cannot be the whole line whatever it says.
foreach(Pair IN ITEMS "SAME;35" "PLAYED;200" "VERBS;60")
  list(GET Pair 0 Name)
  list(GET Pair 1 Least)
  string(LENGTH "${${Name}}" Long)
  if(Long LESS Least)
    message(FATAL_ERROR
      "${Name} reached this script as ${Long} characters, fewer than the ${Least} "
      "it must have. The add_test is passing it wrong - most likely an unquoted "
      "argument cut at a parenthesis. Value: [${${Name}}]")
  endif()
endforeach()

# 19.04: a replay of a climate world also says its weather, in the words the
# engine's Vaelen.Play will print (Vaelen/View/Proof.h). Optional, because the
# months of the world before print no such line and must not start to.
set(Expected "${SAME}" "${PLAYED}" "${VERBS}")
if(DEFINED CLIMATE)
  string(LENGTH "${CLIMATE}" Long)
  if(Long LESS 200)
    message(FATAL_ERROR "CLIMATE reached this script as ${Long} characters, fewer than the 200 a whole "
                        "LogVaelenClimate line has - most likely an unquoted argument. Value: [${CLIMATE}]")
  endif()
  list(APPEND Expected "${CLIMATE}")
endif()
# 19.09: the scene the replay came to - the ground, what stands on it and the
# day's sky over the played life - in the three lines the engine prints.
foreach(Pair IN ITEMS "TERRAIN;120" "LAYOUT;120" "SKY;120")
  list(GET Pair 0 Name)
  list(GET Pair 1 Least)
  if(DEFINED ${Name})
    string(LENGTH "${${Name}}" Long)
    if(Long LESS Least)
      message(FATAL_ERROR "${Name} reached this script as ${Long} characters, fewer than the ${Least} a whole "
                          "LogVaelenScene line has - most likely an unquoted argument. Value: [${${Name}}]")
    endif()
    list(APPEND Expected "${${Name}}")
  endif()
endforeach()
foreach(Line IN LISTS Expected)
  string(FIND "${Said}" "${Line}" At)
  if(At EQUAL -1)
    message(FATAL_ERROR
      "the replay did not print\n  ${Line}\nit printed:\n${Said}")
  endif()
endforeach()

message(STATUS "Replay.Played: ${SAME}")
