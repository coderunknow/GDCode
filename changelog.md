# Changelog

## v0.2.0

- Fix native scene handoff: initialize GD's local-level return context, close GDCode modals and detach input before navigation; keep the editor open if navigation fails. Native Pause/Resume/Exit remain owned by GD.
- Add an offline **AI Prompt** window from both Projects and Help: scroll to read, Copy Prompt to copy the full coding-agent prompt. The canonical documentation is embedded in every platform binary; players need no external file.
- Fix lexer hangs on embedded NUL bytes; reject oversized editor loads/pastes instead of silently truncating saved source.
- Preserve Undo on Replace Paste; Escape cancels Paste. Release editor input for modals, use weak confirmation callbacks, and keep unsaved edits available after a failed close-time save unless explicitly discarded.
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
