# VAELEN - Phase 17 task 17.06: the inspector over the checked-in corpus.
#
# Every line given in WANT_* is required WHOLE. A figure that drifts moves a
# line and this entry says which. And the two refusals the gate asks for live
# here too, because an inspector that describes a truncated file in part, or
# that says BadMagic and nothing more when handed the inner format, is an
# inspector that misleads exactly when it is needed.
#
# Expected in: ATLAS, FILE, and either WANT_HEADER + WANT_TRAILER + WANT_LAST
# (a container) or REFUSE=image | REFUSE=truncated with SCRATCH.
if(REFUSE STREQUAL "image")
  execute_process(COMMAND ${ATLAS} --inspect ${FILE}
    RESULT_VARIABLE Ran OUTPUT_VARIABLE Said ERROR_VARIABLE Err)
  if(Ran EQUAL 0)
    message(FATAL_ERROR "an IMAGE was inspected as if it were a container:\n${Said}")
  endif()
  string(FIND "${Err}" "BadMagic" AtMagic)
  string(FIND "${Err}" "save IMAGE, the inner format" AtImage)
  if(AtMagic EQUAL -1 OR AtImage EQUAL -1)
    message(FATAL_ERROR "the refusal does not name the format it found:\n${Err}")
  endif()
  string(FIND "${Said}" "| STATE" AtRow)
  if(NOT AtRow EQUAL -1)
    message(FATAL_ERROR "a refused file still had a section row printed:\n${Said}")
  endif()
  message(STATUS "inspect: an image is refused by name")
  return()
endif()

if(REFUSE STREQUAL "truncated")
  file(READ "${FILE}" Whole HEX)
  # 4 KiB of hex is 8192 characters: enough to hold the header and the whole
  # section table, so the reader has everything it needs to START describing
  # the file and must refuse anyway.
  string(SUBSTRING "${Whole}" 0 8192 Head)
  # CMake cannot write raw bytes from a hex string, so the cut is made by
  # copying the file and truncating it with the one tool every leg has.
  set(Cut "${SCRATCH}/truncated-4k.container")
  file(COPY_FILE "${FILE}" "${Cut}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env python3 -c
    "import sys; p=sys.argv[1]; b=open(p,'rb').read()[:4096]; open(p,'wb').write(b)" "${Cut}"
    RESULT_VARIABLE CutRan OUTPUT_VARIABLE CutSaid ERROR_VARIABLE CutErr)
  if(NOT CutRan EQUAL 0)
    message(FATAL_ERROR "could not truncate the copy: ${CutSaid}${CutErr}")
  endif()
  file(SIZE "${Cut}" CutSize)
  if(NOT CutSize EQUAL 4096)
    message(FATAL_ERROR "the copy is ${CutSize} bytes, not 4096; the cut did not happen")
  endif()
  execute_process(COMMAND ${ATLAS} --inspect ${Cut}
    RESULT_VARIABLE Ran OUTPUT_VARIABLE Said ERROR_VARIABLE Err)
  if(Ran EQUAL 0)
    message(FATAL_ERROR "a container cut at 4 KiB was described as if whole:\n${Said}")
  endif()
  string(FIND "${Said}" "| STATE" AtRow)
  string(FIND "${Said}" "inspect: container v" AtHeader)
  if(NOT AtRow EQUAL -1 OR NOT AtHeader EQUAL -1)
    message(FATAL_ERROR "a refused file was described in part:\n${Said}")
  endif()
  string(FIND "${Err}" "bytes on disk" AtWhere)
  if(AtWhere EQUAL -1)
    message(FATAL_ERROR "the refusal does not say where it stopped:\n${Err}")
  endif()
  message(STATUS "inspect: a 4 KiB truncation is refused whole, not described in part")
  return()
endif()

execute_process(COMMAND ${ATLAS} --inspect ${FILE}
  RESULT_VARIABLE Ran OUTPUT_VARIABLE Said ERROR_VARIABLE Err)
string(REPLACE "\r" "" Said "${Said}")
if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "--inspect refused (exit ${Ran}):\n${Said}${Err}")
endif()
foreach(Pair "WANT_HEADER;${WANT_HEADER}" "WANT_TRAILER;${WANT_TRAILER}" "WANT_LAST;${WANT_LAST}")
  list(GET Pair 0 Name)
  list(LENGTH Pair Parts)
  if(Parts LESS 2)
    message(FATAL_ERROR "${Name} was not given")
  endif()
  list(GET Pair 1 Want)
  string(LENGTH "${Want}" Len)
  if(Len LESS 30)
    message(FATAL_ERROR "${Name} is '${Want}' (${Len} chars) and too short to pin anything")
  endif()
  string(FIND "${Said}" "${Want}" At)
  if(At EQUAL -1)
    message(FATAL_ERROR "${Name} not printed.\nwanted: ${Want}\ngot:\n${Said}")
  endif()
endforeach()
message(STATUS "inspect: pinned over ${FILE}")
