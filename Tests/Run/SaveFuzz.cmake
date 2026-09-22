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
execute_process(
  COMMAND ${ATLAS} --savefuzz ${STREAM} --points ${POINTS} --seed ${SEED} --stream --want-bound 0 ${MORE}
  RESULT_VARIABLE Ran
  OUTPUT_VARIABLE Said
  ERROR_VARIABLE Wrote)

string(REPLACE "\r" "" Said "${Said}")
string(REPLACE "\r" "" Wrote "${Wrote}")
set(All "${Said}${Wrote}")

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
