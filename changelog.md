# Changelog

## v0.2.0

- Fix native scene handoff: initialize GD's local-level return context, close GDCode modals and detach input before navigation; keep the editor open if navigation fails. Native Pause/Resume/Exit remain owned by GD.
- Add `docs/AI_CODING_PROMPT.md`, a standalone coding-agent master prompt grounded in the repository.
- Add CLI source-context diagnostics (source line + caret, tab/control-byte handling and bounded long-line excerpts); preserve quiet mode, legacy diagnostic rendering and generated output.
- Add navigation-contract and CLI regression tests. See `docs/VALIDATION_v0.2.0.md` for evidence and outstanding in-game acceptance checks.

## v0.1.0

Initial release.

- GDCode language: level settings block, object placement (`block`, `spike`, `saw`, `orb`, `pad`, `portal`, `startpos`, `coin`, raw `obj <id>`), object properties, `var`, `repeat ... as`, `define`/call patterns, arithmetic and seeded `random()`.
- In-game multi-line code editor with line numbers, caret navigation, auto-indent, undo/redo, clipboard copy/paste.
- Live problems panel with line/column, suggestions and click-to-jump.
- Project storage in the mod save folder (`projects/<id>/main.gdx` + `project.json`).
- Level generation into real local levels; regenerate in place with confirmation; "New Level" to fork.
- Settings: what to open after generating, overwrite confirmation, object limit.
