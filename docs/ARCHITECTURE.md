# Architecture

```
            source text (.gdx)
                   |
   +---------------v-----------------+       core/  (pure C++20, no Geode)
   |  Lexer  ->  Parser  ->  AST     |
   |  Lowering (semantics + IR gen)  |
   |  LevelIR  ->  Encoder           |
   +---------------+-----------------+
                   |  LevelIR / raw level string
   +---------------v-----------------+       src/   (Geode mod)
   |  backend/LevelBackend           |  GJGameLevel, ZipUtils, LocalLevelManager
   |  storage/ProjectStore           |  files in the mod save dir
   |  ui/CodeEditor, EditorPopup ... |  cocos2d / Geode UI
   +---------------------------------+
```

## Layers

### `core/` - the compiler (no game dependency)

| file | role |
|------|------|
| `Lexer` | text -> tokens; newline-sensitive, `#`/`//` comments, `#RRGGBB` colors, ASCII only |
| `Parser` | tokens -> AST with error recovery (skip to end of line / block) |
| `Ast` | plain structs (`Expr`, `Prop`, `Stmt`, `Program`) |
| `Lowering` | semantic checks + execution: scopes, `repeat`, patterns, `random`, limits -> `LevelIR` |
| `Catalog` | table of DSL object types/variants -> GD object ids (data, not code) |
| `Ir` | `LevelIR` = settings + objects in GD units; `irToJson` for snapshots |
| `Encoder` | `LevelIR` -> raw GD level string (`kS38`/`kA*` header + `k,v` objects) |
| `Compiler` | `compile(source, options)` = the one entry point |
| `Diagnostic` | severity, code, span, message, suggestion, notes; de-dup + cap |
| `Rng` | xorshift64* - portable determinism (std distributions are not) |
| `Limits` | every unbounded operation is capped here |

Design rules:

- **Pure**: same input -> same output. No globals, no clocks in the output
  (the compile-time budget only decides *whether* to stop, not *what* is
  produced before that).
- **Never throws, never hangs**: all failures are diagnostics; all loops are
  bounded by `Limits`.
- **Table-driven**: adding an object = adding a catalog row. Settings and
  properties are small dispatch tables inside `Lowering`. The GD ids in the
  catalog can be cross-checked against an object-id dataset with
  `python3 tools/check_gd_api.py <objects.csv>`.
- **IR is GD-agnostic in shape**: key/value style objects, so triggers or other
  object kinds can be added later as data without touching the front end.

### `src/backend/` - the only code that touches GD level objects

`writeLevel(ir, target)`:

1. `encodeLevelString(ir)` -> raw string.
2. `ZipUtils::compressString(raw, false, 0)` -> the exact format GD stores in
   `GJGameLevel::m_levelString` (verified against BetterInfo's use of
   `decompressString(m_levelString, false, 0)`).
3. Decompress again and compare - a failed round-trip aborts before anything
   is touched.
4. `GameLevelManager::createNewLevel()` (GD's own "New" button path) or the
   existing linked level; fill name, description (base64, like the game),
   audio track / custom song, object count, length category.

Linked levels: a project remembers the *name* and a *fingerprint* (FNV-1a of
the level string it wrote). On the next generation the level is looked up
among the local levels; if its fingerprint still matches, it is updated in
place (optionally after a confirmation); if it changed - the user edited it in
the editor - the UI offers overwrite vs. new level.

### `src/storage/`

```
<GD save dir>/geode/mods/coderunknow.gdcode/projects/<id>/
    main.gdx        the script
    project.json    { name, created_at, updated_at, last_object_count, linked_level }
```

Plain files so users can back up, share and edit them externally.

### `src/ui/`

- `CodeEditor` - multi-line editor node. Text input arrives through cocos2d's
  IME dispatcher (`CCIMEDelegate`) - the same channel GD's own text inputs
  use, so it works with the OS keyboard on desktop and the soft keyboard on
  mobile. Navigation/shortcuts come through `CCKeyboardDelegate`, taps and
  drags through `CCTouchDelegate`, mouse wheel through `CCMouseDelegate`.
  Keys that may be reported on both channels (Enter/Tab) are de-duplicated.
- `EditorPopup` - editor + live problems list + actions (Check, Generate,
  New Level, Copy, Paste, Undo, Help). Autosaves 1.5 s after changes and on
  close; recompiles 0.45 s after changes.
- `ProjectsPopup` - project list, New / Import clipboard / Folder / delete.
- `HelpPopup` - language reference generated from the object catalog.

## Testing strategy

- `tests/` (host-only, no game): lexer, parser, semantics, generation,
  encoder (including byte-identical records against decoded official level
  data), diagnostics (including a deterministic garbage-input fuzz loop),
  golden snapshots of IR + level string + diagnostics for four scripts.
- CI builds the real `.geode` for Windows, macOS, iOS, Android32/64 and
  publishes the combined package as an artifact.
- In-game verification (manual, see `docs/LIMITATIONS.md` for the checklist).
