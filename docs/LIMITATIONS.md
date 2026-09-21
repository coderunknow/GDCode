# Known limitations and verification notes (v0.2.0)

## Scope (by design, MVP)

- **No triggers.** Only static objects and level start settings. The IR is
  key/value based so triggers can be added later as catalog data + property
  mappings.
- **Small object catalog.** Blocks, spikes, saws, orbs, pads, portals, start
  position, coin. Anything else is reachable through `obj <id> x y`.
- **No assignment / conditionals / functions returning values.** The
  programmable subset is `var`, `repeat ... as`, `define` patterns, arithmetic
  and `random`.
- **No selection / copy-paste of ranges** inside the in-game editor. Use
  *Copy* (whole script) / *Paste* (insert or replace) or edit the `main.gdx`
  file externally - the projects folder button opens it.
- **ASCII only.** GD's bitmap fonts have no glyphs for other scripts; the
  compiler rejects non-ASCII with a positioned error.

## Behaviour to be aware of

- **Level linking is by name + fingerprint.** If you rename the generated
  level in GD, GDCode cannot find it again and creates a new one. If you edit
  it in the editor (and save), the next *Generate* warns before overwriting.
- **Level length shown in GD** is estimated from the right-most object and
  speed portals; GD recomputes it exactly when you save from the editor.
- **Editor on Android/iOS:** typing works through the soft keyboard, but the
  OS keyboard can only *append/delete at the end of its own buffer*, which is
  how cocos2d 2.x IME works for GD's own inputs too. Mid-text edits are best
  done with a hardware keyboard or by pasting the full script.
- **Header defaults:** generated levels use the 2.2 editor's default color
  channels and compatibility flags (`kA27/31/32/33/34/37/38/39/40/41/42/45 = 1`).
  All of them are editable afterwards in the level settings.
- **Descriptions** are stored base64-encoded, like the game does for levels;
  if a description ever shows up garbled, that assumption is the first thing
  to check.

## Verification status

- The compiler core is covered by automated tests (lexer, parser, semantics,
  generation, encoder, diagnostics, golden snapshots) that run in CI.
- The mod compiles and links for Windows, macOS, iOS, Android32 and Android64
  in CI against Geode v5.10.1 / GD 2.2081, and the combined `.geode` package
  is structurally checked (see `docs/BUILD.md`).
- The **in-game behaviour has not been verified by the author yet**; the
  checklist below is what a release should be validated against.

See [the v0.2.0 validation record](VALIDATION_v0.2.0.md) for release-specific
build results and the expanded pause/resume/exit/re-entry matrix. Do not infer
in-game success from CI or the host navigation doubles.

## In-game verification checklist

Automated tests cover many compiler cases, not all behavior; the game-facing layer has to
be exercised in GD itself:

1. Start GD with the mod - `</>` button visible in the main menu.
2. New project -> editor opens, typing, Enter (auto-indent), Backspace, arrows,
   Home/End, Tab, Ctrl+V, Ctrl+Z work; problems panel updates while typing.
3. Introduce a typo (`spirke`) -> red problem with suggestion; click it -> the
   caret jumps to the line.
4. *Generate* -> level opens in the editor with the objects at the expected
   grid cells; play-test from the editor.
5. Back in GDCode: change the script, *Generate* again -> confirmation ->
   same level updated (My Levels shows a single entry).
6. Edit the level manually in GD and save; *Generate* again -> "Level was
   edited" warning; *New Level* creates a second level.
7. Restart GD -> project still listed; generated levels still in My Levels.
