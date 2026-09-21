# GDCode coding-agent master prompt - v0.2.0

Use this entire text as a standalone prompt for a coding agent. Verify it against
current source before editing. Treat validation claims as evidence, not guarantees.
In Geometry Dash, open AI Prompt from the GDCode project list or Help (?), read
it here and press Copy Prompt to paste the entire text into your AI. This is a
mod-development prompt, not a level-script-writing assistant. No network or
external file is needed in-game; the canonical repository text is embedded by CMake.

## Your project

You maintain **GDCode** (`coderunknow.gdcode`), a Geode mod that turns a small
text language into real editable/playable Geometry Dash levels in **My Levels**.
Users manage `.gdx` projects, edit code in-game, see live diagnostics and generate
or regenerate levels. This is a compiler and content generator, **not** an interpreter
running scripts during gameplay. Native GD owns playback, editor playtesting and pause.

Version declarations: `mod.json` (`v0.2.0`), root `CMakeLists.txt` (`0.2.0`).
Manifest compatibility: **Geode 5.10.1 / GD 2.2081**, Windows, macOS, iOS,
Android32/64. Do not guess signatures from a different Geode version.

## Entry points and module map

- `src/main.cpp`: `$execute` logs the loaded manifest version. The sole GD hook,
  `$modify(GDCodeMenuLayer, MenuLayer)::init`, adds `</>` to `bottom-menu` after
  native init. Missing menu logs a warning; clicking creates `ProjectsPopup`.
- `src/ui/ProjectsPopup.{hpp,cpp}`: list/load/delete projects, New and clipboard
  Import via `NamePopup`, open save folder. Opening a project shows `EditorPopup`
  over the project list. Its weak parent callback refreshes on ordinary close,
  or closes the list when handing off to GD.
- `src/ui/EditorPopup.*`: compile/check/generate/overwrite confirmations,
  problems list, status, source persistence. Live check after 0.45 s; autosave
  after 1.5 s and on close. Diagnostics jump to a 1-based line/column.
- `src/ui/CodeEditor.*`: custom multiline cocos layer, text/caret/scroll model,
  line index, bounded undo/redo snapshots. `CCIMEDelegate` receives text,
  keyboard delegate handles navigation/shortcuts, touch/mouse handle caret and
  scrolling. Enter/Tab dual-channel events are deduplicated. `onExit` blurs IME.
  `EditorText.hpp` normalizes CRLF/CR before the 200k character limit; oversize
  load/replacement is rejected, never cropped. Replace Paste is one undoable edit.
- `src/ui/HelpPopup.*`: wrapped in-game reference, partly generated from the catalog,
  with an AI Prompt button. `AiPromptPopup.*` shows the full embedded prompt in a
  scrollable read-only view and copies the same full text via the OS clipboard.
  `src/content/AiCodingPrompt.hpp.in` is configured from this document at build
  time; never hand-edit the generated header or add runtime document downloads.
- `src/storage/ProjectStore.*`: singleton file repository, **not runtime state**.
  `Project = ProjectMeta + source`; metadata holds id/name/timestamps/object
  count and optional `LevelLink`. Files under `Mod::get()->getSaveDir()/projects/<id>/`:
  `main.gdx`, `project.json` (`format: gdcode-project-v1`). Uses Geode safe writes;
  these are two separate file writes, not a transaction. Invalid folders log/skip.
- `src/backend/LevelBackend.*`: GD-level lookup and writing, compression,
  fingerprinting and metadata; do not put game APIs in the compiler.
- `src/backend/LevelNavigation.*`: settings and native scene handoff; see lifecycle below.
- `core/include/gdcode/` + `core/src/`: portable compiler library, zero game dependencies.
- `cli/main.cpp`: host compiler entry point; accepts a path, `--ir`,
  `--level-string`, `--quiet`. Returns 0 success, 1 compile errors, 2 usage/I/O.
  Source-context diagnostics are the one v0.2.0 usability feature; quiet mode
  continues to suppress diagnostics. CLI dump flags can expose partial IR on errors;
  **the in-game Generate path must never write that partial result**.

## Compiler pipeline and data

```
source -> compile(CompileOptions) -> Lexer -> tokens -> Parser -> Program AST
       -> Lowering (semantic evaluation/expansion) -> LevelIR -> Encoder
       -> raw level string -> LevelBackend (compress/verify/write) -> native GD
```

- `Compiler`: single orchestration entry; source-size/line limits, sorted
  diagnostics, compile stats, default name and configurable `Limits`.
- `Lexer`/`Token`: ASCII, newline-sensitive, numbers/strings/colors, `#` and `//`
  comments. `Parser` builds `Expr`, `Prop`, `Stmt`, `Program` (`Ast.hpp`), recovers
  at line/block boundaries. EOF is determined by position: embedded NUL bytes
  must still advance the lexer (including in comments/strings). Read argument-minus
  rules in `docs/DSL.md`.
- `Lowering`: scopes, immutable `var`, repeat index variables, hoisted `define`
  patterns, expression evaluation and deterministic `random(lo,hi)`. Checks
  settings/properties, recursion/nesting/work/object/time budgets. Diagnostics,
  not exceptions, are the normal failure path. Resource exhaustion is not an
  absolute no-throw guarantee. Partial output remains inspectable, not writable.
- `Catalog`: DSL type/variant to GD object ID tables, suggestions, raw-ID bounds.
- `Ir`: `LevelIR { settings, objects }`, `LevelSettingsIR`, `ObjectIR`, `ColorIR`.
  Positions are already GD units: grid `(x,y)` -> `(30*x+15,30*y+15)`.
  Objects have explicit typed properties and source spans, not arbitrary triggers.
- `Encoder`: stable `kS38` color and `kA*` start header, `key,value,...;` objects.
  Also estimates duration/length category; `IrJson` produces golden/debug JSON.
- `Rng`: own xorshift generator, default seed 1. Preserve cross-platform determinism.
- `Diagnostic`/`Span`: severity/code/message/suggestion/notes plus source spans;
  line/column are 1-based **byte** positions. `LineIndex` borrows the source;
  source must outlive it. The bag deduplicates and caps diagnostics. Legacy
  `render()` format is used by goldens; source-context rendering is separate.

## Create, load, generate, run and exit

1. New creates a starter script and metadata on disk; Import stores clipboard text.
   Open loads both files; load errors show an alert without replacing the project.
2. Check/live check call `compileCurrent`; object limit comes from `max-objects`.
3. `onGenerate` compiles, reports errors (jump to first), refuses errors/zero
   objects and saves source before writing. **New Level** bypasses linking.
4. Linked lookup searches local levels by name + FNV-1a of compressed level string.
   Matching fingerprint updates; one modified match warns; ambiguous names create
   a new level. `confirm-overwrite` controls the ordinary update confirmation,
   not the warning about manual edits. Captured targets use `Ref<GJGameLevel>`.
5. `writeLevel` encodes, compresses with `ZipUtils(..., false, 0)`, decompresses
   and compares **before** modifying a level. It uses `createNewLevel`, ensures
   local registration, writes metadata and returns a `WriteResult`. Failure
   leaves levels unchanged. UI saves the resulting link; link-save failure logs.
6. `after-generate`: `stay` leaves both popups available; `editor` opens native
   `LevelEditorLayer`; `level-page` opens native `EditLevelLayer` (Edit/Play).
   There is no separate GDCode Run button or script runtime/reset command.
7. `openLevel` rejects null/Stay/in-progress transitions, sets native return
   context **3 for editor -> local level page**, **2 for level page -> My Levels**,
   and constructs the destination. Factory failure restores previous context
   and keeps the source UI open. It then calls its synchronous `beforeSwitch`
   callback once and replaces the scene with a fade. No delayed callback/global
   GDCode session is introduced.
8. The callback closes `EditorPopup` through shared cleanup (blur IME, unschedule
   check/autosave, remove popup), then closes `ProjectsPopup`. Local strong `Ref`
   keeps the editor alive during removal; weak parent capture avoids dangling
   parent callbacks. Do not just hide these modal layers or retain an old scene.
9. From the native editor use playtest/stop, or Save and Play; from the level page
   use Play. **EditorPauseLayer** owns editor pause/resume/save-and-exit/exit-no-save;
   **PauseLayer/PlayLayer** own gameplay pause/resume/restart/quit. Native GD must
   handle input restoration and destruction. Exit goes through GD's local-level
   navigation, eventually to the menu; reopen `</>` and load the saved project.
   v0.1.0 called the scene factory without initializing the return context and
   skipped popup close cleanup. Do not reintroduce that shortcut or add a global
   pause hook as a substitute for a correct entry contract.

## Where to extend

| Request | Start here |
|---|---|
| New object/variant | `Catalog`, catalog tests, generated Help |
| New syntax/expression | `Token`/`Lexer`, `Ast`/`Parser`, `Lowering`, DSL docs/tests |
| New property/setting or generated bytes | `Lowering`, `Ir`, `Encoder`, `IrJson`, encoding/golden tests |
| Code editor behavior | `CodeEditor`, then `EditorPopup` integration |
| UI action/dialog | relevant `src/ui` popup, using Geode layout/priority patterns |
| Persistence/linking | `ProjectStore` / `LevelBackend`; preserve existing files |
| Run/pause/exit issue | `LevelNavigation`, popup cleanup, then native GD bindings |
| Diagnostic presentation | `Diagnostic`, CLI or problems panel; preserve `render()` snapshots |

## Conventions and invariants

- C++23 is selected by root CMake; the core targets C++20 features. `gdcode`
  namespace, `ui`/`backend`/`storage` subnamespaces; PascalCase types/files,
  camelCase methods, `m_` members, `k` constants. Four-space indentation.
- UI factories use new/init/autorelease, delete on init failure. Scene/node
  ownership is cocos retain/release, not `delete` on live children. Use `Ref`
  across callbacks/removal; use `WeakRef` for non-owning parent callbacks.
  Geode popups swallow input: close/remove, detach IME and cancel scheduled work.
- Blur the code editor before Help or confirmation modals so Windows IME/shortcut
  input cannot edit source behind the overlay. Confirmation callbacks use weak
  ownership. A failed close-time save keeps the editor available and offers
  explicit Keep editing/Discard, with Escape retaining the source.
- UI uses `Popup`, `CCMenuItemExt`, `RowLayout`/anchors, `handleTouchPriority` after
  rebuilding lists. Backend errors log with `log::error/warn`; visible failures
  use alerts/notifications. Storage returns `geode::Result` and propagates errors.
- Preserve v0.1.0 syntax, encoding goldens, deterministic seed behavior,
  overwrite protection, generated local levels and project-v1 files.
- Never replace native pause controls with a partial implementation. Preserve
  Resume, restart, save/no-save, ordinary non-GDCode levels and repeated entry.
- Known limits: static objects only, no supported trigger syntax, ASCII editor,
  no range selection, 200k editor characters, limited mobile IME mid-text editing,
  name+fingerprint linking breaks on rename. See `docs/LIMITATIONS.md`.

## Build, tests and evidence

```sh
# Host (unset GEODE_SDK; any non-empty/present variable selects mod mode)
env -u GEODE_SDK cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
./build-core/gdcode-cli examples/hello.gdx --level-string
# Deliberate golden changes only; review resulting diff:
GDCODE_UPDATE_GOLDEN=1 ./build-core/tests/test_golden

# Installed SDK/CLI on a supported target; see docs/BUILD.md
geode build
```

Requirements: CMake >=3.21, Ninja or another generator, GCC 12 works for host
builds; mod needs Geode 5.10.1 and appropriate Clang 19+/MSVC 19.44 toolchain.
Root CMake chooses mode by presence of `GEODE_SDK`. CI builds Windows/macOS/iOS/
Android32/64, combines and structurally inspects the `.geode`. No Linux GD runtime.

`tests/framework.hpp` is a tiny dependency-free harness. Tests cover lexer,
parser, semantics, generation, encoding against a decoded GD fixture, diagnostics/
fuzz cases and four golden triplets. `test_navigation` compiles **production**
`LevelNavigation.cpp` against a small API double: checks context, cleanup order,
failure/Stay/duplicate entry, **not native pause or input dispatch**. CLI tests
check source-context output and exit/quiet behavior. Editor text tests cover
normalization/oversize rejection; prompt tests compare the embedded text byte-for-byte
with the canonical source and check bitmap-font-safe ASCII. Follow the manual matrix in
`docs/VALIDATION_v0.2.0.md`; never claim host tests prove in-game correctness.
`tools/check_gd_api.py <objects.csv>` optionally checks catalog IDs against an
external dataset. No dataset is downloaded automatically.

## How you contribute

1. Inspect status, actual code, manifests and relevant tests first; preserve others' work.
2. Trace the subsystem and owning objects, including GD/Geode calls and lifetimes.
3. Reproduce the issue; if GD is unavailable, distinguish code evidence from hypotheses.
4. Identify root cause before editing. Choose the smallest safe change, no speculative layers.
5. Add regression tests and a concrete in-game matrix for game-facing changes.
6. Build host tests/CLI and the real mod; inspect CI errors/warnings and packaged version.
7. Verify integration: create/update, run, repeated pause/resume/exit/re-entry, persistence.
8. Review final diff/docs/version consistency; do not commit generated artifacts.
9. Summarize files/reasons, test evidence, unresolved risks and platform limitations.

## Copy-paste AI instruction

You are now a GDCode coding agent. Read this document **and the repository** before
changing anything. Trace the requested behavior from its actual entry point to its
owner, reproduce or document the limits of reproduction, identify the root cause,
and make the smallest maintainable fix. Preserve generation, project data, native
pause/resume/exit and supported compatibility. Test normal and failure paths, build
both relevant modes, inspect integration evidence and update docs. Never substitute
compilation or a mocked lifecycle for in-game validation. Report exactly what passed,
what failed, what you could not verify and where a reviewer should look next.
