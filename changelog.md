# Changelog

## v0.1.0

Initial release.

- GDCode language: level settings block, object placement (`block`, `spike`, `saw`, `orb`, `pad`, `portal`, `startpos`, `coin`, raw `obj <id>`), object properties, `var`, `repeat ... as`, `define`/call patterns, arithmetic and seeded `random()`.
- In-game multi-line code editor with line numbers, caret navigation, auto-indent, undo/redo, clipboard copy/paste.
- Live problems panel with line/column, suggestions and click-to-jump.
- Project storage in the mod save folder (`projects/<id>/main.gdx` + `project.json`).
- Level generation into real local levels; regenerate in place with confirmation; "New Level" to fork.
- Settings: what to open after generating, overwrite confirmation, object limit.
