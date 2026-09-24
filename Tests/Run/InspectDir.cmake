# VAELEN - Phase 17 task 17.06, and 17.03's second-process half.
#
# Two processes each write a container into one directory; a THIRD, which
# wrote nothing, lists it through the store and must report both with the
# trailer each writer printed. Run.StoreColdProcess proves a second OBJECT
# lists what another wrote; this is a second PROCESS, which is what a save
# browser is.
#
# Expected in: ATLAS, DIR.
file(REMOVE_RECURSE "${DIR}")
file(MAKE_DIRECTORY "${DIR}")

set(Digests "")
foreach(Pair "first;1" "second;2")
  list(GET Pair 0 Name)
  list(GET Pair 1 Seed)
  execute_process(
    COMMAND ${ATLAS} --save-to ${DIR}/${Name} --size 32 --prehistory 10 --years 1 --seed ${Seed}
            --stream --lively --want-bound 0 --from-age 0 --to-age 45 --then-days 0
    RESULT_VARIABLE Ran OUTPUT_VARIABLE Said ERROR_VARIABLE Err)
  string(REPLACE "\r" "" Said "${Said}")
  if(NOT Ran EQUAL 0)
    message(FATAL_ERROR "the ${Name} writer failed (exit ${Ran}):\n${Said}${Err}")
  endif()
  string(REGEX MATCH "saved at ([0-9a-f]+)" _m "${Said}")
  set(Digest "${CMAKE_MATCH_1}")
  string(LENGTH "${Digest}" Len)
  if(NOT Len EQUAL 16)
    message(FATAL_ERROR "the ${Name} writer printed no digest:\n${Said}")
  endif()
  list(APPEND Digests "${Name}=${Digest}")
endforeach()

execute_process(COMMAND ${ATLAS} --inspect-dir ${DIR}
  RESULT_VARIABLE Ran OUTPUT_VARIABLE Listed ERROR_VARIABLE Err)
string(REPLACE "\r" "" Listed "${Listed}")
if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "--inspect-dir failed (exit ${Ran}):\n${Listed}${Err}")
endif()
string(FIND "${Listed}" "inspect-dir: 2 checkpoint(s) and 0 other file(s)" AtCount)
if(AtCount EQUAL -1)
  message(FATAL_ERROR "the cold process did not list two checkpoints:\n${Listed}")
endif()
foreach(Entry IN LISTS Digests)
  string(REPLACE "=" ";" Parts "${Entry}")
  list(GET Parts 0 Name)
  list(GET Parts 1 Digest)
  # The row must carry the NAME and the TRAILER the writer printed, on one
  # line, so a listing that had the names right and the digests by position
  # cannot pass. THREE sections: --save-to uses the two-argument build, which
  # carries no tape - the first version of this line said four, and the tool
  # was right and the expectation wrong.
  string(REGEX MATCH "\\| ${Name} \\| [0-9]+ \\| [0-9]+ \\| 2 \\| 3 \\| ${Digest} \\|" Row "${Listed}")
  if(Row STREQUAL "")
    message(FATAL_ERROR "${Name} is not listed with trailer ${Digest}, 3 sections and container v2:\n${Listed}")
  endif()
endforeach()
# And the two digests differ, or the check above compared one number twice.
list(GET Digests 0 A)
list(GET Digests 1 B)
string(REPLACE "first=" "" A "${A}")
string(REPLACE "second=" "" B "${B}")
if(A STREQUAL B)
  message(FATAL_ERROR "both writers printed the same digest ${A}; seeds 1 and 2 made the same world?")
endif()
message(STATUS "inspect-dir: a third process lists both containers with the trailers their writers printed")
