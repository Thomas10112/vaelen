# VAELEN - Phase 17 task 17.01's CONTROL: the corpus regenerates byte for byte.
#
# Run.Containers reads the three checked-in files and checks every recorded
# number against their bytes. It would go on passing on the day this build lost
# the ability to WRITE them - the files are in git and nothing in that test
# produces one. This entry runs the command the README records, into a scratch
# directory, and compares byte for byte.
#
# The two are separate entries because they make opposite claims. Run.Containers
# says a container is readable WITHOUT the simulation that wrote it, and it
# generates no world at all. This one generates three, which is the cost of
# saying the recorded command is still the command that makes these files.
#
# Expected in: ATLAS, CORPUS, SCRATCH.
file(REMOVE_RECURSE "${SCRATCH}")
file(MAKE_DIRECTORY "${SCRATCH}")

execute_process(
  COMMAND ${ATLAS} --containers ${SCRATCH}/
  RESULT_VARIABLE Ran OUTPUT_VARIABLE Said ERROR_VARIABLE Err)
string(REPLACE "\r" "" Said "${Said}")
if(NOT Ran EQUAL 0)
  message(FATAL_ERROR "--containers refused (exit ${Ran}):\n${Said}${Err}")
endif()

set(Names bare-16.container played-16.container full-32.container)
foreach(Name IN LISTS Names)
  if(NOT EXISTS "${CORPUS}/${Name}")
    message(FATAL_ERROR "the checked-in corpus has no ${Name}")
  endif()
  if(NOT EXISTS "${SCRATCH}/${Name}")
    message(FATAL_ERROR "--containers wrote no ${Name}:\n${Said}")
  endif()
  # Compared by HASH and not by size: two containers of the same length whose
  # bytes differ is exactly what a changed world, a changed walk or a changed
  # section order produces, and a size check would miss every one of them.
  file(SHA256 "${CORPUS}/${Name}" Checked)
  file(SHA256 "${SCRATCH}/${Name}" Fresh)
  if(NOT Checked STREQUAL Fresh)
    file(SIZE "${CORPUS}/${Name}" WasSize)
    file(SIZE "${SCRATCH}/${Name}" NowSize)
    message(FATAL_ERROR
      "${Name} no longer regenerates: checked-in ${WasSize} bytes ${Checked}, "
      "rebuilt ${NowSize} bytes ${Fresh}.\n"
      "If this is intended, rewrite the corpus AND Tests/Run/Containers/README.md AND "
      "the table in Tests/Run/Test_Containers.cpp in one commit - the three are one record.\n${Said}")
  endif()
endforeach()

# AND THE COMPARISON IS PUT ON TRIAL IN THE SAME RUN. A file comparison that
# always agreed - a hash of nothing, a path that resolved to the same file
# twice - would pass every clause above. The same comparison is asked about two
# files known to differ, and it must say so.
set(Probe "${SCRATCH}/not-a-container")
file(WRITE "${Probe}" "not a container")
file(SHA256 "${SCRATCH}/bare-16.container" Real)
file(SHA256 "${Probe}" Other)
if(Real STREQUAL Other)
  message(FATAL_ERROR "the comparison agrees about two files that differ - it is measuring nothing")
endif()

# 18.02: the kept older-format container is NOT regenerated, and it must not be
# what the tool writes today - if the two ever agree, the fifth HOST byte is
# gone and this driver would go on saying "3 of 3" over a corpus with no older
# instance in it.
if(NOT EXISTS "${CORPUS}/host24-16.container")
  message(FATAL_ERROR "the checked-in corpus has no host24-16.container (18.02's kept older-format instance)")
endif()
file(SHA256 "${CORPUS}/host24-16.container" Kept)
file(SHA256 "${SCRATCH}/played-16.container" Today)
if(Kept STREQUAL Today)
  message(FATAL_ERROR "host24-16.container is byte-identical to today's played-16.container: the HOST section no longer grew in 18.02")
endif()
message(STATUS "containers: 3 of 3 regenerate byte for byte from the recorded command, and the kept older one differs from today's")
