# VAELEN - Phase 16 gate, clause (k): two PROCESSES, one file.
#
# Every other save test in this tree holds both worlds in one address space.
# That proves the format round-trips in memory; it does not prove the bytes
# which reached a DISK are a world, nor that a process which never generated
# anything can pick them up and go on. This entry is the only one that does.
#
# Run.Store covers the other half of the clause - the atomic write, and a full
# disk refusing without destroying what was already there.
#
# Expected in: ATLAS, FILE, DAYS.
execute_process(
  COMMAND ${ATLAS} --save-to ${FILE} --size 64 --prehistory 20 --years 10
          --stream --lively --want-bound 0 --then-days ${DAYS}
  RESULT_VARIABLE SaveRan OUTPUT_VARIABLE Saved ERROR_VARIABLE SaveErr)
string(REPLACE "\r" "" Saved "${Saved}")
if(NOT SaveRan EQUAL 0)
  message(FATAL_ERROR "the first process could not save (exit ${SaveRan}):\n${Saved}${SaveErr}")
endif()

execute_process(
  COMMAND ${ATLAS} --load-from ${FILE} --size 64 --prehistory 20 --years 10
          --stream --lively --want-bound 0 --then-days ${DAYS}
  RESULT_VARIABLE LoadRan OUTPUT_VARIABLE Loaded ERROR_VARIABLE LoadErr)
string(REPLACE "\r" "" Loaded "${Loaded}")
if(NOT LoadRan EQUAL 0)
  message(FATAL_ERROR "the second process could not load (exit ${LoadRan}):\n${Loaded}${LoadErr}")
endif()

# EVERY DIGEST IS REQUIRED TO HAVE BEEN FOUND. A regex that matches nothing
# leaves its variable empty, and two empty strings compare equal - which is the
# shape of a test that passes because it measured nothing.
string(REGEX MATCH "saved at ([0-9a-f]+)" _m "${Saved}")
set(SavedAt "${CMAKE_MATCH_1}")
string(REGEX MATCH "one process reach ([0-9a-f]+)" _m "${Saved}")
set(OneProcess "${CMAKE_MATCH_1}")
string(REGEX MATCH "adopted at ([0-9a-f]+)" _m "${Loaded}")
set(AdoptedAt "${CMAKE_MATCH_1}")
string(REGEX MATCH "second process reach ([0-9a-f]+)" _m "${Loaded}")
set(TwoProcess "${CMAKE_MATCH_1}")
string(REGEX MATCH "Generations ([0-9]+)" _m "${Loaded}")
set(Generations "${CMAKE_MATCH_1}")

foreach(Pair "SavedAt;${SavedAt}" "OneProcess;${OneProcess}" "AdoptedAt;${AdoptedAt}" "TwoProcess;${TwoProcess}")
  list(GET Pair 0 Name)
  list(LENGTH Pair Parts)
  if(Parts LESS 2)
    message(FATAL_ERROR "${Name} was not printed at all:\n${Saved}\n${Loaded}")
  endif()
  list(GET Pair 1 Value)
  string(LENGTH "${Value}" Len)
  if(NOT Len EQUAL 16)
    message(FATAL_ERROR "${Name} is '${Value}' (${Len} chars) and cannot be a digest:\n${Saved}\n${Loaded}")
  endif()
endforeach()

if(NOT SavedAt STREQUAL AdoptedAt)
  message(FATAL_ERROR "the second process adopted a different world: ${AdoptedAt}, saved ${SavedAt}")
endif()
if(NOT OneProcess STREQUAL TwoProcess)
  message(FATAL_ERROR "${DAYS} days on, the two processes part: ${TwoProcess} against ${OneProcess}")
endif()
if(NOT Generations STREQUAL "0")
  message(FATAL_ERROR "the loading process generated a world (${Generations}) instead of adopting one")
endif()

message(STATUS "two processes, one file: adopted ${AdoptedAt}, ${DAYS} days on both reach ${TwoProcess}")
