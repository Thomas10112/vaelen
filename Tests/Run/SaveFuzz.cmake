# VAELEN - Phase 16 task 16.12(b): the save fuzzer, driven by CTest.
#
# `Tools/Atlas --savefuzz` already judges itself - it returns non-zero when a
# save point fails to be a fixed point, to re-adopt, to refuse a second adopt,
# or to lead where the uninterrupted walk led, and its two controls return
# non-zero when they measure nothing. So this driver holds the exit code AND
# one pinned line, for the reason Tests/Run/ReplayPlayed.cmake gives: in CMake
# a passing PASS_REGULAR_EXPRESSION passes the test ALONE and never looks at
# the exit code.
#
# Expected in: ATLAS, STREAM, MORE (the flags under test) and WANT (a line the
# run must print).
# 18.10: the recorded months belong to the world before Phase 18 - a stream
# carries no Options - so they are replayed into it unless the entry says
# otherwise (-DERA=--climate for a month recorded in a climate world).
if(NOT DEFINED ERA)
  set(ERA --no-climate)
endif()
execute_process(
  COMMAND ${ATLAS} --savefuzz ${STREAM} --points ${POINTS} --seed ${SEED} --stream --want-bound 0 ${ERA}
          --expect-state ${EXPECT} ${MORE}
  RESULT_VARIABLE Ran
  OUTPUT_VARIABLE Said
  ERROR_VARIABLE Wrote)

string(REPLACE "\r" "" Said "${Said}")
string(REPLACE "\r" "" Wrote "${Wrote}")
set(All "${Said}${Wrote}")

# The pinned uninterrupted digest is what makes this a check against the RECORD
# rather than a check of the tool against itself. Sixteen hex digits or it is
# not a digest.
string(LENGTH "${EXPECT}" ExpectLen)
if(NOT ExpectLen EQUAL 16)
  message(FATAL_ERROR "EXPECT is ${ExpectLen} characters and cannot be a state digest: '${EXPECT}'")
endif()

string(LENGTH "${WANT}" WantLen)
if(WantLen LESS 20)
  message(FATAL_ERROR "WANT is ${WantLen} characters and cannot be the line it claims: '${WANT}'")
endif()

if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "savefuzz did not come back clean (exit ${Ran}):\n${All}")
endif()

string(FIND "${All}" "${WANT}" At)
if(At EQUAL -1)
  message(FATAL_ERROR "savefuzz came back clean but did not say what it is pinned to say.\nwanted: ${WANT}\ngot:\n${All}")
endif()

message(STATUS "savefuzz: ${WANT}")
