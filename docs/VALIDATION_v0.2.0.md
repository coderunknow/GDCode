# v0.2.0 engineering record

## Baseline and investigation

- Baseline: commit `438af983`, version v0.1.0; seven host suites pass on GCC 12.
- Reproduction to perform in GD 2.2081 / Geode 5.10.1: main menu `</>` -> New
  -> keep valid starter -> Generate (`after-generate=editor`) -> native playtest /
  Save and Play -> Pause -> Resume, then Pause -> exit. Repeat from `level-page`.
- **No game runtime is available in the Linux development environment.** The
  reported trap has not been reproduced interactively here. Do not treat the
  handoff tests as confirmation that native controls work.
- Code evidence: `EditorPopup::writeAndOpen` previously blurred IME and refreshed
  its parent without closing either popup; `LevelBackend::openLevel` directly
  constructed/replaced GD scenes without initializing `GameManager::m_sceneEnum`.
  There are no GDCode PauseLayer/PlayLayer hooks or custom runtime state machine.
- Native API evidence: Geode 5.10.1 tag `7e41336f` Popup implementation closes by
  disabling touch/keypad and removing itself. GD 2.2081 bindings expose
  `GameManager::returnToLastScene`, native pause handlers, and the local-level
  routing conventions (e.g. `EditLevelLayer::onTest` sets 3, `onClone` sets 2).
  Calling a scene factory is not the entire native entry action. Stale return
  context is the identified navigation defect; its sufficiency for the reported
  symptom remains an in-game verification requirement.

## Fix and contracts

`LevelNavigation` sets the native local-level route before constructing a scene:
editor -> local level page (3), level page -> My Levels (2). It restores context
on factory failure and does nothing for Stay/null/active transitions. On success
it synchronously closes the source UI before scene replacement. Editor close
blurs IME and unschedules checks/autosave; its weak parent callback closes the
project list. Strong temporary ownership protects removal callbacks. Native GD
continues to own Pause/Resume/Restart/Exit; no global session or pause hooks added.

Host navigation tests compile the actual handoff with a narrow API double. They
assert routing and callback order, failure rollback, settings, Stay and duplicate/
repeated entry. They do **not** simulate pause, native routing internals, cocos
refcounts, touch priority, OS keyboard or overlay destruction.

## Exactly one selected feature (recorded before implementation)

**CLI source-context diagnostics**: show the offending source line and caret under
positioned diagnostics. Fits the existing compiler/CLI; helps authors debugging
scripts outside the game without changing syntax, IR or generation. Uses
`Diagnostic` and `LineIndex`; affects `core/include/gdcode/Diagnostic.hpp`,
`core/src/Diagnostic.cpp`, `cli/main.cpp`, diagnostic/CLI tests and docs.

Planned proof: typo with suggestion and caret; tabs/CRLF; empty/EOF/unpositioned
and out-of-range positions; bounded long-line output; safe control-byte display;
CLI normal/error/quiet outputs and exit codes; unchanged legacy render/goldens.

## Automated/build results

- GCC 12 Debug configure/build: passed without compiler warnings; all **9/9**
  CTest suites pass. Baseline seven compiler suites and unchanged goldens still pass.
- ASan + UBSan host build: **9/9** pass, no sanitizer reports (Linux non-PIE).
- CLI example: `examples/hello.gdx` -> **79 objects, 0 errors**.
- Feature unit tests cover suggestions, caret placement, tabs, CRLF, controls,
  empty/EOF/invalid locations and long-line cropping. CLI integration verifies
  error status 1, successful generation/summary and unchanged quiet output.
- Navigation contract: six cases pass (production code against API doubles).
- `git diff --check`: clean; no generated artifacts or golden changes.
- Manifest/CMake version: **v0.2.0 / 0.2.0**, Geode/GD requirements unchanged.
- Real five-platform Geode build/package: pending CI. Native in-game tests: blocked
  by lack of GD runtime. Release readiness is **not** asserted.

## In-game acceptance matrix — NOT RUN (release gate)

Use default starter and `examples/hello.gdx`. Record platform, GD/Geode version,
other enabled mods, result and logs for failures. First test without other mods.

| Scenario | Required outcome | Status |
|---|---|---|
| Main menu, New / existing project / import | button, source and diagnostics usable | Not run |
| Generate editor; pause before playtest | Resume, Save and Exit, Exit without saving (confirm/cancel) work | Not run |
| Editor playtest -> pause/stop, Save and Play -> Pause | correct native editor/gameplay state | Not run |
| Pause via mouse/touch and Escape/Android Back | all intended controls respond; Back resumes where native GD intends | Not run |
| Gameplay pause -> Resume ten times | one pause layer, active input/music restored | Not run |
| Gameplay pause -> Quit (confirm/cancel) | local level page/editor as appropriate, no frozen overlay | Not run |
| Level-page setting -> Play -> Pause -> Quit -> Back | local page -> My Levels -> menu | Not run |
| Stay -> generate twice -> close -> open generated level from My Levels | source UI stays usable; native exit works | Not run |
| Return menu -> reopen same project -> generate/run/exit ten times | no accumulating layers, callbacks or input locks | Not run |
| Regenerate unchanged / manually edited / New Level | confirmations and source/level preservation unchanged | Not run |
| Empty/invalid source; cancelled overwrite | no scene switch or partial level write | Not run |
| Restart GD | saved project/level/link reload | Not run |
| Non-GDCode level play/editor pause/exit | native behavior unchanged | Not run |

Second-pass onboarding review: prompt identifies entry/init, compiler/generation,
Run/native Pause/Exit, syntax/UI extension files, build/tests and test limitations.
Another agent needs no prior conversation; source inspection remains mandatory.
