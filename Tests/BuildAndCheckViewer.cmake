# VAELEN - builds the viewer page from a world the atlas wrote, then checks it.
#
# Two steps in one CTest entry because the second is only meaningful on the
# output of the first: a sound template can still be inlined into a broken page.
execute_process(
  COMMAND ${PYTHON} ${ROOT}/Tools/Viewer/build_viewer.py ${ATLAS} --out ${OUT}
  RESULT_VARIABLE Built OUTPUT_VARIABLE Log ERROR_VARIABLE Log)
if(NOT Built EQUAL 0)
  message(FATAL_ERROR "building the page failed:\n${Log}")
endif()
message(STATUS "${Log}")
execute_process(
  COMMAND ${PYTHON} ${ROOT}/Tools/check_viewer.py ${OUT}
  RESULT_VARIABLE Checked OUTPUT_VARIABLE Log ERROR_VARIABLE Log)
if(NOT Checked EQUAL 0)
  message(FATAL_ERROR "the built page is not sound:\n${Log}")
endif()
message(STATUS "${Log}")
