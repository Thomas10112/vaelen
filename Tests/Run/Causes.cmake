# VAELEN - Phase 17 task 17.05: the census over the checked-in corpus, pinned.
#
# Every line the roadmap records for these three containers is required here,
# whole. A figure that drifts - a publisher that starts or stops passing a
# cause, a container regenerated from a different walk - moves a pinned line
# and this entry says which.
#
# AND NO ROW MAY PRINT A QUESTION MARK. `--causes` is the first consumer of
# 17.02's name table, and over a world this build wrote every type it logs is a
# type this build declares; a `?<hex>` in the table means the generator and the
# world disagree, which is the exact failure 17.02's Run.EventTypes exists to
# catch and this is its second witness.
#
# Expected in: ATLAS, FILE, and HEADLINE, DEPTH (whole lines, quoted).
execute_process(
  COMMAND ${ATLAS} --causes ${FILE}
  RESULT_VARIABLE Ran OUTPUT_VARIABLE Said ERROR_VARIABLE Err)
string(REPLACE "\r" "" Said "${Said}")
if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "--causes refused (exit ${Ran}):\n${Said}${Err}")
endif()

foreach(Pair "HEADLINE;${HEADLINE}" "DEPTH;${DEPTH}")
  list(GET Pair 0 Name)
  list(LENGTH Pair Parts)
  if(Parts LESS 2)
    message(FATAL_ERROR "${Name} was not given")
  endif()
  list(GET Pair 1 Want)
  string(LENGTH "${Want}" Len)
  # A short expectation is an expectation that matches by accident.
  if(Len LESS 30)
    message(FATAL_ERROR "${Name} is '${Want}' (${Len} chars) and too short to pin anything")
  endif()
  string(FIND "${Said}" "${Want}" At)
  if(At EQUAL -1)
    message(FATAL_ERROR "${Name} not printed.\nwanted: ${Want}\ngot:\n${Said}")
  endif()
endforeach()

string(FIND "${Said}" "| ?" Unknown)
if(NOT Unknown EQUAL -1)
  message(FATAL_ERROR "a type this world logged is not in the name table:\n${Said}")
endif()

# The table has a header and at least one row - a census that printed its
# headline and no table would pass the clauses above.
string(REGEX MATCHALL "\n\\| [A-Za-z]+ \\| [0-9]+ \\| [0-9]+ \\| [0-9.]+% \\|" Rows "${Said}")
list(LENGTH Rows RowCount)
if(RowCount LESS 10)
  message(FATAL_ERROR "only ${RowCount} type row(s) printed:\n${Said}")
endif()

message(STATUS "causes: pinned over ${FILE}, ${RowCount} type rows, none unknown")
