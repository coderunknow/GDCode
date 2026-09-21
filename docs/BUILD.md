# Building

## Requirements

- CMake >= 3.21, Ninja (or any generator)
- For the mod: [Geode SDK](https://docs.geode-sdk.org/getting-started/) v5.10.x
  and the Geode CLI. Geode requires Clang >= 19 (Windows/macOS/Android) or
  MSVC >= 19.44; see the Geode docs.
- For the compiler core + tests only: GCC 12 works (root CMake selects C++23, core uses C++20 features).

## Mod (`.geode`)

```sh
geode build            # in the repo root; needs GEODE_SDK to be set by the CLI
```

or manually:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # GEODE_SDK must be in the environment
cmake --build build
```

The resulting `coderunknow.gdcode.geode` is copied into your GD mods folder by
the CLI. Root `CMakeLists.txt` detects `GEODE_SDK` and switches into mod mode.

### CI

`.github/workflows/build.yml` builds the mod for Windows, macOS, iOS,
Android32 and Android64 with `geode-sdk/build-geode-mod` (SDK version taken
from `mod.json`), combines them into one `.geode`, verifies the package
structure (`.github/scripts/inspect-geode.sh`: `mod.json`, one binary per
platform, resources) and uploads it as the artifact **GDCode (all
platforms)**. It also runs the compiler test-suite on Ubuntu.

A final `CI report` job posts one summary **commit comment** per run (job
conclusions and compiler warnings) plus one detail comment per failed job
(errors with context, log tail); the package job posts the `.geode` listing
the same way (`.github/scripts/ci-report.sh`). This exists because GitHub
serves raw logs and artifacts from a storage host that is not reachable from
every environment, while the API is; the job is purely informational and can
be removed without affecting the build.

## Compiler core, tests and CLI (no game needed)

```sh
env -u GEODE_SDK cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core
ctest --test-dir build-core --output-on-failure
./build-core/gdcode-cli examples/hello.gdx --level-string
```

Golden snapshot files live in `tests/golden/`. After an intentional change to
the compiler output regenerate them with:

```sh
GDCODE_UPDATE_GOLDEN=1 ./build-core/tests/test_golden
```

and review the diff.

## Installing a CI build

1. Download the artifact from the workflow run and unzip it.
2. Copy `coderunknow.gdcode.geode` into `<GD folder>/geode/mods/`
   (or drag it onto the Geode mods window).
3. Start the game; the `</>` button appears in the main menu.

## v0.2.0 validation

`ctest` includes the seven compiler suites plus navigation-contract and CLI
integration tests. Native pause/exit must still be tested in GD; see
[VALIDATION_v0.2.0.md](VALIDATION_v0.2.0.md). Host ASan/UBSan check:

```sh
env -u GEODE_SDK cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

The non-PIE flags here are for Linux host sanitizer runs, not mod builds.
