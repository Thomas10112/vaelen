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
# Expected in: ATLAS, STREAM, OUT, and the four lines below as PLAYED/VERBS/SAME.

execute_process(
  COMMAND ${ATLAS} --replay ${STREAM} --panel --want-bound 0 --out ${OUT}
  RESULT_VARIABLE Ran
  OUTPUT_VARIABLE Said
  ERROR_VARIABLE Wrote)

if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "the replay did not run clean (exit ${Ran}):\n${Said}\n${Wrote}")
endif()

# Windows writes \r\n on a text stdout; the expected lines below have neither.
string(REPLACE "\r" "" Said "${Said}")

foreach(Line IN ITEMS "${SAME}" "${PLAYED}" "${VERBS}")
  string(FIND "${Said}" "${Line}" At)
  if(At EQUAL -1)
    message(FATAL_ERROR
      "the replay did not print\n  ${Line}\nit printed:\n${Said}")
  endif()
endforeach()

message(STATUS "Replay.Played: ${SAME}")
