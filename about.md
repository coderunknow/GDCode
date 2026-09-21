# GDCode

Write Geometry Dash levels as **code**.

GDCode adds a small level-scripting language to the game. You type (or paste) a script, the mod compiles it and produces a **real, editable, playable level** in *My Levels* - no manual placing of hundreds of blocks.

```
level "Hello GDCode" {
    speed: normal
    bg: #287DC8
}

repeat 40 as i {
    block i 0
}

define tower(x, h) {
    repeat h as j { block x j + 1 }
}

tower(12, 3)
spike 16 1
orb yellow 20 4
portal ship 30 3
```

## Features

- **In-game code editor** with line numbers, keyboard navigation, undo and clipboard import/export.
- **Live diagnostics** with line/column, "did you mean" suggestions - click a problem to jump to it.
- **Deterministic generation**: the same script (and seed) always produces the same level.
- **Projects** are plain text files in the mod's save folder - back them up or share them.
- **Regenerate in place**: edit the script, generate again, the same level is updated (with a confirmation, and a warning if you changed it by hand in the editor).
- **Safety limits** on object count, loops and compile time, so a typo can never freeze the game.

## Usage

1. Press the green **`</>`** button in the main menu.
2. Create a project, write your script, press **Generate**.
3. The level opens in the editor (configurable in the mod settings).

Press **?** inside the editor for the language reference.

Objects available in this version: blocks, spikes (all sizes), sawblades, jump orbs, pads, portals (gamemode, gravity, size, speed, mirror, dual), start positions and coins - plus `obj <id>` for any other object id.
