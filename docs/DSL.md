# GDCode language reference

GDCode is a small, line-oriented language for describing Geometry Dash levels.
Files use the `.gdx` extension. Everything is ASCII (the game's fonts cannot
render other characters, and the compiler rejects them with a clear message).

## 1. Basics

- One statement per line. Blank lines are ignored.
- Comments start with `#` or `//` and run to the end of the line.
- Names are case-sensitive (`spike`, not `Spike`).
- Numbers may be integers or decimals: `4`, `4.5`, `.5`.
- Strings use double quotes and support `\"`, `\\`, `\n`, `\t`.
- Colors are written `#RRGGBB` (`#FF8800`).

## 2. Coordinates

Positions are given in **blocks** (grid cells), not in raw game units.

- `x` grows to the right, `y` grows upward.
- `y = 0` is the row that sits on the ground; `block 5 0` is a floor block.
- Decimals are allowed: `block 2.5 1` puts a block half a cell to the right.
- Internally one block is 30 units and an object at cell `(gx, gy)` is placed at
  `(gx*30 + 15, gy*30 + 15)` - exactly like objects placed in the editor.

## 3. Objects

```
<type> [variant] <x> <y> [prop:value ...]
```

| type       | variants                                                                                                  | notes |
|------------|-----------------------------------------------------------------------------------------------------------|-------|
| `block`    | -                                                                                                         | 1x1 basic block (id 1) |
| `spike`    | `half`, `small`, `tiny`, `ice`, `fake`, `invisible`                                                       | default: normal spike (id 8) |
| `saw`      | `large` (default), `medium`, `small`                                                                      | rotating sawblades |
| `orb`      | `yellow`, `pink`, `red`, `blue`, `green`, `black`, `dash`, `spider`, `teleport`, `toggle`                 | variant required |
| `pad`      | `yellow`, `pink`, `red`, `blue`, `spider`                                                                 | variant required |
| `portal`   | `cube` `ship` `ball` `ufo` `wave` `robot` `spider` `swing` `gravDown` `gravUp` `mirrorOn` `mirrorOff` `mini` `sizeNormal` `speedHalf` `speed1x` `speed2x` `speed3x` `speed4x` `dualOn` `dualOff` | variant required |
| `startpos` | -                                                                                                         | start position |
| `coin`     | -                                                                                                         | user coin |
| `obj`      | `obj <id> <x> <y>`                                                                                        | escape hatch: any object id 1-4539 |

Examples:

```
block 0 0
spike 3 1
spike half 4 1
saw medium 10 3 rot:45
orb yellow 12 4
pad pink 15 0
portal ship 20 3
obj 1329 25 2          # user coin by raw id
```

### Object properties

| property | value                 | meaning |
|----------|-----------------------|---------|
| `rot`    | number (degrees)      | rotation |
| `scale`  | number > 0            | uniform scale (1 = normal) |
| `flipX`  | `true` / `false`      | flip horizontally |
| `flipY`  | `true` / `false`      | flip vertically |
| `group`  | integer 0-999         | group id |
| `color`  | integer 0-1014        | main color channel (1000 BG, 1001 G, 1002 Line, 1003 3DL, 1004 Obj, ...) |
| `layer`  | integer 0-9999        | editor layer |
| `zOrder` | integer               | z order |

```
block 5 2 rot:90 scale:0.5 flipX:true group:12 color:1004
```

### Negative numbers as arguments

Arguments are separated by spaces, so `-` needs a rule:

```
block 5 -1      # two arguments: x = 5, y = -1   (space before '-', none after)
block 5 - 1 2   # x = 5 - 1 = 4, y = 2
block 5-1 2     # x = 4, y = 2
block (5 -1) 2  # parentheses always mean "one expression": x = 4
```

## 4. Level settings

At most one `level` block per script, at the top level:

```
level "My Level" {
    desc: "Made with GDCode"
    song: 0            # built-in soundtrack index (0 = Stereo Madness)
    customSong: 0      # Newgrounds song id (0 = none)
    speed: normal      # slow | normal | fast | faster | fastest
    mode: cube         # cube | ship | ball | ufo | wave | robot | spider | swing
    mini: false
    dual: false
    flip: false        # start with flipped gravity
    twoplayer: false
    platformer: false
    bg: #287DC8        # background color
    ground: #0F5FA8    # ground color
    line: #FFFFFF
    object: #FFFFFF
    seed: 42           # seed for random()
}
```

Settings can also share a line: `level "L" { seed: 1 speed: fast }`.
When there is no `level` block the level is named after the project.

## 5. Variables and expressions

```
var width = 12
var gap = width / 3 + 1
block width 0
```

- `var` defines a name once per scope; it cannot be re-assigned.
- Expressions support `+ - * /`, parentheses and `random(lo, hi)`.
- Division by zero is a compile error.
- Loop variables and pattern parameters are variables too.
- Reserved words (`level`, `var`, `repeat`, `define`, `as`, `true`, `false`,
  `random`, `obj` and every object type) cannot be used as names.

## 6. Loops

```
repeat 10 {
    spike 5 1
}

repeat 10 as i {          # i = 0, 1, ..., 9
    block i 0
    var top = i / 2
    spike i top + 1
}
```

Loops nest (up to 8 deep). Each iteration gets a fresh scope for `var`.

## 7. Patterns (`define`)

Reusable blocks of statements with parameters. Patterns are hoisted: you may
call one before its definition. They must be defined at the top level.

```
define tower(x, height) {
    repeat height as j {
        block x j
    }
    spike x height
}

tower(10, 3)
tower(14, 5)
```

Patterns may call other patterns (up to 16 levels deep); accidental infinite
recursion is reported instead of hanging the game.

## 8. Randomness

`random(lo, hi)` returns a whole number between `lo` and `hi` (inclusive).
It is driven by a deterministic generator seeded from `seed:` in the `level`
block (default seed 1, with an info diagnostic). The same script and seed
always produce the same level - on every platform.

```
level "Random" { seed: 2024 }
repeat 20 as i {
    spike i * 3 random(1, 3)
}
```

## 9. Limits

These protect the game from runaway scripts. Hitting one is an error; the
objects generated so far are kept for inspection but nothing is written.

| limit | default |
|-------|---------|
| source size | 1 MB / 20 000 lines |
| objects | 100 000 (configurable in mod settings) |
| executed statements | 200 000 |
| `repeat` count / nesting | 100 000 / 8 |
| pattern call depth | 16 |
| compile time | 2 s |
| coordinates | ±100 000 blocks |

## 10. Diagnostics

Every problem carries a line and column, a code and (when possible) a
suggestion:

```
error[unknown-object]: line 8, column 1: unknown object type 'spirke'
    did you mean: 'spike'
error[missing-variant]: line 9, column 1: 'orb' requires a variant
    usage: orb <variant> <x> <y>
    variants: 'yellow', 'pink', 'red', ...
```

The compiler never stops at the first error; it reports as many as it can
(de-duplicated, capped at 200).
