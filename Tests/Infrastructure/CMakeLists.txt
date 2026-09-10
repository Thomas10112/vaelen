# VaelenInfrastructure tests: Tests/Infrastructure/Test_<Suite>.cpp -> CTest entry Infrastructure.<Suite>.
file(GLOB VAELEN_INFRASTRUCTURE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenInfrastructureTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_INFRASTRUCTURE_TEST_SOURCES})

target_include_directories(VaelenInfrastructureTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenInfrastructureTests PRIVATE Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_INFRASTRUCTURE_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_INFRASTRUCTURE_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Infrastructure.${SuiteName} COMMAND VaelenInfrastructureTests --suite ${SuiteName})
  list(APPEND VAELEN_INFRASTRUCTURE_TEST_NAMES Infrastructure.${SuiteName})
endforeach()

add_test(NAME Infrastructure.Registry COMMAND VaelenInfrastructureTests --check-registry)
add_test(NAME Infrastructure.Shuffled COMMAND VaelenInfrastructureTests --shuffle 1)
list(APPEND VAELEN_INFRASTRUCTURE_TEST_NAMES Infrastructure.Registry Infrastructure.Shuffled)

set_tests_properties(${VAELEN_INFRASTRUCTURE_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Infrastructure.Shuffled PROPERTIES TIMEOUT 5400)
# Five centuries at 256 with every Phase 04 to 09 system running.
set_tests_properties(Infrastructure.InfrastructureGate PROPERTIES TIMEOUT 5400)
# Decay and WorksHistory grow a world for centuries before they measure anything,
# which is about two minutes on the Linux debug preset and past the 300 s default
# on a loaded Windows runner - both timed out there on the first full matrix run
# Phase 10 was allowed to finish. The work is the same; the machine is slower.
set_tests_properties(Infrastructure.Decay PROPERTIES TIMEOUT 1800)
set_tests_properties(Infrastructure.WorksHistory PROPERTIES TIMEOUT 1800)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Infrastructure.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Infrastructure.Shuffled PROPERTIES COST 10000)
set_tests_properties(Infrastructure.InfrastructureGate PROPERTIES COST 5000)
