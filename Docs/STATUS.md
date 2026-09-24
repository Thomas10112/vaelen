# VAELEN — Build status

STATUS: VALIDATED for the state it reports, checked on 2026-09-24 against the sources on
branch `claude/vaelen-master-prompt-aw7zqj`. This is the living status
document: it is refreshed at the end of every task (section "How to refresh"). The
per-phase breakdowns below are the record of each phase as it closed and are not
rewritten afterwards; this block is the only part that tracks today.

## BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 17 — DEBUG TOOLS, CLOSED 2026-09-24: all eleven clauses of its gate
              met, read clause by clause in Docs/ROADMAP.md section 23. Clause (j):
              Tools/run_gates.sh linux-clang-debug printed GATES-DONE 0 failing (of 17)
              in 5122 s on 5032246; clause (i): no frozen digest moved over nine tasks.
              CI run 276 on 5032246: nine of ten legs green, the clang-debug leg still
              testing when this was written (run 274 on 65b60d4 was ten of ten).
              NEXT: Phase 18 CLIMATE & SEASONS — its judge-panel plan is running.
              (16 SAVE/PERSISTENCE: fourteen tasks built, its GATE still open —
               clause (j)'s migration half deferred to the v4 bump; and 16.14, whose
               sitting is the owner's Windows machine AND whose engine half —
               Vaelen.Save/Load, a store over IFileManager — is not written anywhere
               yet: it is written here and parsed against the shim before that
               sitting. Section 22 has the clause-by-clause read.
               15 CLOSED 2026-09-21, all six clauses; 14 CLOSED 2026-09-16)
TASK        : 16.14 (engine half) — WRITTEN and PARSED 2026-09-24, UNVERIFIED (engine).

              Found at the Phase 17 close by reading the tree: no Vaelen.Save, no
              Vaelen.Load, no store over IFileManager existed anywhere. Written here
              and parsed against the shim, which grew FFileHelper's byte pair,
              FPaths::GetCleanFilename and HAL/FileManager.h (six verbs, the engine's
              defaults): FVaelenCheckpointStore (the stdio store's twin - <name>.writing
              moved into place whole, a listing that reads the directory), the
              subsystem's Save/Load/Saves, three console commands, three shim
              mutations (16 of 16 caught). Save prints the four digests the way
              Stream.Write does and then the exact VaelenAtlas --load-from command
              that must come back to the same state; Load builds the host from the
              container's HOST section, adopts into a FRESH Aelvor, hands the tape
              back to the door. Docs/ENGINE_HANDOFF.md has the sitting, step by step.

              TWO KERNEL FINDINGS, fixed with tests and no digest moved: the image
              trailer had THREE readers (store, Atlas, corpus test) and the engine
              store would have been a fourth - Run::ImageTrailer is the one now and
              Run.Containers keeps its own as the independent reader and requires
              agreement on every file; and `.writing` leftovers of an interrupted
              write were LISTED AS SAVES (tick 0) because the stdio store's comment
              claimed a rule the name rule did not keep - Run::WritingSuffix is the
              interface's now, IsUsableCheckpointName refuses it, Run.Store and
              Run.StoreColdProcess pin it with the half-file-under-a-plain-name
              control.

TASK (17.gate): Phase 17 gate — READ 2026-09-24, eleven of eleven. Last built: 17.09.

TASK (17.09): 17.09 — DONE 2026-09-24. The why says how it ended.

              THE TWO-STEP LIMIT WAS NEVER IN THE KERNEL: History::Why walked to 64 and
              ExportWhyWithLife printed every step; the limit was ChronicleView::WhyLines
              = 2 in the view leaf. So the deep why is three things and none is a deeper
              walk. History::Why returns WalkEnd through CauseWalk — until now Root,
              CauseMissing and DepthExhausted were all an empty tail. WhyEndText is the
              ONE place a non-root end's sentence lives, and both text exports print it.
              The view holds FOUR lines (not two) and a WhyEnd field from its own walk, so
              a longer chain says DepthExhausted instead of trailing off.

              FOUR AND NOT EIGHT, by two measurements: the deepest chain 17.05 found
              anywhere is three edges, and the row says the leaf is 8 KiB — the first cut
              (eight lines, 2048 bytes) was 9552 and the static_assert refused it. Four
              lines at 192 bytes, the chronicle text's own per-line budget, is 8144.

              OVER A REAL WORLD THE CHRONICLE PRINTS WHAT THE CENSUS PREDICTS: "the why
              is 2 line(s) deep on this world, ending at a root" — a PlayerActed has no
              cause of its own. The old `WhyCount == WhyLines` was a coincidence of
              capacity and content; it reads == 2 with WhyEnd == 0 now, in two tests.

              Sim.HistoryText.WhySaysHowItEnded plants chains through EventBus::Publish
              with a cause — whole, longer than allowed, caused by a person — and each is
              named, each end's sentence printed, the six sentences two empty and four
              pairwise different. A CUT CHAIN CANNOT BE PLANTED IN A WORLD: the first
              version tried, the serial below the root was a real event, and the case
              fell into an else branch — found when the deliberate failure fired on
              three checks and not four. A world's log has no holes; the test asserts
              that now, and CauseMissing stays with Sim.CauseWalk's hand-built log.

              The row's condition, answered: the census said three edges in the ledger
              and zero in the human story, so this is code — and its honest shape is an
              END that is named, because the chains a person will actually see are two
              long and the question they raise is "is that all?", answered Root.

TASK (17.08): 17.08 — DONE 2026-09-24. The cache that is safe by accident, its nine
              doors guarded, and a tenth found OPEN on the first run.

              No change to DiplomacySystem's cache, as the row said: guard now, fix if
              asked. Run.RefusalsAreTheCachesSafety, three cases, 0.74 s.

              THE PREMISE MEASURED. Six 32-tile worlds at seeds 1-6 have 12, 13, 12, 13,
              13 and 10 regions and differ pairwise by digest: FOUR PAIRS share a count
              and are different worlds — each a stale graph the cache's key
              (Diplomacy.cpp:72, the region COUNT) cannot see. Two in three seeds.

              NINE DOORS SHUT, each failure text naming the door AND the cache: Adopt's
              WrongSeed and seven *Differs, and the kernel's SeedMismatch through
              LoadSnapshot on a BEGUN world. The control first — the matching world is
              accepted at both doors — and every refused host still un-begun.

              THE TENTH DOOR IS OPEN. A begun 16-tile world of the same seed takes a
              32-tile image and LoadSnapshot answers Ok, because WorldMap::LayoutDigest
              folds layer names and element sizes and NO EXTENTS — Phase 16's defect 5,
              still pinned by Golden.TheLayoutDigestCannotTellTwoMapSizesApart; 16.08 did
              not touch it. After the load the map is 32 wide and the Aelvor declares 16:
              ADR-0150's chimera, measured. In production the door is reached only
              through Adopt, which refuses the size by the HOST section first.

              NOT CLOSED HERE because the layout digest is written INTO the image
              (Snapshot.cpp:435): folding the extents moves every frozen digest — clause
              (i). It is a re-freeze like ADR-0131. So the third case pins the door OPEN
              and FAILS THE DAY IT CLOSES, so whoever closes it finds this file, the golden
              test and ADR-0150 together. The counts were 3 against 15 this time, so the
              graph would have been rebuilt — "this time" is the operative phrase.

              THE GATE'S CONTROL, RUN: YearsDiffers disabled in a scratch build — "the
              YearsDiffers door is OPEN: Adopt answered Ok where YearsDiffers was
              required - DiplomacySystem's region graph (Diplomacy.cpp:72) ...". Named
              door, named cache.

              Owner's question 4, sharper again: the kernel door already loads a
              differently-sized world of the same seed, and closing it costs a re-freeze
              — Phase 18's climate re-freeze is coming, and they should share the commit.

TASK (17.07): 17.07 — DONE 2026-09-24. A harness that can see a vacuous assertion.

              VT_CHECK_ROUNDTRIP(Written, Read) records a failure when the WRITTEN value
              is byte-equal to T{} BEFORE it compares anything, because a reader that
              wrote nothing would then pass; the type must be trivially copyable with
              unique object representations or it is refused at compile time.
              VT_CHECK_DIGEST_EQ(A, B) refuses 0 and EventLog::EmptyDigest as operands —
              the harness sits below VaelenSim, so it carries the value as a literal and
              Tests/Sim/Test_EventLog.cpp holds the two together with a static_assert.

              Core.Harness, two new cases. VacuityIsSeen, BOTH ARMS IN ONE RUN: the
              assertion Phase 16 committed — two default-constructed StartRules compared —
              PASSES under VT_CHECK (the defect, kept visible) and FAILS under
              VT_CHECK_ROUNDTRIP in the same test; a genuine round trip passes under both,
              so the witness is not simply a macro that fails; a lossy round trip fails
              for the loss. DigestsThatCompareNothingAreRefused: five checks, one pass and
              four refusals, two of which would have been green under VT_CHECK_EQ — which
              is how nine matrix cells once compared an empty life against itself.

              TWO DELIBERATE FAILURES, BOTH FIRED: the vacuity check disabled — "the
              witness let the vacuous round trip through (0 failures)"; the empty-log
              refusal disabled — Failures 3 against 4.

              NOT done here, on purpose: converting existing tests. The nine
              empty-against-itself cells of Run.SaveContinue are the open review finding
              and get their own commit with their own measurement.

TASK (17.06): 17.06 — DONE 2026-09-24. The inspector: a container described from its
              bytes, and a directory listed by a process that wrote nothing.

              Atlas --inspect FILE prints the header, every section with kind, offset,
              length, share and digest, the image TRAILER named as "the state digest
              every other tool means", the HOST wiring, the RUN state's shape and the
              STREAM's counts, header and rules — WITHOUT CONSTRUCTING AN AELVOR.
              ReadHostSection, ReadRunSection and ReadStreamSection need no world, and
              that is the claim: a 2 GB save described in the time it takes to read it.
              Until this, nothing in the tree read a container from disk except a test.

              Atlas --inspect-dir DIR is 17.03's second-process half.
              Atlas.InspectDir.ColdProcess: two writer processes at seeds 1 and 2, a
              third that lists both with the trailers their writers printed ON THE SAME
              LINE AS THEIR NAMES, so a listing right by name and wrong by position
              cannot pass. The first expectation said four sections; --save-to carries no
              tape, so three. The tool was right and the expectation wrong.

              AND THE FIRST RUN OVER THE CORPUS DIRECTORY FOUND SOMETHING: README.md was
              listed as a checkpoint with tick 0, version 0 and sixteen zeros for a digest
              — a row that looks like data and is not. The store is right to hand it back;
              a browser is wrong to show it as a save. ContainerVersion is 0 exactly when
              ReadCheckpoint refused, so the tool prints "not a container" and counts
              "3 checkpoint(s) and 1 other file(s)".

              TWO REFUSALS, both about what is NOT printed. An image is refused BY NAME —
              "BadMagic (it is a save IMAGE, the inner format, not a VAELENCP container
              around one)". A container cut at 4 KiB, whose header is readable and whose
              table points past the end, is refused WHOLE: "Corrupt, 4096 bytes on disk;
              section 0 is where it stopped describing them", and not one header or table
              line printed.

              THREE DELIBERATE FAILURES, ALL FIRED. Header printed before the refusal:
              "a refused file was described in part". A pinned trailer bent by one hex
              digit: "WANT_TRAILER not printed". One byte of a listed file flipped:
              "1 checkpoint(s) and 1 other file(s)", shown as "not a container".

TASK (17.05): 17.05 — DONE 2026-09-24. The census, and it concluded the phase is about
              something else — just not the something the plan expected.

              THE PANEL MEASURED 7 CAUSES IN 535 EVENTS (1.31%) with a deepest chain of
              ONE EDGE, and three of its four angles designed a walker, an index and a
              renderer over that. TakeCauseCensus over the 17.01 corpus reproduces those
              figures by an independent instrument: 3/129, 3/148, 7/577, depth 1, fan-out
              1. Then it ran over a FRESH AELVOR 128 at 300+120 years, 21 min 51 s:

                16,842,422 events, 5,211,672 with a cause (30.94%), 11,630,750 roots
                deepest chain 3 edges, median depth 0, widest fan-out 11 (event 4193697)
                0 dangling, 0 not an event, 0 not before their effect, 53 event types

              THE PANEL HAD MEASURED THE WRONG WORLDS. Three ten-year worlds whose economy
              had barely started. The plan's "98.7% roots" was true of the corpus and
              false of the world, and my own comments in Causality.h and two tests carried
              it; corrected here. 17.09 is therefore CODE and not a note: there IS a third
              step to walk to.

              BUT THE TABLE PER TYPE SAYS WHERE THE GRAPH LIVES, and it is not where a
              person would look. StockTaken 95.4% and StockAdded 98.8% with a cause —
              5.2 million of the 5.21 million causes are the stock ledger. DisasterStruck,
              RegionSettled, Condemned, Pardoned, ReligionFounded: 100%, in the hundreds.
              And PersonDied, PersonBorn, PersonMarried, RulerSeated, HeirNamed,
              FamilyFounded, MigrationWave: 0.0%, every one. A deeper why explains where
              goods came from. It explains no death, no birth and no reign. The causal
              graph is dense in the ECONOMY and empty in the HUMAN STORY, and that is the
              measurement 17.09 and any later "fill the Cause edge" task must start from.

              ALSO MEASURED, and nobody asked: PlayerActed is 9,147,427 of 16.8 million
              events — 54% of the log — in a world nobody played. That is the Lively
              wiring's persons acting through the command surface, and it is the single
              largest thing in every save. HeardOf is another 2.3 million. Phase 18's log
              question has its first figure.

              Vaelen/Sim/Causality.h gained CauseCensus and TakeCauseCensus: one forward
              pass for the depths, correct because causes precede effects and the log is
              in id order — no recursion, no stack to blow on a chain a million long — and
              a sort for the fan-out. Atlas --causes FILE adopts a container from its own
              HOST section so the wiring cannot be spelled wrongly on the command line,
              and prints the table per type through 17.02's names; --census generates.

              Sim.Causality, four cases: the planted graph counted exactly (chain of
              seven, fan-out of five, one dangling, one Person cause, one cause after its
              effect — eighteen events, and the parts must add up to the whole); the SAME
              eighteen with every cause cleared reporting depth 0 and fan-out 0 while the
              planted one still reports 7 in the same run; an empty log all zeros and not
              a verdict; and the census agreeing with 17.04's walk, which shares no code
              with it. Atlas.Causes.{bare-16,played-16,full-32} pin the corpus lines whole
              and refuse any `?<hex>` row.

              AND THE FIRST PLANTING WAS WRONG, not the code: event 30's "dangling" cause
              was 99, which FOLLOWS it, and both instruments called it NotBeforeEffect —
              correctly, and in agreement. A dangling link must precede and be absent.

TASK (17.04): 17.04 — DONE 2026-09-24. A causal walk that says how it ended, and an
              experiment that told me about the test instead.

              History::CauseChain walks the cause edge backwards and hands back a vector.
              It has SIX ways to stop and the caller can tell them apart in none of them:
              History.cpp:198-211 reached four through ONE break and the fifth by falling
              out of the loop. A tool that walks backwards and stops cannot say whether it
              reached the BEGINNING OF THE WORLD or FELL OFF THE END OF A LOG — and this
              is the phase whose job is telling a person why something happened.

              Vaelen/Sim/Causality.h declares WalkEnd { Root, NoSuchEvent, CauseMissing,
              CauseNotAnEvent, CauseNotBeforeEffect, DepthExhausted } and CauseWalk.
              CauseChain stays, reimplemented as a wrapper that discards the end, so its
              TWO callers do not churn — HistoryText.cpp:267 and Atlas --why. The plan
              said five.

              SIX ENDS AND NOT SEVEN, dropped on purpose. The plan wrote
              WalkLimits{Depth, Nodes} with a BudgetExhausted beside it. A backwards walk
              is LINEAR, one node per step, so the two limits can never disagree — and a
              state no input can reach is a state no test can reach either. No cycle guard
              is needed either: CauseNotBeforeEffect makes every step strictly decrease
              the id.

              TWO ORDERING DECISIONS, EACH PINNED BY A CASE THAT CAN SEE IT. The kind
              check comes BEFORE the ordering check, because IdKind is the high byte of
              the id: an Entity cause (kind 1) compares BELOW an Event effect (kind 2) and
              sails past the ordering guard, while a Person cause (kind 23) compares above
              and would be called an ordering fault. Both are the same mistake. Swapping
              them makes the Person case report CauseNotBeforeEffect, and the test says so.

              AND THE SECOND EXPERIMENT DID NOT FAIL, which is why the second decision has
              a row of its own. Moving the depth check ahead of the link resolution was
              meant to break the boundary case; for a WHOLE chain the two orders are
              identical, so the test was measuring nothing about the ordering. They differ
              in exactly one place — when the budget runs out on the step that would have
              found the link missing — and both facts are then true at once. THE DATA
              FAULT WINS: CauseMissing says the log is broken, DepthExhausted says the
              caller asked for less than there was, and a person shown the second when the
              first is true raises the limit and learns nothing. A case for it was added
              and the same experiment then reported "a cut on the limit must report the
              cut, not the limit: DepthExhausted".

              That is the THIRD time this session an experiment has told me about the
              instrument rather than the guard, and the third time the fix was to make the
              instrument able to see.

              Sim.CauseWalk, three entries, 0.00 s: eight cases (one per end, both sides
              of CauseNotAnEvent, the depth boundary at exactly the chain's length, a
              depth of zero, and the cut-on-the-limit case); the PRE-FIX ARM KEPT
              PERMANENTLY — today's CauseChain over a whole chain and a cut one must give
              the SAME answer, that indistinguishability IS the defect, while CauseWalk
              over the same two logs says Root and CauseMissing in the same run; and six
              depths comparing the wrapper against the walk event by event, so 17.04
              cannot have changed behaviour while claiming to add a return value.

              The logs are hand-built. A real world's log is 98.7% roots, so five of the
              six ends would never occur and the test would be a test of demography.

              36/36 green under Sim., Sim.Shuffled included.

TASK (17.03): 17.03 — DONE 2026-09-24. The store reads a directory, and reports the
              digest it says it reports. Both defects were mine, from 16.07.

              THE FIRST: List() iterated a private vector that only Write() and
              Remember() ever filled, so a host started fresh and pointed at a folder
              full of saves was told it was empty — which defeats the one thing a save
              browser is for. Run.Store passed the whole time because it wrote and listed
              in ONE process with ONE object: an instrument blind to the only dimension
              that matters. List() reads the DIRECTORY now (<filesystem>, error_code
              overloads throughout, never the throwing ones, because this tree builds
              -fno-exceptions), sorted so two hosts agree on order, skipping anything
              IsUsableCheckpointName refuses — which also skips a .writing temporary left
              by an interrupted write. Written and Remember are DELETED: a cache of names
              can now only disagree with the disk, and worse than a store that lists
              nothing is a store that lists something that is not there.

              THE SECOND, AND MAKING THE TEST FAIL ON PURPOSE REVERSED WHICH FAULT
              MATTERS. The entry's digest was Sections.front().Digest, reported after the
              result of Find(State, Length) had been called and thrown away. I wrote in
              the plan, and repeated it in 17.01's README, that this was "right today
              because STATE happens to be section 0". MEASURED: on an ORDINARY container
              the old code reports e0614906cb8a5676 where ComputeStateDigest returns
              0f6fa26b35d09a70. The section digest and the image trailer are different
              numbers over the same bytes, on every container, whatever the order. So
              position was the SECOND fault — ADR-0150's safe-by-accident, which had not
              started mattering yet — and the first was reporting a number no other
              instrument in this repository means by a save's digest. A host comparing a
              listed digest against a logged one was told two identical saves were
              different worlds, and it was told that ALWAYS. All three places carrying my
              wrong account are corrected in the same commit.

              StoreEntry also gained SectionCount, so a caller can see a save's SHAPE —
              above all whether it carries its own input tape — without opening it twice.

              AND THE SECOND CONTROL FOUND SOMETHING ABOUT THE FORMAT. The gate asks for
              a container in which STATE is not first. Rotating the table ROWS is refused
              BadSectionTable, correctly: ReadCheckpoint requires ascending offsets, no
              overlap, and every payload byte claimed. A legal container with STATE second
              needs the PAYLOADS moved with the rows, which the test now does before
              recomputing the trailer. It reads Ok, its first section is RUN, its STATE
              bytes are memcmp-identical to the original's — and the fixed store reports
              the same trailer while the pre-fix one reports RUN's section digest.

              Run.StoreColdProcess, two cases, 0.10 s. THE PRE-FIX LISTING IS KEPT, as
              AWrittenOnlyStore, deriving from the real store so it differs from it in the
              defect and in nothing else (StdioCheckpointStore is no longer final for
              exactly this). It lists ZERO of the same three files in the same run, then
              lists the one file it writes itself — so the arm is about coldness and not
              about the class being broken outright.

              TWO DELIBERATE FAILURES AGAINST THE FIXED CODE, BOTH FIRED. Sections.front()
              put back: "alpha: listed e0614906cb8a5676, ComputeStateDigest
              0f6fa26b35d09a70". List returning before it reads the directory: "a cold
              store listed 0 of 3".

              THE SECOND-PROCESS HALF of the gate clause rides on 17.06's --inspect-dir,
              which is the tool that does it. The defect itself is per-INSTANCE, so a
              second object over the same directory is the evidence that distinguishes it.

              AND THE CI FOUND TWO DEFECTS IN THE TESTS THEMSELVES on 4340938 — four of
              ten legs red — that no Linux debug build could see. The serious one: a
              pointer INTO a temporary vector, `Named(Cold.List(), "swapped")`, read after
              the vector died. gcc's allocator left the bytes in place and every Linux leg
              passed; AppleClang read 0000000000000000 and MSVC read dddddddddddddddd, its
              freed-memory fill. AddressSanitizer sees it deterministically —
              heap-use-after-free at Test_StoreColdProcess.cpp:360 on the pre-fix file,
              clean on the fixed one — and that probe is now the local instrument for this
              class. The other: gcc -O2 -Wnull-dereference refuses a vector index on a
              copy, and -Werror made both gcc release legs fail to BUILD; a RelWithDebInfo
              tree matching the CI preset is configured beside the debug one so this can
              be seen before a push. Neither defect was in the store; both were in the
              instruments that judge it, and both were found by the only thing that runs
              them on four compilers.

TASK (17.02): 17.02 — DONE 2026-09-24. The event-type name table, generated from the
              115 declarations and checked against the kernel's own hash.

              AN EVENT CARRIES A HASH AND NOTHING ELSE about its identity: Event.h stores
              Hash64 TypeHash, and the name lives at the declaration site in the
              EventType<T> constant. The moment an event is in a log, an image or a
              container, the word is gone. Grepping this tree for EventTypeName,
              NameOfEvent or TypeName( returned NOTHING, so every census, inspector and
              causal walk this phase plans would have printed sixteen hex digits at a
              person.

              Source/VaelenSim/Public/Vaelen/Sim/EventTypeNames.h is generated by
              Tools/gen_event_names.py from 115 MakeEventType call sites across nine
              modules — 115 distinct names, no duplicates, no hash collisions, all checked
              before it writes a byte. THE COUNT IN THE PLAN WAS WRONG BY ONE AND IN MY
              OWN HANDWRITING: grep -c returns 116, and the 116th is the template's own
              definition in Event.h, which has no name to read. The generator's regex
              matches a string LITERAL and its self-test requires it NOT to match that
              definition, so the off-by-one cannot come back.

              GENERATED AND NOT CONSTEXPR because a compile-time table needs every
              module's headers in one translation unit and VaelenSim may not know
              VaelenMilitary exists. The header knows hashes and string literals and
              depends on CoreTypes.h, Hash.h and <cstdio>, which purity allows.

              NameOfEventType(Hash64, char (&)[18]) is a binary search over a hash-sorted
              table, and an unknown hash is answered with ?<16 hex digits> IN THE CALLER'S
              BUFFER — not an empty string, not a static, not a crash. A log written by a
              build carrying a type this one lacks is what a save format exists to make
              possible; the three wrong answers are a crash, a blank row that looks like a
              bug in the census, and a plausible name belonging to something else.

              FOUR ENTRIES. Kernel.EventTypes re-runs the generator and diffs — the only
              thing that notices a type added, renamed or deleted. Kernel.EventTypesSelfTest
              runs the generator's own controls, including that the empty string hashes to
              the FNV-1a offset basis, the one value the arithmetic cannot get right by
              accident. Run.EventTypes (five cases) re-hashes every one of the 115 names
              with the KERNEL's HashString, checks the table is strictly sorted, finds
              every row by its own hash, samples one real declaration from each of seven
              modules, and requires an unknown hash to answer ?<hex> while a known one
              still answers its name in the same run.

              FOUR DELIBERATE FAILURES, ALL OF WHICH FIRED. Renaming ArmyRetreated to
              ArmyWithdrew: the check reports + ArmyWithdrew and - ArmyRetreated BY NAME,
              because a diff of a 115-row table is unreadable and the answer is almost
              always one name. Adding a type: 116 declarations found. Hashing with a
              trailing NUL — the classic way to build a table that is self-consistent,
              compiles, round-trips against itself and agrees with the running world about
              nothing — makes the generated text differ AND, when that header is built,
              makes Run.EventTypes report 129 failures.

              THE ROWS CARRY NO // path:line COMMENT, after seeing the alternative:
              clang-format aligns 115 trailing comments into one column whose position is
              set by the longest line in the block, so one long event name would reformat
              the whole table. The site is one grep away and the header says so.

TASK (17.01): 17.01 — DONE 2026-09-24. A real container corpus, before anything reads one.

              PHASE 16 BUILT VAELENCP AND CLOSED WITHOUT ONE CHECKED IN. Tests/Run/Golden
              holds .snapshot IMAGES — the inner format, magic VAELEN\0\0\0, version 3 —
              and a reader handed one says BadMagic. So every instrument Phase 17 plans
              (the inspector, the store's cold listing, the census) was specified against
              files that did not exist, and three of the four planning angles wrote gate
              clauses over them.

              Tests/Run/Containers/ now holds three v2 containers whose SHAPE differs and
              not only their contents, because a corpus where every file has the same
              section table cannot catch a reader that assumes one: bare-16 (three
              sections, no play wiring, 77685 bytes), played-16 (FOUR, with STREAM, 80862)
              and full-32 (three — played, but saved without its tape, 182719). The README
              records every header field, every section row and the image TRAILER, which
              is the last eight bytes of the STATE section and is what ComputeStateDigest
              returns — as distinct from the STATE row's section digest beside it, a
              different number over the same bytes. Both are recorded because confusing
              them is a live defect in Tools/Store/StdioCheckpointStore.h and 17.03 is the
              fix.

              bare-16's trailer is d17d7fd9a6f09ea6, which is the golden bare-16.snapshot's
              state digest, and its STATE section is 77478 bytes, that file's exact size.
              The container carries the image VERBATIM and that is now checkable by memcmp
              rather than by argument.

              AND THE CORPUS COULD NOT HAVE A PLAYED CONTAINER AT ALL until something was
              measured. TakeUp was offered nobody at 16, 24 or 32 tiles. Sweeping the
              rules at ten years of pre-history: WantBound 1 offers nobody under any age
              window; WantBound 0 offers person 1 at ages 0-10 and nobody at 12-120.
              NOBODY IN A TEN-YEAR WORLD IS TWELVE, and nobody in it is bound to anything —
              everyone alive was born inside it. StartRules{}, a bound life aged 16 to 40,
              is offered nobody, which is also why full-16.snapshot next door was never
              played. The two played containers carry {0, 45, 0, 0}: all four fields
              non-default, which guards 17.07's vacuous-assertion defect a task early, by
              the corpus rather than by a macro.

              TWO ENTRIES MAKING OPPOSITE CLAIMS. Run.Containers generates NO WORLD — there
              is no Aelvor in the file — and has five cases: the record against the bytes;
              the STREAM rules against the defaults and then against their values; every
              recorded field bent in turn with the comparison required to name it; edited
              bytes refused (section count 3/5/0/64, a flipped payload byte, a truncation)
              with the untouched bytes still reading in the same run; and a .snapshot
              handed to ReadCheckpoint required to say BadMagic. Atlas.ContainersRegenerate
              generates three worlds and requires byte equality, because Run.Containers
              would go on passing on the day this build lost the ability to WRITE one.

              THREE DELIBERATE FAILURES, ALL OF WHICH FIRED (ADR-0149). Flipping byte 40000
              of bare-16: the regeneration entry says "no longer regenerates" and
              Run.Containers names the refusal. Deleting one field's comparison from
              FirstDisagreement: "bending log bytes was not seen at all". The first run
              also found a real defect in the recorder — a 256-byte snprintf had sliced a
              table header in half at |---|---|-- while the FILES were correct, so only
              the record was truncated. The buffer is 640 now and its return value is
              checked, because a README that agrees with nothing is the same class of
              defect as a store that reports a digest by position.

              Run.Containers 0.01 s, Atlas.ContainersRegenerate 0.17 s, corpus 341 kB.

              AND THIS BLOCK HAD BEEN LEFT AT 16.02 while Phase 16 ran to 16.13. That is a
              gap in this document and not in the work: the thirteen tasks are in
              Docs/ROADMAP.md section 22 with their as-built notes and their measurements.
              They are not backfilled here, because a status block reconstructed after the
              fact from a roadmap is a status block nobody checked.

              It returned void. A failing body went into a [[maybe_unused]] and was
              reported by VAELEN_CHECKF, which Assert.h compiles to ((void)0) under
              NDEBUG and under UE_BUILD_SHIPPING — so in a player's build nothing
              happened at all. It now returns SnapshotResult, and on ANY refusal it
              truncates the caller's buffer back to the size it was handed, so a caller
              that appends never handles bytes nobody meant. The dispatch guard, which
              was the same disappearing macro, is a returned Inconsistent.

              AND THE DEFECT WAS DESCRIBED WRONGLY, INCLUDING BY ME. The planning said a
              short image was "well-formed, wrong, and validated on load", and I repeated
              it before checking. Measured at four cut points: a resealed short image is
              REFUSED, Truncated every time, because the reader runs out of a section. So
              the old code did not corrupt a loaded world. What it did was tell the caller
              nothing at the moment of WRITING — a player was told their game was saved
              and found out it was not only when they opened it, with the world it came
              from already gone. Snapshot.AShortImageIsRefusedButTheWriterNeverKnew
              carries the correction in its name.

              Both new tests ran with VAELEN_ASSERTS_ENABLED = 0, verified rather than
              assumed: the build is Release, CMAKE_CXX_FLAGS_RELEASE carries -DNDEBUG, and
              a probe prints 0. That is the build the defect lived in. The CI debug legs
              run them with asserts on.

              NOT [[nodiscard]], and that is a decision. Ninety call sites ignore the
              result today; putting ninety mechanical edits into the commit that fixes a
              data-loss path is how a mistake hides. The defect was that the function
              could not report, not that tests do not listen. Snapshot.h says so.

              ALSO CONFIRMED, by the same probe: defect 2 is real. At cuts of 25, 50 and
              75 per cent the target world is left a CHIMERA after LoadSnapshot has
              honestly refused — neither the world it was nor the world in the image.
              16.03 is the task for it.

TASK (16.02): 16.02 — DONE 2026-09-21. SaveSnapshot can fail, and says so. Its full
              narrative is the block this one displaced; Docs/ROADMAP.md section 22 keeps
              the measurements.

TASK (16.01): 16.01 — DONE 2026-09-21. The golden corpus and the gate list, both of
              them one-way doors, which is why they are the phase's first commit.

              THE PLANNING FOUND TWELVE DEFECTS before a line was written, and one is a
              live data-loss path: SaveSnapshot returns void, drops its body's result into
              a [[maybe_unused]] and checks it with VAELEN_CHECKF, which Assert.h compiles
              to ((void)0) under NDEBUG and under UE_BUILD_SHIPPING — the builds a player
              runs. A failing serialisation then writes a SHORT image, and the next line
              hashes those short bytes and appends a trailer over them, so the file is
              well-formed, wrong, and validates on load. Verified by reading
              Snapshot.cpp:253-255 and Assert.h:33-43, not taken on a planner's word.
              16.02 fixes it and costs no digest.

              WHAT PHASE 16 IS ABOUT, measured before it was planned: SaveSnapshot saves
              the WORLD and it works — a played AELVOR 128 restores to an identical digest
              and runs ten more days to the same figure. What is missing is the RUN.
              Run::Aelvor's Near_ and Watched_ live outside the image, so the moment
              anybody LOOKS a restored world parts company with the one it was copied from
              (6d4b82134edfcbf6 against 02c94cdaf3367b43). About thirty bytes, one part in
              700,000 of the image. Fifteen phases of accumulated state the Phase 01
              snapshot never knew it had to carry.

              16.01 SHIPPED, AND ITS OWN ESTIMATE WAS WRONG BY 216x. The plan costed a
              third golden at "about 104 KB"; that world weighs 22,568,335 bytes. Measured
              instead: the event log is 75% of any image with a history, and a 16-tile
              world goes from 78 KB to 11.4 MB between ten years of pre-history and
              thirty. The corpus is three young worlds — 329 KB in all — and buys FORMAT
              coverage, not world coverage. Tests/Run/Golden/README.md says so in the file
              rather than leaving a green test to imply more than it proves.

              AND THE CORPUS PROVED A DEFECT BY EXISTING: full-16 and full-32 share the
              layout digest 5ab1a2f994715f25, so the only value LoadSnapshot compares
              cannot tell a 16-tile world from a 32-tile one.
              Golden.TheLayoutDigestCannotTellTwoMapSizesApart pins it, and when 16.08
              fixes it that test must FAIL and be rewritten to say the opposite.

              The gate list is seventeen entries, not the eleven it ran through fifteen
              phases: Run.Golden, Replay.Played, Replay.Walked, Replay.Lived, Run.Gate and
              Run.Gate.Lived joined it. Eleven of them check a digest a system computes;
              six check a digest a RECORDING replays to, and Phase 16 is about to change
              the code that reads recordings. `--self-test` appends an impossible gate and
              requires the count to go non-zero; a moved figure makes Gate.cmake exit 1.
              Both arms were run.

TASK (15)   : 15.10 — DONE. The walk was lived at the keyboard on 2026-09-21 and the
              gate keeps all six clauses.

              142 day turns, 138 looks, 4 takings and 2 Speak intents, recorded in UE 5.6
              on Win64 (MSVC 19.51) with the daily cadence on, replayed headlessly on
              gcc/Linux to the SAME four digests: state 609253a29361ec5f, log
              a2839792e0837328, life 0a4babc60f6e4d90, panel c4ddbe971538c59c. 15 pins
              published while held, 0 wrong, both grains agreeing on every one of the 142
              days, no day turn promoting twice, somewhere to walk within one day turn of
              each taking. CTest Replay.Lived and Run.Gate.Lived pin it; 179 entries.

              THE FIRST WALK WAS REFUSED, AND THE INSTRUCTION WAS THE DEFECT. Clause (a):
              99 looks, 0 pins. A pin fires when a detailed region stops being wanted and
              holds somebody - and the camera is what makes a region wanted, so the fence
              only fires when the camera crosses the played person's own region and
              leaves it. The instruction said "stay near the centre and move a little";
              the camera sat on region 54 for seventy-five days while the person lived in
              region 9, forty-five metres away. --walk's 53 pins had been written down as
              a property of the walk when they are a property of a camera that changes
              region daily. Isolated by experiment on the recorded stream: reach 0 to 1
              changed nothing, wandering gave 57, alternating between two regions that
              are not the played one gave 0, alternating with the played one gave 38.
              ADR-0140's amendment. No new session was needed - thirty more day turns
              appended to the same PIE closed it.

              THE ENGINE HALF, WIRED 2026-09-18.

              THE PHASE 15 KERNEL IS BUILT AND RUN UNDER UBT. On 2026-09-18 the owner
              built the six modules Phase 15 has touched with MSVC 19.51 on Win64 and ran
              `Vaelen.Play 128 120` + `Vaelen.Stream.Write`. The four digests are
              IDENTICAL to a headless gcc/Linux run of the same wiring — state
              4c03becd9cd9c994, log dc418af0cd4b8d76, life 92f78aa85c0e1654, panel
              3106f24e235deeca — down to the person taken up (3535, region 26,
              Odordissuss) and the 36374 alive. Two compilers, two operating systems,
              one world, after four modules changed.

              THE GATE IS NOW A COMMAND. `VaelenAtlas --gate FILE` replays a walk
              recorded anywhere and reports clauses (a) and (c) to (e) as PASS/FAIL,
              printing clause (b)'s line in the shape Vaelen.Stream.Write prints it.
              `Run::DayWatch` is how it asks questions about every day of a walk without
              replaying the stream a second time; `Door.AWatchedReplayIsTheSameReplay`
              proves a watched replay is the same replay. CTest `Run.Gate` pins the five
              clause lines whole. ADR-0147.

              AND THE GATE CAUGHT ITSELF FIRST. Its first version reported two clauses
              failing on the checked-in walk, which `--walk` had written and declared
              sound. Both were the gate's own bugs and both were the same mistake:
              measuring a difference from a baseline never read. Promotions counted from
              0 rather than from the four hundred years the world had already run;
              takings counted by watching the played person change, when two of the four
              offer the same person back. Neither was found by reading the code — they
              were found because a second instrument disagreed.

              THE ENGINE HALF IS WRITTEN. `UVaelenWorldSubsystem::Watch` remembers where
              the host is looking and `AdvanceDay` hands it through `Door::Look` once
              per day turn, before turning it; `RegionUnderGround` inverts the drawer's
              own tile placement; `AVaelenPlayerController` reads the camera on the day
              key and nowhere else, because there is no Tick in that module and
              check_ui_fence.py refuses one by name. `Vaelen.Play <size> <years> 1` asks
              for the daily cadence, which is OFF by default so every Phase 14 number
              stands. `Vaelen.Look` and `Vaelen.Where` are the same inputs typed.
              UNVERIFIED under UBT: written and parsed against the shim, not yet built.

              AND FOUR UNREAL APIS THE PROJECT HAD NEVER COMPILED WERE REMOVED rather
              than verified (2026-09-19). A shim entry is a claim nothing in this
              repository can check: a wrong one makes the CI greener and surfaces two days
              later on the owner's machine. FMath::FloorToDouble became plain arithmetic
              after the bounds check that makes a cast equal a floor; Vaelen.Where takes
              whole centimetres through FCString::Atoi instead of fractions through Atod;
              UE_KINDA_SMALL_NUMBER became KINDA_SMALL_NUMBER, which VaelenViewDrawer.cpp
              has actually built; the helper calling GetPlayerViewPoint was made non-const
              so its constness stops mattering. Only FRotator::Vector() remains, with its
              evidence beside it - the built drawer compiles its exact inverse. ADR-0134's
              amendment: prefer the spelling the project has compiled over the one you are
              confident about, and keep the shim to what the project uses.

              AND THE SITTING COULD NOT HAVE SUCCEEDED (2026-09-21). The gate asks for
              four takings; the door takes somebody up on its own only when the played
              person dies, and measured rather than assumed, they do not - 4000 day turns
              at AELVOR 128, eleven years, and the first person taken up was still alive.
              A sitting would have recorded ONE taking and failed clause (e) after an
              evening's work. The host was missing a verb, not the world:
              UVaelenWorldSubsystem::TakeSomebodyElse and `Vaelen.TakeUp`. The whole
              sitting is now simulated headlessly with the host's own loop and passes all
              six clauses, and Door.TheHostsOwnSittingMeetsTheGate pins the shape.

              What remains: `Vaelen.Play 128 100 1`, then four lives of twenty-five days -
              Space twenty-five times, `Vaelen.TakeUp`, and again - then F9 and `--gate`.

              THE REVIEW of 2026-09-17, which rebuilt the headless half.

              THE REVIEW: 29 findings, 15 survived two refuters each, 15 fixed. What it
              found is worth more than the fixes. 15.10's stand-in was GREEN BY
              COINCIDENCE: Replay.Walked pinned the replay against itself, so it proved
              the replay stable and nothing about whether it was faithful. Under that
              were two real defects — Attention::Most changed the world and could not be
              replayed at all, and Replay applied a same-tick taking before the look it
              followed. And 15.07 did not do the one thing it exists for: a camera could
              hold a region for a whole played life and the world would never detail it,
              because NearDetail's holdings ate the detail budget. No test asserted it.

              --walk now replays its own walk into a fresh world BEFORE writing a byte
              and refuses to write one whose state digest differs. Turned on, it refused
              three walks in a row. The checked-in walk is regenerated with
              Attention::Most = 2 on purpose, so it can never again round-trip by
              agreeing with a default.

              Four assertions this phase failed or passed vacuously because their
              premise was assumed instead of constructed. In a simulated world the setup
              IS the experiment.


              AND A CORRECTION TO WHAT WAS ASKED OF THAT MACHINE. The September month is
              NOT invalidated by ADR-0141: Replay.Played replays it under the cadence it
              was made with, and does. What 15.10 needs is a SEPARATE recording - a
              walk, with streaming on - not a month replayed again. Two streams, two
              purposes.
              15.05 is covered now. The harness it wanted exists: the two systems that
              write RegionFaith are removed from the scheduler once the region is
              detailed, so the faiths hold still long enough for the crossing to be
              watched. The control was RUN and not reasoned about — the fix reverted,
              the suite rebuilt, 181 believers crossing into a destination with no room
              for their faith, the test failing; the fix restored, 0 crossing, the test
              passing. 181 lost believers per three years, in one crossing, in one
              world, is the size of the defect.
STATUS      : Phase 14's gate is met on (a), (b), (c) and (d). Eighty-three days
              played at the keyboard in UE 5.6 replay headlessly to the same four
              digests byte for byte; the HUD costs 0.52 ms of game thread and nothing
              on the GPU, over a scene that runs at about 122 fps against a bar of 90.
              Clause (e) is MET: CI run 215 on 4137b08 finished with ten jobs of ten
              green — six Linux presets, Windows MSVC, macOS AppleClang, clang-format
              and the engine-modules parse that carries Kernel.UiFence. PHASE 14 IS
              CLOSED. Phase 15 is broken down into 15.01-15.10 (ROADMAP section 21) and
              opens on five defects the planning found, all five confirmed in the code;
              15.01 to 15.04 are VALIDATED and 15.05 is INCOMPLETE by its own admission.

              WHAT THE RUN ACTUALLY COSTS, and a correction. The longest job,
              linux-clang-debug, took 119 minutes against timeout-minutes 180 — 66 % of
              its budget, 61 minutes spare. An earlier entry here called that ONE minute
              of headroom against a budget of 120. That was wrong: 120 was the budget
              ADR-0119 found too small in Phase 13, and the same ADR is what raised it
              to 180 and gave the long poles a COST so ctest starts them first. The
              number was repeated from a planning note instead of being read out of
              .github/workflows/kernel-ci.yml, which says 180 on the line above the
              matrix. Nothing needed fixing and nothing was changed.

              Run 215, job by job, so the next person reads figures rather than a mood:
              clang-format 7 s · parse 1 min 46 s · gcc-noasserts 10 min ·
              gcc-release 14 min · clang-noasserts 19 min · clang-release 24 min ·
              macOS 82 min · gcc-debug 94 min · Windows MSVC 99 min ·
              clang-debug 119 min. Budget 180 on the Linux, Windows and macOS legs,
              10 on clang-format and parse.

PROGRESS
████████████████████████░░ 95%

CURRENTLY
→ The first screen exists as text the kernel composes and the engine only copies. Nine view
  leaves, a 4064-byte page whose LAST row is its own digest, one module that holds the world
  (VaelenGame) and one that draws it and reads eight keys (VaelenUI), with a fence that reads
  every #include of the interface and refuses thirteen words the UI may not write.

  14.10 turned up the defect of the phase, and it was in the GAME and not in the test. The game
  offered eight verbs and could take seven: only one region of a played AELVOR was ever detailed
  (the busiest), while the LOD rules allow four, so the list of neighbours a Move may reach was
  empty on every day of every played world. Measured at 256 over thirty days: NearCount 0, thirty
  out of thirty. A month played at the keyboard meets the same wall. ADR-0139 is the fix - after a
  taking, the world asks for detail on the neighbours of the played person's region - and it moved
  exactly one frozen digest, the played page.

COMPLETED
✓ Phases 00-13 closed · Phase 13 gate PASSED 2026-09-14, 256 at 100 fps on a T400, the engine and
  the headless kernel agreeing on every figure
✓ 14.01 intents as a leaf · 14.02 the view headers as leaves + Take.h · 14.03 VaelenRun (Aelvor,
  Door, Replay) · 14.04 LifeView · 14.05 ChronicleView, incremental · 14.06 the page + two frozen
  digests · 14.07 the UI include fence
✓ 14.08 VaelenGame and 14.09 VaelenUI BUILT AND RUN - UnrealBuildTool on 2026-09-16 (UE 5.6,
  MSVC 19.51), after three defects nothing here could see; then eighty-three days played at the
  keyboard. No file in Source/ carries STATUS: UNVERIFIED any more.
✓ 14.10 clauses (a), (b), (d): the engine's two LogVaelenPlay lines and the headless replay of
  the same stream are byte-identical - same MD5 - and the panel digest 7de2c5faf3cc1813 is also
  the last line of the screenshot. 99 of 99 verdicts reproduced, 83 of 83 days.
✓ 14.10 headless half: Atlas --want-bound/--stand/--panel lines, a checked-in stream, CTest
  Replay.Played pinning four digests - replay: 256/100 + 30 days in 4.76 s (release), 25.21 s
  (debug), so the entry stays on windows-msvc-debug and clause (e) names eight legs
✓ ADR-0136 to ADR-0139

NEXT
→ Phase 15 — STREAMING & LOD. Nothing is asked of the engine machine any more: the build
  happened, the month was played, the frame rate was read. The phase-15 breakdown is planned
  headless-first and opens on three defects the planning itself found, all three confirmed in
  the code rather than suspected:
  · ReleaseDetail (Population/Lod.cpp) has NO caller outside the tests, while RequestDetail
    appends monotonically and refuses everything once LodState::MaxWanted = 8 is reached.
    Aelvor::NearDetail asks for up to three neighbours on EVERY taking and releases none, and
    Door::Day takes somebody new up whenever the played person dies. So after roughly three
    deaths the wanted list saturates, Near goes empty and every Move is refused TooFar for
    good — ADR-0139's fix expires. The checked-in month has exactly one taking, which is why
    CI is green and why the Phase 14 gate is still honest.
  · Aelvor.cpp's NearDetail reads a fresh `const LodRules Rules;` rather than the rules the
    LodSystem was built with. Not a defect today — both are default-constructed — but the day
    one is parameterised they part company silently.
  · This file listed the STAND-IN replay's four digests until 2026-09-16. Corrected above.

TESTS
✓ 181 CTest entries on linux-gcc-release, every gate of fifteen phases, Run.Checkpoint among them
✓ CI: 10 jobs - six Linux presets, clang-format 18, Windows MSVC, macOS AppleClang, and the engine
  modules parse (clang 18) that builds nothing and reads everything
✓ verify_fast: purity 216 files 0 violations · shim self-test 13 mutations · 13 translation units
  parsed · 4 wirings of AELVOR agree · UI fence 8 files, 16 mutations · AND, since 2026-09-21,
  every TU the change reaches COMPILED, syntax-only, with the build's own flags (ADR-0148). The
  six checks above it all read the source as text; none had ever handed a file to a compiler, and
  two commits shipped that would not build while all six were green. It refuses rather than skips
  when it has no configured build directory.
! THE GATE'S OWN 128 CELL IS 2.37 GB AND TWENTY MINUTES, measured 2026-09-21 on
  Options{128, Play, Stream, Lively, Colony} with 300+120 years: Begin() takes 1,217,277 ms and leaves
  3,289 MiB resident; BuildCheckpoint writes 2,368.6 MiB in 95,024 ms. Three such worlds SIGKILL a
  16 GB container. The "22 MB played AELVOR 128" quoted from 16.01 is a DIFFERENT wiring, and the
  "~145 ms rollback" derived from it in 16.03 is wrong by two orders of magnitude for this cell.
  Consequences, none of them yet fixed: ComputeStateDigest does a FULL SaveSnapshot per call (95 s and
  2.37 GB here, ~200 call sites); 16.03's atomic load doubles peak memory while it runs; and the 128
  gate cell cannot be an ordinary CTest entry.
! A CANCELLED CI RUN IS NOT A PASSED ONE. kernel-ci sets concurrency.cancel-in-progress and its
  long legs are budgeted at 180 minutes, so pushing faster than that kills the previous verdict.
  Runs 233, 234 and 235 are all cancelled; the branch's last finished run before 96e420f was
  6dd21ec on 2026-09-19 - before 15.10, before Phase 15 closed, before 16.01. Read the JOBS, and
  wait for them.
✓ Frozen and reproducing: the ADR-0135 pair (frame abc5a5767c6cf9dd, ground 8f7f4948f49b6e86), the
  view gate 115c2ff70a5327c4, the page empty 54787451e65766c1 and played 703c838ca533a095, and the
  replay's four, which are the OWNER'S MONTH and no longer the stand-in's (state
  2ffed5236a593c1b, log 1fdd4211695649bc, life 654e2de6455d8b7a, panel 7de2c5faf3cc1813 —
  the four Tests/Run/CMakeLists.txt pins)

BLOCKERS
! One CI run, green on the head, for clause (e): Replay.Played on the eight CTest legs
  and Kernel.UiFence in the parse job. Runs 208 and 209 were each cancelled by the next
  push; 210 is the first that can finish. Nothing else is outstanding for Phase 14.

  Carried into Phase 15, both measured on 2026-09-16 rather than feared: promoting one
  region to person-by-person detail costs about 65 ms (the first day turn after a taking
  is 340 ms against 78 ms with nobody taken up), and the 13.09 scene seen from ground
  level is GPU-bound at 30.4 ms on 954.8K primitives because every tile near the camera
  is drawn at full detail and nothing decides otherwise. Seen from its own camera the
  same scene is 8.16 ms. That gap is what Phase 15 exists to close.
  For the record, the three defects that stood between the code and the owner's first build,
  none visible headless: UnrealHeaderTool's generated destructor on an incomplete pimpl;
  DEFINE_VTABLE_PTR_HELPER_CTOR, which instantiates that same destructor whatever the class
  declares; and RegionGraphCache, exported whole while emitting neither of its implicit special
  members, because nothing inside VaelenSim ever constructs one. The last needs DLLs to show at
  all - CMake builds static libraries, where the class is present whether or not anybody
  exported it.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

```

## Phase 00 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 4)

| Task | Content | Status |
|---|---|---|
| 00.01 | Project architecture: `Vaelen.uproject`, targets, modules, CMake dual build, presets, CI, conventions | VALIDATED (headless) / VALIDATED (engine: first UBT build 2026-09-07, ARCHITECTURE section 8) |
| 00.02 | Core primitives: `CoreTypes`, `Version`, `Hash`, `Random`, `Ids` | VALIDATED: Hash 15, Random 29, Ids 19, Version 7, CoreTypes 1 tests |
| 00.03 | Logging and assertions: `Log`, `Assert` | VALIDATED: Log 23, LogFloor 1, Assert 33 (23 of them in the assertions-off build) |
| 00.04 | Test harness, runner, kernel purity checker | VALIDATED: Harness 5 tests, runner registry/shuffle/reverse entries, purity self-test 36 checks |
| 00.05 | Adversarial review, fixes, documentation | VALIDATED (headless): 8 review lenses, 189 findings, fixes committed in `7d41751`; docs refreshed |

## Phase 01 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 5)

| Task | Content | Status |
|---|---|---|
| 01.01 | Entity handles and registry | VALIDATED: EntityHandle 3, EntityRegistry 13 tests |
| 01.02 | Component type registry, sparse-set pools, store | VALIDATED: ComponentType 4, ComponentPool 8, ComponentStore 3 |
| 01.03 | Systems, deterministic scheduler, LOD periods | VALIDATED: Scheduler 8 |
| 01.04 | Simulation clock and calendar | VALIDATED: SimClock 4 |
| 01.05 | Events, append-only log, next-tick bus | VALIDATED: Event 2, EventLog 2, EventBus 6 |
| 01.06 | World, archives, versioned snapshot, plain-data rule | VALIDATED: Archive 4, World 3, Snapshot 8 |
| 01.07 | Deterministic replay gate | VALIDATED: Replay 5 (frozen hashes reproduced by clang, gcc, MSVC, AppleClang) |
| 01.08 | Abstract mini-world long-duration gate | VALIDATED: MiniWorld 4 (100 000 ticks, invariants, periodic replays, frozen end state) |

Phase 01 against the exit criteria of `Docs/ROADMAP.md` section 2: (1) green on the
whole CI matrix (runs 13 and 14 for 01.06 and 01.07: six Linux presets, clang-format,
Windows MSVC, macOS AppleClang; 01.08 is checked by the run of its own commit); (2)
determinism tests exist for every system: identical runs give identical digests, state
round-trips through snapshots, frozen values guard the RNG (Phase 00), the replay
reference and the mini-world end state; (3) no INCOMPLETE file; engine files UNVERIFIED;
(4) unit, integration (Scheduler, EventBus, Snapshot, Replay), deterministic, edge-case and
long-duration (MiniWorld) categories present; (5) ADR-0010 to ADR-0015 cover the phase's
decisions, docs updated. Verdict: **Phase 01 VALIDATED on the headless side, UNVERIFIED
on the engine side until the first UE 5.6 build.**

## Phase 02 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 6)

World-generation baselines at 1024 x 1024 (AELVOR seed, logged and not asserted): elevation 0.74 s, hydrology 0.36 s, regions 0.35 s, deposits 0.30 s in release; full pipeline 25 s in debug with assertions.

| Task | Content | Status |
|---|---|---|
| 02.01 | Grid, tile layers, world-gen config, WorldMap state block, save format 2 | VALIDATED: TileGrid 4, WorldMap 6 tests |
| 02.02 | Fix64 Q32.32 fixed point, deterministic lattice noise (value, gradient, fractal, warp) | VALIDATED: FixedPoint 4, Noise 5 tests |
| 02.03 | Elevation and coastline: continental mask, relief, ridges, edge sea, terrain flags, slope, ASCII export | VALIDATED: WorldGen 6 tests |
| 02.04 | Climate and biomes: sea distance, latitude and lapse, advected moisture with rain shadow, biome table, seasons hook | VALIDATED: Climate 6 tests |
| 02.05 | Hydrology: depression filling, D8 flow, accumulation, sediment fill vs lakes, rivers and lakes as entities | VALIDATED: Hydrology 5 tests |
| 02.06 | Regions: jittered-lattice seeds, terrain-cost growth, size-floor merging, entities, adjacency graph, the graph cache | VALIDATED: Regions 6 tests |
| 02.07 | Resource deposits: suitability rules, hashed draws, spacing cells, richness and tiers, entities | VALIDATED: Deposits 5 tests |
| 02.08 | Phase 02 gate: one-call pipeline, frozen whole-world digests at three sizes, regeneration, snapshot, edge cases, simulation over a generated world | VALIDATED: WorldPipeline 4 tests |

Phase 02 against the exit criteria of `Docs/ROADMAP.md` section 2: (1) green on the
whole CI matrix for 02.01 through 02.07 (runs 18 to 24: six Linux presets, clang-format,
Windows MSVC, macOS AppleClang), 02.08 checked by its own run; (2) every stage has a
frozen digest at 256 and the whole world at 64, 256 and 1024, regeneration is byte
identical and snapshots re-hash identically; (3) no INCOMPLETE file; engine files
UNVERIFIED; (4) unit (helpers, rules, fixed point), integration (pipeline, snapshot of
generated worlds), deterministic (frozen values everywhere), edge-case (drowned world,
1 x 1, non-square, two islands, synthetic ridge and basin) and long-duration (1024 x
1024 baselines per stage and for the pipeline, simulation and replay over a generated
world) categories present; (5) ADR-0016 to ADR-0023 cover the phase's decisions, docs
updated. Verdict: **Phase 02 VALIDATED on the headless side, UNVERIFIED on the engine
side until the first UE 5.6 build.**

## Phase 03 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 7)

| Task | Content | Status |
|---|---|---|
| 03.01 | Eras, era calendar, chronicle records, history queries | VALIDATED: History 3 tests |
| 03.02 | Culture entities, coarse population per region, growth, migration, assimilation, splits | VALIDATED: Population 5 tests |
| 03.03 | Languages per culture, phonology drift, pronounceable unique names for cultures, regions, rivers, lakes, eras | VALIDATED: Naming 5 tests |
| 03.04 | Religions born from events, believers per region, spread along the graph and with migration, schisms, tenets | VALIDATED: Religion 5 tests |
| 03.05 | Disasters and omens tied to the world, with deaths, faith and era consequences | VALIDATED: Disasters 5 tests |
| 03.06 | PreHistory object and one-call run, per-century frozen reference at 256, mid-history snapshot, 1024 baseline | VALIDATED: PreHistory 5 tests |
| 03.07 | Why-queries, region timelines, the chronicle as deterministic text | VALIDATED: HistoryText 5 tests |
| 03.08 | Phase 03 gate: 2000 years at 256 with invariants every decade, frozen at 1000 / 1500 / 2000, snapshot at year 1000 | VALIDATED: HistoryGate 2 tests |

Phase 03 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green
for 03.01 to 03.07 (runs 26 to 32, run 29 red on MSVC and fixed in 03.05), 03.08 by its own
run; (2) determinism tests for every system, frozen digests per task and per century,
reproduced by clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files
UNVERIFIED; (4) unit, integration, deterministic, edge-case and long-duration categories
present (2000 years at 256, 500 years at 1024); (5) ADR-0024 to ADR-0031, docs updated.
Verdict: **Phase 03 VALIDATED on the headless side, UNVERIFIED on the engine side until
the first UE 5.6 build.**

## Phase 04 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 8)

| Task | Content | Status |
|---|---|---|
| 04.01 | `VaelenPopulation` module, `PersonInfo`, promotion of a region into persons and demotion back, consistency, the index counter | VALIDATED: Persons 6 tests |
| 04.02 | LifeSystem (ageing, mortality, fertility, reconciliation), RegionLod marker observed by the coarse systems | VALIDATED: Lives 5 tests |
| 04.03 | FamilySystem (marriages, families, heads, extinction), lineage queries, births to couples | VALIDATED: Families 5 tests |
| 04.04 | PersonNeeds and NeedSystem (rations, famine from drought, disease from plague, deaths with causes) | VALIDATED: Needs 6 tests |
| 04.05 | PersonTraits and TraitSystem (traits from identity and parents, skills through life, names in the person's language) | VALIDATED: Traits 5 tests |
| 04.06 | LodState, LodSystem (requests, promotions, demotions, crossings both ways) | VALIDATED: Lod 5 tests |
| 04.07 | PersonChronicle listener, person lines, unified chronicle, stories and the why of a death | VALIDATED: PersonHistory 5 tests |
| 04.08 | Phase 04 gate: 500 years at 256 with a detailed region and every Phase 04 system, invariants every decade, frozen at 250 / 500, snapshot at year 250 | VALIDATED: PopulationGate 1 test |

Phase 04 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green
for 04.01 to 04.07 (runs 34 to 39), 04.08 by its own run; (2) determinism tests for every
system, frozen digests per task and for the gate at 250 and 500 years, reproduced by
clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files UNVERIFIED; (4)
unit, integration, deterministic, edge-case and long-duration categories present (500
years at 256 with a detailed region, 500 years of LOD alternation at 64); (5) ADR-0032 to
ADR-0039, docs updated. Verdict: **Phase 04 VALIDATED on the headless side, UNVERIFIED on
the engine side until the first UE 5.6 build.**

## Phase 05 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 9)

| Task | Content | Status |
|---|---|---|
| 05.01 | `VaelenSociety` module, organisations as entities (councils, temples), seats, heads, coarse counts | VALIDATED: Organizations 5 tests |
| 05.02 | PersonStanding and StandingSystem (score from house, age, traits, skills and offices; ranks and tiers; the elite of a region) | VALIDATED: Standing 4 tests |
| 05.03 | NormSet per culture and NormSystem (customs from identity and parent, drifts on schisms and disasters), MarriageNorms observed by the family system | VALIDATED: Norms 4 tests |
| 05.04 | BondState, RegionStrata and BondageSystem (debt and birth entries, hardening, manumission, flight, holders, strata per region) | VALIDATED: Bondage 4 tests |
| 05.05 | DecisionSystem (grain against drought, preaching, training, raids planned), guilds and warbands, RegionStores observed by the need system | VALIDATED: Decisions 4 tests |
| 05.06 | Strata honoured at a promotion (Promotion entry), departures release bonds, 500 years of alternation | VALIDATED: Strata 3 tests |
| 05.07 | SocietyChronicle listener, society lines, unified chronicle, the why of a decision | VALIDATED: SocietyHistory 3 tests |
| 05.08 | Phase 05 gate: 500 years at 256 with a detailed region and every Phase 04 and 05 system, invariants every decade, frozen at 250 / 500 with the log and the text, snapshot at year 250 | VALIDATED: SocietyGate 1 test |

Phase 05 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green
for 05.01 to 05.07 (runs 42 to 48), 05.08 by its own run; (2) determinism tests for every
system, frozen digests per task and for the gate at 250 and 500 years, reproduced by
clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files UNVERIFIED; (4)
unit, integration, deterministic, edge-case and long-duration categories present (500
years at 256 with a detailed region, 500 years of alternation at 64 with every system);
(5) ADR-0040 to ADR-0047, docs updated. Verdict: **Phase 05 VALIDATED on the headless
side, UNVERIFIED on the engine side until the first UE 5.6 build.**

## Phase 06 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 10)

| Task | Content | Status |
|---|---|---|
| 06.01 | `VaelenEconomy` module, goods as kinds, RegionStock and HouseStock, the endowment, split and fold across the grains, AddStock | VALIDATED: Stocks 4 tests |
| 06.02 | ProductionSystem: harvest by workers and farming cut by droughts, spoilage, meals by house, the ration observed by the needs, deposits, craft | VALIDATED: Production 4 tests |
| 06.03 | MarketSystem: a market per region, integer prices from wanted over held within a floor and a ceiling, price events with the harvest as cause, ValueOf | VALIDATED: Markets 4 tests |
| 06.04 | TradeSystem: routes between neighbouring markets on price gaps, goods carried cheap to dear, idle routes closed, settlements founded and abandoned | VALIDATED: Trade 4 tests |
| 06.05 | WealthSystem: houses valued and ranked at their market, the rank weighing in standing, heirs by descent custom, goods passing to the heir | VALIDATED: Wealth 5 tests |
| 06.06 | The economy across the grains: wealth and heirs cleared with the grain, conservation to the unit over 500 years, every invariant in a living world | VALIDATED: Grains 2 tests |
| 06.07 | EconomyChronicle: roads, towns, prices at their bounds, shortfalls, fortunes and inheritances recorded; a line for every economic event; the why of a dear loaf | VALIDATED: EconomyHistory 3 tests |
| 06.08 | Phase 06 gate: 500 years at 256 with every Phase 04, 05 and 06 system, invariants every decade, frozen at 250 and 500 with the log and the text | VALIDATED: EconomyGate 1 test |

## Phase 07 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 11)

| Task | Content | Status |
|---|---|---|
| 07.01 | `VaelenPolitics` module, polities as entities founded on councils, the seat and the ruler, belonging written on the region itself | VALIDATED: Polities 4 tests |
| 07.02 | Law as one number on the polity, written onto the regions as dues the economy observes; the grain taken into a treasury | VALIDATED: Law 4 tests |
| 07.03 | Authority written on each region, falling with distance from the seat; the upkeep of a word; ground taken and ground that slips | VALIDATED: Reach 5 tests |
| 07.04 | The line a polity remembers, the claimant the descent custom names, and the unrest a seat taken by anyone else costs its hold | VALIDATED: Succession 4 tests |
| 07.05 | Factions as entities of their own kind: a grievance with a place and sometimes a person, gathering while unanswered and taking the ground at its threshold | VALIDATED: Factions 4 tests |
| 07.06 | Relations as entities of their own kind, warmed and cooled by the world two polities share; a war puts the weaker border in play and the reach system takes it | VALIDATED: Diplomacy 4 tests |
| 07.07 | A sentence for every political event and a record only for what a century would remember | VALIDATED: PoliticsHistory 3 tests |
| 07.08 | Phase 07 gate: 500 years at 256 with two powers and every Phase 04 to 07 system, invariants every decade, frozen at 250 and 500 with the log and the text | VALIDATED: PoliticsGate 1 test |

Phase 07 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green for 07.01 to 07.07 (runs 60 to 67; runs 60 and 65 were superseded on the same headless tree), 07.08 by its own run; (2) determinism tests for every system, frozen digests per task and for the gate at 250 and 500 years, reproduced by clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files UNVERIFIED; (4) unit, integration, deterministic, edge-case and long-duration categories present (500 years at 256 with two detailed regions); (5) ADR-0056 to ADR-0063, docs updated. Verdict: **Phase 07 VALIDATED on the headless side, UNVERIFIED on the engine side until the next UE 5.6 build.**

## Phase 08 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 12)

| Task | Content | Status |
|---|---|---|
| 08.01 | `VaelenMilitary` module; a levy raised from the regions a polity holds and fed out of its treasury; armies as entities of kind Army | VALIDATED: Armies 4 tests |
| 08.02 | Marching: a host walks the region graph towards the nearest enemy ground, one hop a season, eats off what it stands on and loosens an enemy ruler's grip | VALIDATED: March 4 tests |
| 08.03 | Battle: two hosts of powers at war on one region settled in a year by strength, whose ground it is, and a stream fixed by the world seed | VALIDATED: Battle 4 tests |
| 08.04 | Siege: a host sitting before an enemy seat, the only way a capital ever changes hands, and the polity that loses one ends | VALIDATED: Siege 4 tests |
| 08.05 | War as a thing with a beginning and an end: opened by a relation turning, held open while it runs, closed by exhaustion, with the stance of 07.06 following it | VALIDATED: War 4 tests |
| 08.06 | What war costs the living: the dead where they came from, the standing of those who came back, the people who would not stay | VALIDATED: Toll 4 tests |
| 08.07 | War in the chronicle: a sentence for every military event, the few a century keeps, and the why of a lost province walked back to the battle | VALIDATED: MilitaryHistory 4 tests |
| 08.08 | Phase 08 gate: five centuries at 256 with every Phase 04 to 08 system, every invariant each decade, a snapshot replayed, four frozen digests | VALIDATED: MilitaryGate 1 test |

## Phase 09 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 13)

| Task | Content | Status |
|---|---|---|
| 09.01 | `VaelenInfrastructure` module; buildings as entities of kind Building raised out of a region's common stock and the hands it can spare, what they cost taken in the log | PROTOTYPE: Buildings 4 tests |
| 09.02 | What a building does: the granary of 05.05, the harvest of 06.02, the wall of 08.04, each as the one number that phase already reads | PROTOTYPE: Works 5 tests |
| 09.03 | Settlements as places: a tile of their own, a size from the people and the traffic, and the region's works standing in them | PROTOTYPE: Places 4 tests |
| 09.04 | Roads: a route of 06.04 cut out of the two ends together, carrying more, worn when nobody keeps it, fallen back to a track | PROTOTYPE: Roads 5 tests |
| 09.05 | Decay and ruins: everything built falls unless it is kept, faster where the weather or a war struck, and a ruin makes the next one cheaper | PROTOTYPE: Decay 5 tests |
| 09.06 | Logistics: one number on the ground from the best road touching it, read by the marching of 08.02 and the reach of 07.03 | PROTOTYPE: Logistics 5 tests |
| 09.07 | Infrastructure in the chronicle: a granary raised, a mill fallen in, a road cut, a town settled, and the why of a fall walked back to the flood | PROTOTYPE: WorksHistory 5 tests |
| 09.08 | Phase 09 gate: five centuries at 256 with every Phase 04 to 09 system, every invariant each decade, a snapshot replayed, four frozen digests | VALIDATED: InfrastructureGate 2 tests |

Phase 09 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) six Linux presets green with every gate, 109 CTest entries each and 0 warnings; (2) determinism tests for every system, frozen digests per task and for the gate at 250 and 500 years with the log and the text; (3) no PROTOTYPE or INCOMPLETE file in the module, the two engine-facing files UNVERIFIED; (4) unit, integration, deterministic, edge, text and long-duration categories present, and the one gap 09.05 disclosed closed by the gate; (5) ADR-0074 to ADR-0081, docs updated. Verdict: **Phase 09 VALIDATED on the headless side, UNVERIFIED on the engine side until the next UE 5.6 build.**

Phase 10 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI run 108 on `494aa30`, all
nine jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44, macOS 15 AppleClang -
including `Kernel.Purity` and `Kernel.PuritySelfTest`; (2) determinism tests for every system of the
phase, frozen digests per task and for the gate at the half and the end with the log and the life,
and a replay that reproduces a life from seed, takings and intents; (3) no INCOMPLETE file in the
module - every file is PROTOTYPE and listed below with its limits, the two engine-facing files
UNVERIFIED until the next UE 5.6 build; (4) unit, integration, deterministic, edge, text, replay and
long-duration categories all present; (5) ADR-0082 to ADR-0089 written, docs updated, and the
verification record below carries what the matrix found rather than a summary of it. Verdict:
**Phase 10 VALIDATED on the headless side, UNVERIFIED on the engine side until the next UE 5.6
build.**

One limit stated plainly, because it is a property of the world and not of the module: at AELVOR 256
a bound person of a crowded region does not live forty years. The gate plays its forty across three
lives, and whether that mortality is this world being harsh or the ration of 04.04 being wrong at
that density is 11.05's question.

## Phase 10 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 14)

| Task | Content | Status |
|---|---|---|
| 10.01 | `VaelenPlayer` module; the player as a mark on an existing person of a detailed region, carrying no state of its own | PROTOTYPE: Player 4 tests |
| 10.02 | The enslaved start: a life found among the people 05.04 had already bound, never written into the world | PROTOTYPE: Start 4 tests |
| 10.03 | The player's grain: the day for one person while the world runs at the year, granting hours and writing no history | PROTOTYPE: Hours 5 tests |
| 10.04 | Intent as commands: a queue the outside submits to, and a system inside the simulation that is the only thing allowed to act on it, so a recorded stream replays to the same life | PROTOTYPE: Commands 6 tests |
| 10.05 | What the player can do: work, rest, eat, move, speak, give, take, each through the system that already owns that change | PROTOTYPE: Doings 6 tests (+1 in Population) |
| 10.06 | What the people around them make of them: opinions read out of the acts in the log, weighed by the standing of whoever holds them | PROTOTYPE: Regard 6 tests |
| 10.07 | The player in the chronicle: a life as records, every event with a sentence, and the why walked back through every layer below | PROTOTYPE: LifeHistory 6 tests |
| 10.08 | Phase 10 gate: forty years at 256 played across three lives out of a recorded stream, every invariant each decade, and the stream replayed to the same life | PROTOTYPE: PlayerGate 1 test |

## Phase 11 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 15)

| Task | Content | Status |
|---|---|---|
| 11.01 | A region the world keeps detailed by policy rather than by crowding, and what that costs | VALIDATED (headless) |
| 11.02 | Its people live at the day while the world keeps the year; a year of days sums to the year exactly | VALIDATED (headless) |
| 11.03 | Ore lifted from a seam that runs out, credited through `AddStock` and nothing else | VALIDATED (headless) |
| 11.04 | Founded bound, held by the colony: bondage cannot GROW into a colony and no elite could carry it | VALIDATED (headless) |
| 11.05 | The bond does not shorten a life; the ground does | VALIDATED (headless) |
| 11.06 | A subsistence world cannot feed a colony, so a colony feeds itself | VALIDATED (headless) |
| 11.07 | What the world does with no event behind it cannot be remembered | VALIDATED (headless) |
| 11.08 | The gate: a century of the colony at full detail with the world at the year | VALIDATED (headless) |

**Phase 11 CLOSED** — CI run 113 green on all nine jobs; engine side UNVERIFIED under UBT.

## Phase 12 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 16)

| Task | Content | Status |
|---|---|---|
| 12.01 | `VaelenGameplay`; a person nobody plays acting through the verbs of 10.05, restricted to move and say | VALIDATED (headless) |
| 12.02 | An opinion between any two people, and the thing no layer had: something HEARD rather than suffered | VALIDATED (headless) |
| 12.03 | Documents that outlive their writer, are copied, are lost, and are wrong when the writer was | VALIDATED (headless) |
| 12.04 | Maps, the one document the world can check — and a reader who names a forged road as readily as a real one | VALIDATED (headless) |
| 12.05 | A name held by a PLACE, travelling the trade routes and thinner at every crossing | VALIDATED (headless) |
| 12.06 | The first cost a reputation carries: the worst-named bound, the best-named among the bound let go | VALIDATED (headless) |
| 12.07 | A chronicle of what was BELIEVED, so a bondage walks back to what a place was TOLD | VALIDATED (headless) |
| 12.08 | The gate: a century at 256, 14 400 intents over two lives, 327 267 acts, replayed to seven digests | VALIDATED (headless) |

**Phase 12 CLOSED.**

## Phase 13 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 17)

| Task | Content | Status |
|---|---|---|
| 13.01 | A read-only VIEW of the world for a frame, taken once per frame, never written to | VALIDATED (headless) |
| 13.02 | What changed since the last frame: the delta that rebuilds the new frame byte for byte | VALIDATED (headless) |
| 13.03 | Level of detail for the EYE, which is not the simulation's | VALIDATED (headless) |
| 13.04 | The view under a snapshot and a reload, because a frame taken across a save must not tear | VALIDATED (headless) |
| 13.05 | Kernel gate: a century at 256, a year watched frame by frame, the screen built of deltas alone | VALIDATED (headless) |
| 13.06 | The first UBT build of all eleven kernel modules — the task the UNVERIFIED marks waited for since Phase 00 | VALIDATED (engine) |
| 13.07a-c | The ground in the view, the world drawn from it, and `VaelenPresentation` | VALIDATED |
| 13.08a-f | The network, the world in time, what happened, why, and the page checked | VALIDATED |
| 13.09 | The gate: the editor open on AELVOR at 256, a century running, the frame rate written down | VALIDATED (engine) |

**Phase 13 CLOSED 2026-09-14** — gate PASSED at 100 fps on a T400; the engine and the
headless kernel agree on every figure at 256. This is the phase that turned the oldest
UNVERIFIED marks in the repository.

## Phase 14 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 20)

| Task | Content | Status |
|---|---|---|
| 14.01 | The command surface as a leaf (`Player/Intent.h`), and the input stream | VALIDATED (headless) |
| 14.02 | The view headers as leaves, `Take.h` the one header naming the World | VALIDATED (headless) |
| 14.03 | `VaelenRun`, the thirteenth kernel module: `Aelvor`, `Door`, `Replay` | VALIDATED (headless) |
| 14.04 | `LifeView` — the played life in one view | VALIDATED (headless) |
| 14.05 | `ChronicleView` — the last lines about the played person, taken incrementally | VALIDATED (headless) |
| 14.06 | The page (`View/Panel.h`), composed kernel-side, its last row its own digest | VALIDATED (headless) |
| 14.07 | The UI include fence: the restricted parse, `check_ui_fence.py`, the shim | VALIDATED (the scripts) |
| 14.08 | `VaelenGame` — the one place a world is held | **UNVERIFIED** (written, parsed, fenced; never built by UBT) |
| 14.09 | `VaelenUI` — the HUD that copies the page, eight keys, no asset and no Blueprint | **UNVERIFIED** (same) |
| 14.10 | The gate: a month played by hand at 256, replayed by no hand | **INCOMPLETE** — (a), (b), (d) VALIDATED on the owner's build of 2026-09-16 and CTest `Replay.Played`; (c) measured at 35.52 fps against a `>= 90` clause, over a scene with nothing drawn, so it is neither met nor fairly falsified |

Phase 14 against the exit criteria of `Docs/ROADMAP.md` section 2: (1) the CI matrix is
green apart from what it cannot build; (2) determinism is the gate itself — a recorded
month replays to four identical digests; (3) two files stay UNVERIFIED and say so;
(4) unit, deterministic, frozen-digest and replay categories present; (5) ADR-0136 to
ADR-0139 cover the phase's decisions. Verdict: **Phase 14 NOT closed.** The one thing
missing is the owner's UE 5.6 build.

## File status

Every file under `Source/`, `Tests/` and `Tools/` carries a `// STATUS:` line (rule R5 of
the purity checker, applied to headers and sources).

### Source/

| File | STATUS |
|---|---|
| `VaelenCore/Public/Vaelen/Core/CoreTypes.h` | VALIDATED (Phase 00) — integration and long-duration tests deferred to Phase 01 |
| `VaelenCore/Public/Vaelen/Core/Version.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Assert.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Log.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Hash.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Random.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Ids.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Private/Assert.cpp`, `Log.cpp`, `Random.cpp`, `Ids.cpp`, `Version.cpp` | VALIDATED (Phase 00) — covered by the matching `Tests/Core/Test_*.cpp` |
| `VaelenCore/Private/VaelenCoreModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) |
| `VaelenCore/VaelenCore.Build.cs` | VALIDATED (UE 5.6, 2026-09-07) |
| `Vaelen.Target.cs`, `VaelenEditor.Target.cs` | no STATUS line; `VaelenEditor` built 2026-09-07, the game target only had its rules assembly compiled |
| `Vaelen/Vaelen.Build.cs`, `Vaelen/Public/Vaelen.h`, `Vaelen/Private/Vaelen.cpp`, `VaelenLogSink.h/.cpp` | VALIDATED (UE 5.6, 2026-09-07) |

### Source/VaelenSim (Phase 01)

| File | STATUS |
|---|---|
| `Public/Vaelen/Sim/SimApi.h`, `PlainData.h`, `EntityHandle.h`, `EntityRegistry.h`, `ComponentType.h`, `ComponentPool.h`, `ComponentStore.h`, `SimClock.h`, `System.h`, `Event.h`, `EventBus.h`, `Archive.h`, `World.h`, `Snapshot.h`, `Private/EntityRegistry.cpp`, `ComponentType.cpp`, `ComponentStore.cpp`, `Scheduler.cpp`, `EventBus.cpp`, `Archive.cpp`, `World.cpp`, `Snapshot.cpp` | VALIDATED (Phase 01) — integration and long-duration tests arrive with 01.07 / 01.08 |
| `Private/VaelenSimModule.cpp`, `VaelenSim.Build.cs` | VALIDATED (UE 5.6, 2026-09-07) |
| `Public/Vaelen/Sim/TileGrid.h`, `WorldMap.h`, `Private/WorldMap.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_TileGrid.cpp`, `Test_WorldMap.cpp` |
| `Public/Vaelen/Sim/FixedPoint.h`, `Noise.h`, `Private/Noise.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_FixedPoint.cpp`, `Test_Noise.cpp` |
| `Public/Vaelen/Sim/WorldGen.h`, `Private/WorldGen.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_WorldGen.cpp`, `Test_Climate.cpp` |
| `Public/Vaelen/Sim/Hydrology.h`, `Private/Hydrology.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_Hydrology.cpp` |
| `Public/Vaelen/Sim/Regions.h`, `Private/Regions.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_Regions.cpp` |
| `Public/Vaelen/Sim/Deposits.h`, `Private/Deposits.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_Deposits.cpp` |
| `Public/Vaelen/Sim/WorldGenPipeline.h`, `Private/WorldGenPipeline.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_WorldPipeline.cpp` |
| `Public/Vaelen/Sim/History.h`, `Private/History.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_History.cpp` |
| `Public/Vaelen/Sim/Population.h`, `Private/Population.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Population.cpp` |
| `Public/Vaelen/Sim/Naming.h`, `Private/Naming.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Naming.cpp` |
| `Public/Vaelen/Sim/Religion.h`, `Private/Religion.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Religion.cpp` |
| `Public/Vaelen/Sim/Disasters.h`, `Private/Disasters.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Disasters.cpp` |
| `Public/Vaelen/Sim/PreHistory.h`, `Private/PreHistory.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_PreHistory.cpp` |
| `Public/Vaelen/Sim/HistoryText.h`, `Private/HistoryText.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_HistoryText.cpp` |

### Source/VaelenPopulation (Phase 04)

| File | STATUS |
|---|---|
| `VaelenPopulation.Build.cs`, `Private/VaelenPopulationModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED — six Linux presets |
| `Public/Vaelen/Population/PopulationApi.h` | VALIDATED (Phase 04) |
| `Public/Vaelen/Population/Persons.h`, `Private/Persons.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Persons.cpp` |
| `Public/Vaelen/Population/Lives.h`, `Private/Lives.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Lives.cpp` |
| `Public/Vaelen/Population/Families.h`, `Private/Families.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Families.cpp` |
| `Public/Vaelen/Population/Needs.h`, `Private/Needs.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Needs.cpp` |
| `Public/Vaelen/Population/Traits.h`, `Private/Traits.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Traits.cpp` |
| `Public/Vaelen/Population/Lod.h`, `Private/Lod.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Lod.cpp` |
| `Public/Vaelen/Population/PersonHistory.h`, `Private/PersonHistory.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_PersonHistory.cpp` |

### `Source/VaelenSociety` (Phase 05)

| File | Status |
|---|---|
| `VaelenSociety.Build.cs`, `Private/VaelenSocietyModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED (Phase 05) |
| `Public/Vaelen/Society/SocietyApi.h` | VALIDATED (Phase 05) |
| `Public/Vaelen/Society/Organizations.h`, `Private/Organizations.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Organizations.cpp` |
| `Public/Vaelen/Society/Standing.h`, `Private/Standing.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Standing.cpp` |
| `Public/Vaelen/Society/Norms.h`, `Private/Norms.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Norms.cpp` |
| `Public/Vaelen/Society/BondState.h`, `Public/Vaelen/Society/Bondage.h`, `Private/Bondage.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Bondage.cpp` |
| `Public/Vaelen/Society/Decisions.h`, `Private/Decisions.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Decisions.cpp` |
| `Public/Vaelen/Society/SocietyHistory.h`, `Private/SocietyHistory.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_SocietyHistory.cpp` |

### `Source/VaelenEconomy` (Phase 06)

| File | Status |
|---|---|
| `VaelenEconomy.Build.cs`, `Private/VaelenEconomyModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED (Phase 06) |
| `Public/Vaelen/Economy/EconomyApi.h` | VALIDATED (Phase 06) |
| `Public/Vaelen/Economy/Stocks.h`, `Private/Stocks.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Stocks.cpp` |
| `Public/Vaelen/Economy/Production.h`, `Private/Production.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Production.cpp` |
| `Public/Vaelen/Economy/Markets.h`, `Private/Markets.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Markets.cpp` |
| `Public/Vaelen/Economy/Trade.h`, `Private/Trade.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Trade.cpp` |
| `Public/Vaelen/Economy/Wealth.h`, `Private/Wealth.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Wealth.cpp` |
| `Public/Vaelen/Economy/EconomyHistory.h`, `Private/EconomyHistory.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_EconomyHistory.cpp` |

### `Source/VaelenMilitary` (Phase 08)

| File | Status |
|---|---|
| `VaelenMilitary.Build.cs`, `Private/VaelenMilitaryModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | VALIDATED (Phase 08) |
| `Public/Vaelen/Military/MilitaryApi.h` | VALIDATED (Phase 08) |
| `Public/Vaelen/Military/Armies.h`, `Private/Armies.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Armies.cpp` |
| `Public/Vaelen/Military/March.h`, `Private/March.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_March.cpp` |
| `Public/Vaelen/Military/Battle.h`, `Private/Battle.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Battle.cpp` |
| `Public/Vaelen/Military/Siege.h`, `Private/Siege.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Siege.cpp` |
| `Public/Vaelen/Military/War.h`, `Private/War.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_War.cpp` |
| `Public/Vaelen/Military/Toll.h`, `Private/Toll.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Toll.cpp` |
| `Public/Vaelen/Military/MilitaryHistory.h`, `Private/MilitaryHistory.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_MilitaryHistory.cpp` |

### `Source/Vaelen` (engine bridge and presentation)

| File | Status |
|---|---|
| `Vaelen.Build.cs`, `Private/Vaelen.cpp`, `Private/VaelenLogSink.h/.cpp`, `Public/Vaelen.h` | VALIDATED under UBT (UE 5.6, 2026-09-07, editor) |
| `Public/VaelenAtlasActor.h`, `Private/VaelenAtlasActor.cpp` | VALIDATED under UBT (UE 5.6, 2026-09-08) — compiled clean and run through the `Vaelen.Atlas` console command, which generated AELVOR 128 and reported it |

### `Source/Vaelen` (engine bridge and presentation)

| File | Status |
|---|---|
| `Vaelen.Build.cs`, `Private/Vaelen.cpp`, `Private/VaelenLogSink.h/.cpp`, `Public/Vaelen.h` | VALIDATED under UBT (UE 5.6, 2026-09-07, editor) |
| `Public/VaelenAtlasActor.h`, `Private/VaelenAtlasActor.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |

### `Source/VaelenPolitics` (Phase 07)

| File | Status |
|---|---|
| `VaelenPolitics.Build.cs`, `Private/VaelenPoliticsModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | VALIDATED (Phase 07) |
| `Public/Vaelen/Politics/PoliticsApi.h` | VALIDATED (Phase 07) |
| `Public/Vaelen/Politics/Polities.h`, `Private/Polities.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Polities.cpp` |
| `Public/Vaelen/Politics/Law.h`, `Private/Law.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Law.cpp` |
| `Public/Vaelen/Politics/Reach.h`, `Private/Reach.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Reach.cpp` |
| `Public/Vaelen/Politics/Succession.h`, `Private/Succession.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Succession.cpp` |
| `Public/Vaelen/Politics/Factions.h`, `Private/Factions.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Factions.cpp` |
| `Public/Vaelen/Politics/Diplomacy.h`, `Private/Diplomacy.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Diplomacy.cpp` |
| `Public/Vaelen/Politics/PoliticsHistory.h`, `Private/PoliticsHistory.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_PoliticsHistory.cpp` |
| `Public/Vaelen/Politics/Law.h`, `Private/Law.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Law.cpp` |
| `Public/Vaelen/Politics/Law.h`, `Private/Law.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Law.cpp` |

### Tests/

| File | STATUS | Tests |
|---|---|---|
| `Harness/VaelenTest.h`, `Harness/TestMain.cpp` | VALIDATED (Phase 00) | — |
| `Core/Test_Assert.cpp` | VALIDATED | 33 (23 build-independent) |
| `Core/Test_CoreTypes.cpp` | VALIDATED | 1 + compile-time asserts |
| `Core/Test_Harness.cpp` | VALIDATED | 5 |
| `Core/Test_Hash.cpp` | VALIDATED | 15 |
| `Core/Test_Ids.cpp` | VALIDATED | 19 |
| `Core/Test_Log.cpp` | VALIDATED | 23 |
| `Core/Test_LogFloor.cpp` | VALIDATED | 1 |
| `Core/Test_Random.cpp` | VALIDATED | 29 |
| `Core/Test_Version.cpp` | VALIDATED | 7 |
| `Sim/Test_EntityHandle.cpp`, `Test_EntityRegistry.cpp` | VALIDATED | 3, 13 |
| `Sim/Test_ComponentType.cpp`, `Test_ComponentPool.cpp`, `Test_ComponentStore.cpp` | VALIDATED | 4, 8, 3 |
| `Sim/Test_Scheduler.cpp`, `Test_SimClock.cpp` | VALIDATED | 8, 4 |
| `Sim/Test_Event.cpp`, `Test_EventLog.cpp`, `Test_EventBus.cpp` | VALIDATED | 2, 2, 6 |
| `Sim/Test_Archive.cpp`, `Test_World.cpp`, `Test_Snapshot.cpp` | VALIDATED | 4, 3, 8 |
| `Sim/Test_Replay.cpp` (deterministic + integration gate) | VALIDATED | 5 |
| `Sim/Test_MiniWorld.cpp` (long-duration gate) | VALIDATED | 4 |
| `Sim/Test_TileGrid.cpp`, `Test_WorldMap.cpp` (Phase 02) | VALIDATED | 4, 6 |
| `Sim/Test_FixedPoint.cpp`, `Test_Noise.cpp` (Phase 02) | VALIDATED | 4, 5 |
| `Sim/Test_WorldGen.cpp` (Phase 02) | VALIDATED | 6 |
| `Sim/Test_Climate.cpp` (Phase 02) | VALIDATED | 6 |
| `Sim/Test_Hydrology.cpp` (Phase 02) | VALIDATED | 5 |
| `Sim/Test_Regions.cpp` (Phase 02) | VALIDATED | 6 |
| `Sim/Test_Deposits.cpp` (Phase 02) | VALIDATED | 5 |
| `Sim/Test_WorldPipeline.cpp` (Phase 02 gate) | VALIDATED | 4 |
| `Sim/Test_History.cpp` (Phase 03) | VALIDATED | 3 |
| `Sim/Test_Population.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_Naming.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_Religion.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_Disasters.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_PreHistory.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_HistoryText.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_HistoryGate.cpp` (Phase 03 gate) | VALIDATED | 2 |
| `Population/Test_Persons.cpp` (Phase 04) | VALIDATED | 6 |
| `Population/Test_Lives.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Families.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Needs.cpp` (Phase 04) | VALIDATED | 6 |
| `Population/Test_Traits.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Lod.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_PersonHistory.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_PopulationGate.cpp` (Phase 04) | VALIDATED | 1 |
| `Society/Test_Organizations.cpp` (Phase 05) | VALIDATED | 5 |
| `Society/Test_Standing.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Norms.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Bondage.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Decisions.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Strata.cpp` (Phase 05) | VALIDATED | 3 |
| `Society/Test_SocietyHistory.cpp` (Phase 05) | VALIDATED | 3 |
| `Society/Test_SocietyGate.cpp` (Phase 05) | VALIDATED | 1 |
| `Economy/Test_Stocks.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Production.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Markets.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Trade.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Wealth.cpp` (Phase 06) | VALIDATED | 5 |
| `Economy/Test_Grains.cpp` (Phase 06) | VALIDATED | 2 |
| `Economy/Test_EconomyHistory.cpp` (Phase 06) | VALIDATED | 3 |
| `Economy/Test_EconomyGate.cpp` (Phase 06) | VALIDATED | 1 |
| `Politics/Test_Polities.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Law.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Reach.cpp` (Phase 07) | VALIDATED | 5 |
| `Politics/Test_Succession.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Factions.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Diplomacy.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_PoliticsHistory.cpp` (Phase 07) | VALIDATED | 3 |
| `Politics/Test_PoliticsGate.cpp` (Phase 07) | VALIDATED | 1 |
| `Military/Test_Armies.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_March.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_Battle.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_Siege.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_War.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_Toll.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_MilitaryHistory.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_MilitaryGate.cpp` (Phase 08) | VALIDATED | 1 |
| `Infrastructure/Test_Buildings.cpp` (Phase 09) | VALIDATED | 4 |
| `Infrastructure/Test_Works.cpp` (Phase 09) | VALIDATED | 5 |
| `Infrastructure/Test_Places.cpp` (Phase 09) | VALIDATED | 4 |
| `Infrastructure/Test_Roads.cpp` (Phase 09) | VALIDATED | 5 |
| `Infrastructure/Test_Decay.cpp` (Phase 09) | VALIDATED | 5 |
| `Infrastructure/Test_Logistics.cpp` (Phase 09) | VALIDATED | 5 |
| `Infrastructure/Test_WorksHistory.cpp` (Phase 09) | VALIDATED | 5 |
| `Infrastructure/Test_InfrastructureGate.cpp` (Phase 09) | VALIDATED | 2 |
| `Player/Test_Player.cpp` (Phase 10) | PROTOTYPE | 4 |
| `Player/Test_Start.cpp` (Phase 10) | PROTOTYPE | 4 |
| `Player/Test_Hours.cpp` (Phase 10) | PROTOTYPE | 5 |
| `Player/Test_Commands.cpp` (Phase 10) | PROTOTYPE | 6 |
| `Player/Test_Doings.cpp` (Phase 10) | PROTOTYPE | 6 |
| `Player/Test_Regard.cpp` (Phase 10) | PROTOTYPE | 6 |
| `Player/Test_LifeHistory.cpp` (Phase 10) | PROTOTYPE | 6 |
| `Player/Test_PlayerGate.cpp` (Phase 10) | PROTOTYPE | 1 |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). CTest entries: `Kernel.Purity`, `Kernel.PuritySelfTest`, `Core.Assert`, `Core.CoreTypes`, `Core.Harness`, `Core.Hash`, `Core.Ids`, `Core.Log`, `Core.LogFloor`, `Core.Random`, `Core.Version`, `Core.Registry`, `Core.Shuffled`, `Core.Reversed` (14 entries). Sim suites: EntityHandle 3, EntityRegistry 13, ComponentType 4, ComponentPool 8, ComponentStore 3, SimClock 4, Scheduler 8, Event 2, EventLog 2, EventBus 6, Archive 4, World 3, Snapshot 8, Replay 5, MiniWorld 4, TileGrid 4, WorldMap 6, FixedPoint 4, Noise 5, WorldGen 6, Climate 6, Hydrology 5, Regions 6, Deposits 5, WorldPipeline 4, History 3, Population 5, Naming 5, Religion 5, Disasters 5, PreHistory 5, HistoryText 5, HistoryGate 2 (161 tests; 158 tests without assertions); CTest entries `Sim.EntityHandle`, `Sim.EntityRegistry`, `Sim.ComponentType`, `Sim.ComponentPool`, `Sim.ComponentStore`, `Sim.SimClock`, `Sim.Scheduler`, `Sim.Event`, `Sim.EventLog`, `Sim.EventBus`, `Sim.Archive`, `Sim.World`, `Sim.Snapshot`, `Sim.Replay`, `Sim.MiniWorld`, `Sim.TileGrid`, `Sim.WorldMap`, `Sim.FixedPoint`, `Sim.Noise`, `Sim.WorldGen`, `Sim.Climate`, `Sim.Hydrology`, `Sim.Regions`, `Sim.Deposits`, `Sim.WorldPipeline`, `Sim.History`, `Sim.Population`, `Sim.Naming`, `Sim.Religion`, `Sim.Disasters`, `Sim.PreHistory`, `Sim.HistoryText`, `Sim.HistoryGate`, `Sim.Registry`, `Sim.Shuffled` (42 entries in total). Population suites: Persons 6, Lives 5, Families 5, Needs 6, Traits 5, Lod 6, PersonHistory 5, PopulationGate 1 (39 tests; 39 without assertions); CTest entries `Population.Persons`, `Population.Lives`, `Population.Families`, `Population.Needs`, `Population.Traits`, `Population.Lod`, `Population.PersonHistory`, `Population.PopulationGate`, `Population.Registry`, `Population.Shuffled` (10 entries). Society suites: Organizations 5, Standing 4, Norms 4, Bondage 4, Decisions 4, Strata 3, SocietyHistory 3, SocietyGate 1 (28 tests; 28 without assertions); CTest entries `Society.Organizations`, `Society.Standing`, `Society.Norms`, `Society.Bondage`, `Society.Decisions`, `Society.Strata`, `Society.SocietyHistory`, `Society.SocietyGate`, `Society.Registry`, `Society.Shuffled` (10 entries). Economy suites: Stocks 4, Production 4, Markets 4, Trade 4, Wealth 5, Grains 2, EconomyHistory 3, EconomyGate 1 (27 tests; 27 without assertions); CTest entries `Economy.Stocks`, `Economy.Production`, `Economy.Markets`, `Economy.Trade`, `Economy.Wealth`, `Economy.Grains`, `Economy.EconomyHistory`, `Economy.EconomyGate`, `Economy.Registry`, `Economy.Shuffled` (10 entries). Politics suites: Polities 4, Law 4, Reach 5, Succession 4, Factions 4, Diplomacy 4, PoliticsHistory 3, PoliticsGate 1 (29 tests; 29 without assertions); CTest entries `Politics.Diplomacy`, `Politics.Factions`, `Politics.Law`, `Politics.Polities`, `Politics.PoliticsGate`, `Politics.PoliticsHistory`, `Politics.Reach`, `Politics.Succession`, `Politics.Registry`, `Politics.Shuffled` (10 entries). Military suites: Armies 4, Battle 4, March 4, MilitaryGate 1, MilitaryHistory 4, Siege 4, Toll 4, War 4 (29 tests; 29 without assertions); CTest entries `Military.Armies`, `Military.Battle`, `Military.March`, `Military.MilitaryGate`, `Military.MilitaryHistory`, `Military.Siege`, `Military.Toll`, `Military.War`, `Military.Registry`, `Military.Shuffled` (10 entries). Infrastructure suites: Buildings 4, Works 5, Places 4, Roads 5, Decay 5, Logistics 5, WorksHistory 5, InfrastructureGate 2 (35 tests; 35 without assertions); CTest entries `Infrastructure.Buildings`, `Infrastructure.Works`, `Infrastructure.Places`, `Infrastructure.Roads`, `Infrastructure.Decay`, `Infrastructure.Logistics`, `Infrastructure.WorksHistory`, `Infrastructure.InfrastructureGate`, `Infrastructure.Registry`, `Infrastructure.Shuffled` (10 entries). Player suites: Player 4, Start 4, Hours 5, Commands 6, Doings 6, Regard 6, LifeHistory 6, PlayerGate 1 (38 tests; 38 without assertions); CTest entries `Player.Player`, `Player.Start`, `Player.Hours`, `Player.Commands`, `Player.Doings`, `Player.Regard`, `Player.LifeHistory`, `Player.PlayerGate`, `Player.Registry`, `Player.Shuffled` (10 entries).

### Tools/ and CI

| File | STATUS |
|---|---|
| `Tools/check_kernel_purity.py` | VALIDATED (Phase 00): self-test 36 checks, kernel scan 12 files, 0 violations, 2 exemptions (the `long long` aliases) |
| `Tools/kernel_modules.txt` | lists `VaelenCore` |
| `.github/workflows/kernel-ci.yml` | All 9 jobs green on GitHub (run 5): six Linux presets, `format`, Windows MSVC, macOS |

### `Source/VaelenInfrastructure` (Phase 09)

| File | Status |
|---|---|
| `VaelenInfrastructure.Build.cs`, `Private/VaelenInfrastructureModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | VALIDATED (Phase 09) |
| `Public/Vaelen/Infrastructure/InfrastructureApi.h` | VALIDATED (Phase 09) |
| `Public/Vaelen/Infrastructure/Buildings.h`, `Private/Buildings.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_Buildings.cpp` |
| `Public/Vaelen/Infrastructure/Works.h`, `Private/Works.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_Works.cpp` |
| `Public/Vaelen/Infrastructure/Places.h`, `Private/Places.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_Places.cpp` |
| `Public/Vaelen/Infrastructure/Roads.h`, `Private/Roads.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_Roads.cpp` |
| `Public/Vaelen/Infrastructure/Decay.h`, `Private/Decay.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_Decay.cpp` |
| `Public/Vaelen/Infrastructure/Logistics.h`, `Private/Logistics.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_Logistics.cpp` |
| `Public/Vaelen/Infrastructure/InfrastructureHistory.h`, `Private/InfrastructureHistory.cpp` | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_WorksHistory.cpp` |
| The whole module together | VALIDATED (Phase 09) — covered by `Tests/Infrastructure/Test_InfrastructureGate.cpp` |

### `Source/VaelenPlayer` (Phase 10)

| File | Status |
|---|---|
| `VaelenPlayer.Build.cs`, `Private/VaelenPlayerModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | PROTOTYPE (Phase 10) |
| `Public/Vaelen/Player/PlayerApi.h` | PROTOTYPE (Phase 10) |
| `Public/Vaelen/Player/Player.h`, `Private/Player.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_Player.cpp` |
| `Public/Vaelen/Player/Start.h`, `Private/Start.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_Start.cpp` |
| `Public/Vaelen/Player/Hours.h`, `Private/Hours.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_Hours.cpp` |
| `Public/Vaelen/Player/Commands.h`, `Private/Commands.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_Commands.cpp` |
| `Public/Vaelen/Player/Doings.h`, `Private/Doings.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_Doings.cpp` |
| `Public/Vaelen/Player/Regard.h`, `Private/Regard.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_Regard.cpp` |
| `Public/Vaelen/Player/PlayerHistory.h`, `Private/PlayerHistory.cpp` | PROTOTYPE (Phase 10) — covered by `Tests/Player/Test_LifeHistory.cpp` |

## Verified here

Toolchain: clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64. Every preset was configured, built and tested with
`cmake --preset`, `cmake --build --preset`, `ctest --preset` into `out/build/<preset>`:

| Preset | Build | `ctest` | `VaelenCoreTests` | `VaelenSimTests` | `VaelenPopulationTests` | `VaelenSocietyTests` | `VaelenEconomyTests` | `VaelenPoliticsTests` | `VaelenMilitaryTests` | `VaelenInfrastructureTests` |
|---|---|---|---|---|---|---|---|---|---|---|
| linux-clang-debug | 0 warnings | 109/109 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed | 35 run, 35 passed |
| linux-gcc-debug | 0 warnings | 109/109 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed | 35 run, 35 passed |
| linux-clang-release | 0 warnings | 109/109 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed | 35 run, 35 passed |
| linux-gcc-release | 0 warnings | 109/109 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed | 35 run, 35 passed |
| linux-clang-noasserts | 0 warnings | 109/109 passed | 108 run, 108 passed, 22214 checks | 158 run, 158 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed | 35 run, 35 passed |
| linux-gcc-noasserts | 0 warnings | 109/109 passed | 108 run, 108 passed, 22214 checks | 158 run, 158 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed | 35 run, 35 passed |

Run on 2026-09-09 for the closing of Phase 09, with every gate included. The debug presets take about an hour each now: `Infrastructure.InfrastructureGate` alone is 779 s inside a four-job CTest run, and `linux-clang-debug` totals 3414 s against `linux-gcc-debug`'s 3847 s; the optimised presets are about 717 s and the no-assert ones about 296 s.

Mini-world baseline (100 000 ticks, 41 entities, 305 027 events, 34 168 227-byte snapshot), logged by `Sim.MiniWorld`, not asserted: clang debug 0.39 s (255 k ticks/s), gcc debug 0.40 s, clang release 0.135 s (739 k ticks/s), gcc release without assertions 0.127 s (790 k ticks/s); snapshot 0.09-0.14 s.

Phase 10 CI record, and the most useful thing the matrix has done in ten phases. Runs 89 to 96
(10.01 through the gate) were each cancelled by the next push, because `cancel-in-progress` is on and
a task takes less time than a full matrix does. Run 104 was the first Phase 10 run allowed to finish,
and it came back with eight failures - none of them in Phase 10, all of them mine:

    93% tests passed, 8 tests failed out of 110
      54 - Population.PopulationGate       87 - Military.MilitaryGate
      61 - Society.SocietyGate             94 - Infrastructure.Decay (Timeout)
      66 - Economy.EconomyGate             95 - Infrastructure.InfrastructureGate
      78 - Politics.PoliticsGate          100 - Infrastructure.WorksHistory (Timeout)

Every Phase 10 test passed, `Player.PlayerGate` included at 454 s with its frozen digests, and the
Windows MSVC and macOS AppleClang BUILDS were clean - so the module is portable and the phase's own
work is sound. What failed was the frozen state digest of six CLOSED phases, because `PersonHeld` had
been registered inside `LodTypes::Declare`: that puts a component type into the type registry of
every world declaring 04.06, which is every gate from Phase 04 up, and a state digest counts type
ids. The two timeouts are the same tests at the 300 s default on a slower runner, now 1800 s.

The hole in the local loop is worth more than the bug. `quick.sh` runs `ctest -E "(Gate|Shuffled)"`,
and every affected test is a gate: ten commits of "both presets green" were true and blind to exactly
this class of regression. A change that touches a module below the one being built has to run the
gates, and from here it does.

Fixed by making the hold an observed type (`Population::DeclareHold`, `LodSystem::ObserveHeld`), so a
world with no player registers nothing new. Verified: `ctest -R Gate` on `linux-clang-debug` -
History 44 s, Military 770 s, Politics 1155 s, Infrastructure 779 s and the rest all pass with the
digests they were closed on. Phase 10's own two STATE digests were re-recorded because the Player
world now registers one type more; its LOG and LIFE digests did not move, which is the evidence that
the change was to the bookkeeping and not to the world.

GitHub Actions runs 13 to 28 (01.06 through 03.03): all 9 jobs green each; run 29 (03.04) red on Windows MSVC only (a dangling pool pointer in the faith listener changed the religion digest, and `Sim.Shuffled` exceeded its 300 s CTest timeout), both fixed in the 03.05 commit and green again in runs 30 to 48 (03.05 to 05.07); run 49 (05.08) was cancelled by the job timeouts - the Phase 05 gate took the serial debug test runs past 30 minutes on Linux and 45 on Windows (every Linux release and no-assert job green) - so from 06.01 CTest runs 4 jobs in every preset (`execution.jobs` in `CMakePresets.json`, the stdio capture entries serialised by a resource lock), which brings a debug run under ten minutes - green again in runs 50 to 68 (06.01 to 07.08 and the atlas actor; the first Unreal build at run 54 included; runs 56 and 60 were superseded by 57 and 61 on the same headless tree); so the frozen replay, mini-world and snapshot values hold on Windows MSVC and macOS AppleClang as well. Phase 00 record - run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14).

Also run locally: `python3 Tools/check_kernel_purity.py --self-test` (36 checks, 0 failed),
`python3 Tools/check_kernel_purity.py --root . --verbose` (12 files, 0 violations),
`clang-format --style=file --dry-run -Werror` on every kernel and test source (0 drift).

### Engine (development PC, 2026-09-07)

Toolchain: UE 5.6.1 (`5.6.1-44394996+++UE5+Release-5.6`), MSVC 14.51.36256 from Visual
Studio 2026 Community (the only x64 toolchain installed; UBT calls it "Visual Studio 2022
compiler version 14.51.36256 is not a preferred version" and uses it anyway, so no
`BuildConfiguration.xml` override was needed), Windows SDK 10.0.26100.0, Windows 11
Enterprise 10.0.26200, .NET 8.0.300 bundled with the engine.

| Step | Result |
|---|---|
| `Build.bat -projectfiles` | Succeeded, 14 s |
| `Build.bat VaelenEditor Win64 Development` (first) | Failed: `C2280` on `ComponentStore`, then `LNK2019/LNK2001` on 7 kernel symbols |
| `Rebuild.bat VaelenEditor Win64 Development` (after the fixes) | Succeeded, 77 actions, 65 s, 0 errors, 654 C4251 + 42 C4996 warnings |
| `Binaries/Win64` | `UnrealEditor-Vaelen{Core,Sim,Population,Society,Economy}.dll` + `UnrealEditor-Vaelen.dll` |
| Editor opened on `Vaelen.uproject` | `LogVaelen: VAELEN 0.0.1 - kernel save format v3 - kernel asserts on - module started` |
| `check_kernel_purity.py` after the fixes (UE's bundled Python 3) | 102 files, 0 violations |
| `clang-format 22.1.3 -i --style=file` on every edited source | 0 drift |

## Unverified

- The `Vaelen` game target: only `VaelenEditor` was built on 2026-09-07, so the
  monolithic path (every `VAELEN_<MODULE>_API` empty) has never been linked. Nor was any
  configuration other than Development: the `VAELEN_ASSERTS_ENABLED=0` and
  `VAELEN_LOG_COMPILED_MIN_LEVEL=2` branches of the `Build.cs` files stay unexercised
  under UBT. `Docs/ARCHITECTURE.md` section 8.3 keeps the list.
- Engine-backed CI: none, still. Every engine build this project has had was run by hand
  on the development PC; nothing re-runs them, and no automation can (ADR-0134). The
  `engine modules parse (clang 18)` CI job is NOT that: it parses the Unreal-facing
  modules against `Tools/EngineShim`, a stand-in, and proves the SHAPE of a program and
  nothing about the engine. Its own docstring says so.
- `Source/VaelenGame` (14.08) and `Source/VaelenUI` (14.09): written, parsed against the
  shim and fenced by `Tools/check_ui_fence.py`, and **never compiled by UnrealBuildTool**.
  The 2026-09-15 adversarial review found a defect exactly there - UnrealHeaderTool emits
  a destructor for a `UCLASS` that declares none, into a translation unit that never sees
  the pimpl's definition, so the build stops on a file nobody in this repository wrote.
  Fixed, and unfindable by anything here: there is no UnrealHeaderTool in the shim, so
  the translation unit that breaks does not exist to be parsed.
- The headless suites against the four kernel headers edited for that build
  (`ComponentStore.h`, `Population.h`, `Religion.h`, `Regions.h`): the development PC has
  no CMake, no Ninja and no system Python 3, so only the purity checker was re-run there
  (102 files at the time, 0 violations, through the Python bundled with UE; the tree is
  216 files today). The GitHub matrix is what confirms the tests and the frozen digests -
  175 CTest entries as of 2026-09-15.
- clang-cl on Windows: only Microsoft cl (MSVC 19.44) was exercised by CI; the
  `/clang:-ffp-contract=off` branch is untested.
- Long-duration and integration test categories: deferred to Phase 01 (ROADMAP 01.07,
  01.08); every kernel STATUS line says so.

## Discrepancies

None known between code, comments and documents as of 2026-09-15, after the adversarial
reviews of 14.05, 14.06, 14.07-14.09 and 14.10. The four the 14.07-14.09 review found in
the documents themselves - a regex count that contradicted its own list, and three stale
claims in `ARCHITECTURE.md` about modules that now exist - were corrected rather than
annotated.

Findings of the 00.05 review pass that were deliberately NOT applied: adding `FPSemantics` to `VaelenCore.Build.cs`
(property not confirmable without an engine; the in-source `fp contract(off)` pragmas
protect the kernel instead), pinning GitHub Actions to commit SHAs (major tags kept),
and the `IdKind` placeholders for Phases 02-12 (kept, now documented as provisional).

## How to refresh this document

```
for P in linux-clang-debug linux-gcc-debug linux-clang-release linux-gcc-release linux-clang-noasserts linux-gcc-noasserts; do
  cmake --preset $P && cmake --build --preset $P && ctest --preset $P
done
out/build/linux-clang-debug/Tests/Core/VaelenCoreTests --list | cut -d. -f1 | sort | uniq -c
python3 Tools/check_kernel_purity.py --self-test
python3 Tools/check_kernel_purity.py --root . --verbose
```

Then update the BUILD STATUS block, the task table and the numbers above.
