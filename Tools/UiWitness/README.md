# The witness UI (14.07)

`Tools/parse_engine_modules.py` and `Tools/check_ui_fence.py` police two
modules, `VaelenGame` and `VaelenUI`, that 14.08 and 14.09 write. This is what
they police until those exist: a miniature of each, small enough to read in
one sitting and real enough that every rule the fence claims has something to
fire on.

It is NOT built by anything. No `.Build.cs`, no entry in `Vaelen.uproject`, no
CMake target: UnrealBuildTool never sees it and neither does the headless
build. It is parsed by the CI's front end against `Tools/EngineShim`, and read
by the fence, and that is all.

What each file is here to prove:

14.08 wrote the real `Source/VaelenGame`, so the witness's own copy of that
module is gone: the tools prefer the real one, and the UI below is now parsed
against the REAL subsystem header. That is worth more than the copy was - the
first parse after 14.08 landed failed here, because this UI had been written
against a made-up API (`Page()`, `TurnTheDay()`) while the real module says
`Panel()` and `AdvanceDay()`. A witness that cannot rot silently is the point.

| file | what would go unnoticed without it |
|---|---|
| `VaelenUI/Public/VaelenHUD.h` | that a UI header sees `Vaelen/View/Panel.h` and no kernel header. |
| `VaelenUI/Private/VaelenHUD.cpp` | that a page is drawn from `Lines()` alone, with no World anywhere in the translation unit. |
| `VaelenUI/Public/VaelenPlayerController.h`, `.cpp` | that every key goes through `Press()` and the door's `Mean`, and that the keys are the ones the page carries. |

When `Source/VaelenUI` exists too, the tools read that instead -
`Tools/parse_engine_modules.py` prefers a module with a `Private` directory
under `Source/` and says which root it took each from - and this tree stays as
what the self-tests mutate, which is what keeps the mutations off the real
modules. `VaelenGame` has already gone that way (14.08), and the
`VaelenWorldSubsystem.generated.h` the shared stub directory exists for is now
the real module's, included by the UI below: the same proof on firmer ground.
