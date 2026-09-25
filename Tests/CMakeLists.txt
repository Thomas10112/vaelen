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
add_subdirectory(Run)

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
  # The same world is wired in four places - the headless atlas, the game
  # module's actor, the presentation actor and, since 14.03, Run::Aelvor, the
  # wiring a host is meant to use - and ADR-0135 is what happened when they
  # drifted: the engine drew a world in which nobody was ever in bondage, and
  # nothing said so for as long as nobody cross-checked.
  #
  # A CTest entry rather than a CI-only step, so that it runs on every leg and
  # on every developer's machine, like the purity check it sits beside. It reads
  # four source files and needs no compiler of its own.
  add_test(NAME Kernel.WorldWiring
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_world_wiring.py)
  # 18.01: the classified census of every frozen sixteen-hex literal. Phase
  # 17's clause (i) grep, `\b[0-9a-f]{16}\b`, printed 0 over the ADR-0131
  # re-freeze that removed 55 `0x...ull` literals - a suffix leaves no word
  # boundary - so "no frozen digest moved" had never been measured by anything
  # that could see one move. --check refuses a literal in a file the table does
  # not name; --self-test has that commit as its known answer, from a fixture,
  # because this CI clones at depth 1 and cannot show be9df8d itself.
  add_test(NAME Kernel.FrozenCensus
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/frozen_census.py --check)
  add_test(NAME Kernel.FrozenCensusSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/frozen_census.py --self-test)
  # 14.07: what the UI may include, and what it may never name. The UI cannot
  # be built by any leg of this CI, and under UnrealBuildTool every kernel
  # include path is transitive into it (VaelenView.Build.cs:29), so this text
  # check is the only thing standing between the UI and Commands.h. It reads
  # Source/VaelenUI and Source/VaelenGame/Public when they exist and the
  # witness of 14.07 (Tools/UiWitness) until they do.
  add_test(NAME Kernel.UiFence
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_ui_fence.py --root ${PROJECT_SOURCE_DIR})
  add_test(NAME Kernel.UiFenceSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_ui_fence.py --self-test --root ${PROJECT_SOURCE_DIR})
  # 17.02: the event-type name table. An Event carries a Hash64 and nothing
  # else about its identity, and until this phase nothing in the tree could
  # turn one back into a word - so every census, inspector and causal walk
  # Phase 17 plans would print sixteen hex digits at a person. The table is
  # GENERATED from the 115 MakeEventType declarations across nine modules,
  # because a compile-time one would need every module's headers in a single
  # translation unit and VaelenSim may not know VaelenMilitary exists.
  #
  # This entry re-runs the generator and diffs. It is the only thing that
  # notices a type added, renamed or deleted; Run.EventTypes checks the table
  # against the kernel's own HashString and against real declaration sites,
  # which is a different claim and a different entry.
  add_test(NAME Kernel.EventTypes
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/gen_event_names.py --check)
  add_test(NAME Kernel.EventTypesSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/gen_event_names.py --self-test)
  # 19.01, ADR-0159: what the engine modules say about themselves, held to what
  # the owner's machine built. Nothing in CI compiles those four modules, so
  # their STATUS lines were the whole record, and on 2026-09-25 five of them
  # said VALIDATED over code no compiler had read. EngineStatus refuses a claim
  # whose CODE moved since the build it names (Tools/engine_builds.txt, the
  # records in Tools/EngineBuilds/); ShimLedger refuses a name of the shim the
  # engine code relies on that no build compiled and nobody listed. Each
  # self-test holds a known answer from git and fails its instrument on purpose.
  add_test(NAME Kernel.EngineStatus
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_engine_status.py)
  add_test(NAME Kernel.EngineStatusSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_engine_status.py --self-test)
  add_test(NAME Kernel.ShimLedger
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_shim_ledger.py)
  add_test(NAME Kernel.ShimLedgerSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_shim_ledger.py --self-test)
  set_tests_properties(Kernel.Purity Kernel.PuritySelfTest Kernel.WorldWiring Kernel.UiFence
                       Kernel.UiFenceSelfTest Kernel.EventTypes Kernel.EventTypesSelfTest
                       Kernel.EngineStatus Kernel.EngineStatusSelfTest Kernel.ShimLedger
                       Kernel.ShimLedgerSelfTest
                       PROPERTIES TIMEOUT 120)
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
  # 19.01, ADR-0159: an engine sitting is closed by its committed log re-read
  # here against the headless command that must print the same lines - not by
  # pasting them into a document. Its known answer is the sitting of 2026-09-16:
  # accepted as it stands, refused with one digit changed, with a line missing,
  # and against the same stream replayed into the climate world.
  add_test(NAME Session.SelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_session.py
            ${PROJECT_SOURCE_DIR}/Tests/Run/Sessions/s0916.session --atlas $<TARGET_FILE:VaelenAtlas> --self-test)
  set_tests_properties(Session.SelfTest PROPERTIES TIMEOUT 3600 COST 2000)

  add_test(NAME Atlas.Output
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py ${VAELEN_ATLAS_A})
  set_tests_properties(Atlas.Output PROPERTIES FIXTURES_REQUIRED AtlasA TIMEOUT 300)
  add_test(NAME Atlas.Deterministic
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py
            --same ${VAELEN_ATLAS_A} ${VAELEN_ATLAS_B})
  set_tests_properties(Atlas.Deterministic PROPERTIES FIXTURES_REQUIRED "AtlasA;AtlasB" TIMEOUT 300)
  # 14.02: the ADR-0135 pair, held by a test at last. These are the two digests
  # the engine and the headless kernel were shown to agree on - frame and
  # ground at 128/120, the same seed - and until this entry they lived in
  # Docs/DECISIONS.md and nowhere a CI leg reads. A header split moves no
  # bytes, and this is what says so on every leg rather than in a sentence.
  set(VAELEN_ATLAS_128 ${CMAKE_CURRENT_BINARY_DIR}/atlas-128-120.json)
  add_test(NAME Atlas.Runs128
    COMMAND VaelenAtlas --size 128 --years 120 --out ${VAELEN_ATLAS_128})
  set_tests_properties(Atlas.Runs128 PROPERTIES FIXTURES_SETUP Atlas128 TIMEOUT 900)
  add_test(NAME Atlas.Frozen128
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py ${VAELEN_ATLAS_128}
            --expect frame=0xec18241b89c3d246 ground=0x8f7f4948f49b6e86)
  set_tests_properties(Atlas.Frozen128 PROPERTIES FIXTURES_REQUIRED Atlas128 TIMEOUT 300)
  # 18.04: the same world TOLD it has a climate. The ground does not move (it
  # is the map); the climate leaf's digest is frozen here the day it first
  # existed. Since 18.10 --climate is the default and this is the same world
  # as Atlas.Runs128 above - kept, because the leaf's pin is the one that must
  # not move with the simulation. --no-climate documents carry no climate
  # object (Atlas.Frozen128Before below).
  set(VAELEN_ATLAS_128_CLIMATE ${CMAKE_CURRENT_BINARY_DIR}/atlas-128-120-climate.json)
  add_test(NAME Atlas.Climate128
    COMMAND VaelenAtlas --size 128 --years 120 --climate --out ${VAELEN_ATLAS_128_CLIMATE})
  set_tests_properties(Atlas.Climate128 PROPERTIES FIXTURES_SETUP Atlas128Climate TIMEOUT 900)
  add_test(NAME Atlas.ClimateFrozen128
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py ${VAELEN_ATLAS_128_CLIMATE}
            --expect climate=0x59615fa1f5d24bd1 ground=0x8f7f4948f49b6e86)
  set_tests_properties(Atlas.ClimateFrozen128 PROPERTIES FIXTURES_REQUIRED Atlas128Climate TIMEOUT 300)

  # 14.03: the empty play - the Play wiring with nobody taken up and no
  # stream, written as a replay document. Running it is the test; what the
  # document says is read by 14.10's checker.
  add_test(NAME Atlas.Empty
    COMMAND VaelenAtlas --size 48 --prehistory 30 --years 10 --empty --out ${CMAKE_CURRENT_BINARY_DIR}/atlas-empty.json)
  set_tests_properties(Atlas.Empty PROPERTIES TIMEOUT 900)

  # 14.06: the first screen of that same empty play at 128/120, printed. The
  # page is composed kernel-side from three views, so the tool draws exactly
  # what the engine will; its LAST row is the page's own digest, and that row
  # is the one View.Panel freezes (VAELEN_PANEL_FROZEN_EMPTY). A row of the
  # page that differs anywhere moves that digest and reddens this.
  #
  # TWO entries for one command, and the reason is CMake's: a test with
  # PASS_REGULAR_EXPRESSION passes on the match ALONE and its exit code is
  # never looked at, so the frozen row and a tool that ran to the end cannot
  # be held by the same entry. PanelEmpty is the exit code; PanelFrozen is
  # the row.
  add_test(NAME Atlas.PanelEmpty
    COMMAND VaelenAtlas --empty --size 128 --years 120 --panel
            --out ${CMAKE_CURRENT_BINARY_DIR}/atlas-panel-empty.json)
  set_tests_properties(Atlas.PanelEmpty PROPERTIES TIMEOUT 900)
  add_test(NAME Atlas.PanelFrozen
    COMMAND VaelenAtlas --empty --size 128 --years 120 --panel
            --out ${CMAKE_CURRENT_BINARY_DIR}/atlas-panel-frozen.json)
  set_tests_properties(Atlas.PanelFrozen PROPERTIES TIMEOUT 900
    PASS_REGULAR_EXPRESSION "\ndigest d7e7149e65ceb69a\n")

  # 18.10: THE WORLD BEFORE PHASE 18, still held. The default flipped to the
  # climate and re-froze Frozen128's frame and PanelFrozen's page above; these
  # entries hold the same commands with --no-climate to the digests they had
  # before the flip, so the pre-winter path is a world every leg still
  # reproduces and not a memory (gate clause (i)).
  set(VAELEN_ATLAS_128_BEFORE ${CMAKE_CURRENT_BINARY_DIR}/atlas-128-120-before.json)
  add_test(NAME Atlas.Runs128Before
    COMMAND VaelenAtlas --no-climate --size 128 --years 120 --out ${VAELEN_ATLAS_128_BEFORE})
  set_tests_properties(Atlas.Runs128Before PROPERTIES FIXTURES_SETUP Atlas128Before TIMEOUT 900)
  add_test(NAME Atlas.Frozen128Before
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py ${VAELEN_ATLAS_128_BEFORE}
            --expect frame=0xabc5a5767c6cf9dd ground=0x8f7f4948f49b6e86)
  set_tests_properties(Atlas.Frozen128Before PROPERTIES FIXTURES_REQUIRED Atlas128Before TIMEOUT 300)
  add_test(NAME Atlas.PanelFrozenBefore
    COMMAND VaelenAtlas --no-climate --empty --size 128 --years 120 --panel
            --out ${CMAKE_CURRENT_BINARY_DIR}/atlas-panel-before.json)
  set_tests_properties(Atlas.PanelFrozenBefore PROPERTIES TIMEOUT 900
    PASS_REGULAR_EXPRESSION "\ndigest 54787451e65766c1\n")

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
