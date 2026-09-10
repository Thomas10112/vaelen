# VAELEN tests (headless). One executable per kernel module; one CTest entry
# per suite (= per Test_<Suite>.cpp file) so failures are attributed precisely.
add_subdirectory(Core)
add_subdirectory(Sim)
add_subdirectory(Population)
add_subdirectory(Society)
add_subdirectory(Economy)
add_subdirectory(Politics)
add_subdirectory(Military)
add_subdirectory(Infrastructure)
add_subdirectory(Colony)
add_subdirectory(Player)
add_subdirectory(Gameplay)
add_subdirectory(View)

# Kernel purity: no engine headers, exceptions, RTTI or non-deterministic
# randomness inside engine-agnostic modules. Python 3 is a hard requirement:
# a silently missing check would be a fake green.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME Kernel.Purity
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_kernel_purity.py --root ${PROJECT_SOURCE_DIR})
  # The checker's own regression suite: proves every rule still fires.
  add_test(NAME Kernel.PuritySelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_kernel_purity.py --self-test)
  set_tests_properties(Kernel.Purity Kernel.PuritySelfTest PROPERTIES TIMEOUT 120)
elseif(VAELEN_REQUIRE_PURITY_CHECK)
  message(FATAL_ERROR "Python 3 is required for the Kernel.Purity test. Install it or configure with -DVAELEN_REQUIRE_PURITY_CHECK=OFF (the purity check is then NOT run).")
else()
  message(WARNING "Python3 not found: Kernel.Purity test is NOT registered (VAELEN_REQUIRE_PURITY_CHECK=OFF)")
endif()

# ── The atlas tool ───────────────────────────────────────────────────────────
# Compiling proves nothing about a tool. These run it: once to see it produce a
# world, twice to see it produce the SAME world, and the checker reads the file
# back rather than trusting an exit code (Tools/check_atlas_output.py).
set(VAELEN_ATLAS_A ${CMAKE_CURRENT_BINARY_DIR}/atlas-run-a.json)
set(VAELEN_ATLAS_B ${CMAKE_CURRENT_BINARY_DIR}/atlas-run-b.json)
# --every exercises the timeline path, and --colony the mining one, so CI runs
# the tool the way it is actually used rather than only its simplest shape.
add_test(NAME Atlas.Runs
  COMMAND VaelenAtlas --size 48 --prehistory 30 --years 10 --every 5 --colony --why --out ${VAELEN_ATLAS_A})
add_test(NAME Atlas.RunsAgain
  COMMAND VaelenAtlas --size 48 --prehistory 30 --years 10 --every 5 --colony --why --out ${VAELEN_ATLAS_B})
set_tests_properties(Atlas.Runs PROPERTIES FIXTURES_SETUP AtlasA TIMEOUT 900)
set_tests_properties(Atlas.RunsAgain PROPERTIES FIXTURES_SETUP AtlasB TIMEOUT 900)

if(Python3_Interpreter_FOUND)
  add_test(NAME Atlas.Output
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py ${VAELEN_ATLAS_A})
  set_tests_properties(Atlas.Output PROPERTIES FIXTURES_REQUIRED AtlasA TIMEOUT 300)
  add_test(NAME Atlas.Deterministic
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py
            --same ${VAELEN_ATLAS_A} ${VAELEN_ATLAS_B})
  set_tests_properties(Atlas.Deterministic PROPERTIES FIXTURES_REQUIRED "AtlasA;AtlasB" TIMEOUT 300)
  # The checker's own regression suite: proves every rule still fires.
  add_test(NAME Atlas.OutputSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py --self-test)
  set_tests_properties(Atlas.OutputSelfTest PROPERTIES TIMEOUT 120)

  # ── The viewer ─────────────────────────────────────────────────────────────
  # A thousand lines of page that nothing compiles. Tools/check_viewer.py reads
  # it without a browser: the script parses, every element it reaches for is in
  # the markup, every control names a key the script knows, nothing it RENDERS
  # is non-ASCII, and every CSS token it uses is defined on bare :root. Those
  # are the classes that actually broke it on the day it was written.
  add_test(NAME Viewer.Page
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_viewer.py
            ${PROJECT_SOURCE_DIR}/Tools/Viewer/Atlas.html)
  set_tests_properties(Viewer.Page PROPERTIES TIMEOUT 120)
  add_test(NAME Viewer.SelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_viewer.py --self-test)
  set_tests_properties(Viewer.SelfTest PROPERTIES TIMEOUT 120)

  # End to end: the world the tool wrote, inlined into the page, checked again.
  # The template can be sound and the build step still produce something broken.
  add_test(NAME Viewer.Built
    COMMAND ${CMAKE_COMMAND} -DPYTHON=${Python3_EXECUTABLE} -DROOT=${PROJECT_SOURCE_DIR}
            -DATLAS=${VAELEN_ATLAS_A} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/atlas-page.html
            -P ${PROJECT_SOURCE_DIR}/Tests/BuildAndCheckViewer.cmake)
  set_tests_properties(Viewer.Built PROPERTIES FIXTURES_REQUIRED AtlasA TIMEOUT 300)
elseif(VAELEN_REQUIRE_PURITY_CHECK)
  message(FATAL_ERROR "Python 3 is required to check what VaelenAtlas writes.")
else()
  message(WARNING "Python3 not found: the Atlas.Output tests are NOT registered")
endif()
