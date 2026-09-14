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

| file | what would go unnoticed without it |
|---|---|
| `VaelenGame/Public/VaelenWorldSubsystem.h` | a `.generated.h` of ONE module included from ANOTHER - the shared stub directory of 14.07. With a per-module stub scan this file is `not found` while VaelenUI is parsed. |
| `VaelenGame/Private/VaelenWorldSubsystem.cpp` | that the module holding the world may name `Vaelen::Run` and `Take.h`, which the UI may not: the fence reads `VaelenGame/Public`, not its `Private`. |
| `VaelenUI/Public/VaelenHUD.h` | that a UI header sees `Vaelen/View/Panel.h` and no kernel header. |
| `VaelenUI/Private/VaelenHUD.cpp` | that a page is drawn from `Lines()` alone, with no World anywhere in the translation unit. |
| `VaelenUI/Public/VaelenPlayerController.h`, `.cpp` | that every key goes through `Press()` and the door's `Mean`, and that the keys are the ones the page carries. |

When `Source/VaelenGame` and `Source/VaelenUI` exist, the tools read those
instead - `Tools/parse_engine_modules.py` prefers a module with a `Private`
directory under `Source/` and says which root it took each from - and this
tree stays as what the self-tests mutate, which is what keeps the mutations
off the real modules.
