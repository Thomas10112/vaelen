# The shim's contract probe - Phase 19 task 19.02

Not a module and never built by anything: a few translation units written the
way 19.06 will write the walk (a character with a spring-arm camera, a
controller that maps Z/Q/S/D and the mouse at run time with Enhanced Input, a
procedural-mesh ground with collision, a sun and a sky made in C++, a line
trace, a multicast delegate), so that every API the 19.02 shim headers believe
in is USED by something the parse reads.

`Tools/parse_engine_modules.py` parses it with the engine modules;
`Tools/test_engine_shim.py` mutates it (a mesh section with seven arguments, a
handler of the wrong shape, a misspelt movement field, a key mapped into a
const context) and requires every mutation to be refused. `AShimProbeWalker::Jump`
calls `Super::Jump()`, which exists on ACharacter and not on AActor: it parses
only because 19.02 made Super exact, and `--super inherited` is shown to refuse it.

Nothing here is evidence that the engine agrees. Every name it uses that the
engine modules do not is a belief of Tools/EngineShim until sitting S2 (19.06)
compiles a use of it - and Tools/shim_beliefs.txt counts it the day 19.06's
code does.
