# VAELEN - 15.10's gate, run over a walk and matched line by line.
#
# A separate script from ReplayPlayed.cmake rather than a flag on it, because
# the two ask different questions of the same file: that one asks what the walk
# replays to, this one asks whether the walk still meets the phase gate. Sharing
# a driver would have meant a driver that does neither plainly.
#
# Expected in: ATLAS, STREAM, EXPECT (the four digests), and the clause lines as
# A, B, BB, C, D, E. Clause (f) is the frozen constants, which are the rest of
# this suite.
if(NOT DEFINED ATLAS OR NOT DEFINED STREAM OR NOT DEFINED EXPECT)
  message(FATAL_ERROR "Gate.cmake needs ATLAS, STREAM and EXPECT")
endif()

# 18.10: the recorded months belong to the world before Phase 18 - a stream
# carries no Options - so they are replayed into it unless the entry says
# otherwise (-DERA=--climate for a month recorded in a climate world).
if(NOT DEFINED ERA)
  set(ERA --no-climate)
endif()
execute_process(
  COMMAND ${ATLAS} --gate ${STREAM} --want-bound 0 ${ERA} --expect ${EXPECT}
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE CODE)

if(NOT CODE EQUAL 0)
  message(FATAL_ERROR "the gate refused the walk (exit ${CODE}):\n${OUT}${ERR}")
endif()

# WHOLE LINES. The first version of this used string(FIND), which is a substring
# match: "(a) PASS ... 100 Looked records, 53 pins" would have been kept by a
# build that printed that and then appended anything at all, and a clause whose
# figures drifted would still have matched a prefix. The gate's whole value is
# that its numbers are the measurement, so the numbers are matched exactly.
string(REPLACE "\n" ";" LINES "${OUT}")
foreach(WHICH A B BB C D E)
  if(NOT DEFINED ${WHICH})
    message(FATAL_ERROR "Gate.cmake was not given clause ${WHICH}")
  endif()
  set(FOUND FALSE)
  foreach(LINE IN LISTS LINES)
    string(REGEX REPLACE "\r$" "" LINE "${LINE}")
    if(LINE STREQUAL "${${WHICH}}")
      set(FOUND TRUE)
      break()
    endif()
  endforeach()
  if(NOT FOUND)
    message(FATAL_ERROR "clause ${WHICH} is not a line the gate printed.\nwanted: ${${WHICH}}\ngot:\n${OUT}")
  endif()
endforeach()

message(STATUS "the gate holds over the walk, six clause lines matched whole")
