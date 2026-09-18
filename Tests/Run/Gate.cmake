# VAELEN - 15.10's gate, run over a walk and matched line by line.
#
# A separate script from ReplayPlayed.cmake rather than a flag on it, because
# the two ask different questions of the same file: that one asks what the walk
# replays to, this one asks whether the walk still meets the phase gate. Sharing
# a driver would have meant a driver that does neither plainly.
#
# Expected in: ATLAS, STREAM, and the five clause lines as A..E. Clause (f) is
# the frozen constants, which are the rest of this suite.
if(NOT DEFINED ATLAS OR NOT DEFINED STREAM)
  message(FATAL_ERROR "Gate.cmake needs ATLAS and STREAM")
endif()

execute_process(
  COMMAND ${ATLAS} --gate ${STREAM} --want-bound 0
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE CODE)

if(NOT CODE EQUAL 0)
  message(FATAL_ERROR "the gate refused the walk (exit ${CODE}):\n${OUT}${ERR}")
endif()

# Whole lines, and each one named in the failure. A substring match on "(a)
# PASS" would be kept by a build that printed the clause and measured nothing.
foreach(WHICH A B C D E)
  if(NOT DEFINED ${WHICH})
    message(FATAL_ERROR "Gate.cmake was not given clause ${WHICH}")
  endif()
  string(FIND "${OUT}" "${${WHICH}}" AT)
  if(AT EQUAL -1)
    message(FATAL_ERROR "clause ${WHICH} is not what the gate printed.\nwanted: ${${WHICH}}\ngot:\n${OUT}")
  endif()
endforeach()

message(STATUS "the gate holds over the walk, five clauses matched whole")
