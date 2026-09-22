# VAELEN - Phase 16 task 16.11: the control the provenance row specifies.
#
# The negative of ReplayPlayed.cmake. That driver replays a checked-in walk
# WITH the host's wiring and pins the four digests it reaches. This one
# replays the same walk with `Options{Play}` alone - which is what a reader
# that ignores the HOST section would build - and pins that it comes back
# WRONG, on the exact counts.
#
# Without this, 16.11 would carry the wiring in the file and have no evidence
# that carrying it matters: a STREAM and a HOST section that were never needed
# are four kilobytes of ceremony, and a test that only ever runs the correct
# route cannot tell the difference.
#
# Expected in: ATLAS, STREAM, OUT, WRONG (the counts line's middle), and RIGHT
# (a digest the CORRECT route reaches, which must be ABSENT here).
#
# A failing replay is checked for HOW it failed, not that it failed. Atlas
# exits non-zero for a stream of another world, an unreadable file and a bad
# argument alike, so exit code alone would pass against a typo in STREAM.
execute_process(
  COMMAND ${ATLAS} --replay ${STREAM} --panel --want-bound 0 --out ${OUT}
  RESULT_VARIABLE Ran
  OUTPUT_VARIABLE Said
  ERROR_VARIABLE Wrote)

string(REPLACE "\r" "" Said "${Said}")
string(REPLACE "\r" "" Wrote "${Wrote}")
set(All "${Said}${Wrote}")

# An expectation too short to hold what it claims to hold is a hard error, not
# a quietly weaker check - the lesson Tests/Run/ReplayPlayed.cmake learned when
# an unquoted -D cut a pinned line at its first parenthesis.
string(LENGTH "${WRONG}" WrongLen)
if(WrongLen LESS 30)
  message(FATAL_ERROR "WRONG is ${WrongLen} characters and cannot be the counts it claims: '${WRONG}'")
endif()
string(LENGTH "${RIGHT}" RightLen)
if(RightLen LESS 16)
  message(FATAL_ERROR "RIGHT is ${RightLen} characters and cannot be a digest: '${RIGHT}'")
endif()

if(Ran EQUAL 0)
  message(FATAL_ERROR "the replay reported SUCCESS without the wiring, which is the whole "
                      "thing this control exists to catch:\n${All}")
endif()

string(FIND "${All}" "${WRONG}" AtWrong)
if(AtWrong EQUAL -1)
  message(FATAL_ERROR "the replay failed, but not in the way it is pinned to.\nwanted: ${WRONG}\ngot:\n${All}")
endif()

string(FIND "${All}" "${RIGHT}" AtRight)
if(NOT AtRight EQUAL -1)
  message(FATAL_ERROR "the guessed route reached the digest the CORRECT route reaches, so this "
                      "control is measuring nothing:\n${RIGHT}")
endif()

message(STATUS "without the wiring: ${WRONG}")
