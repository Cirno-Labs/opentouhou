# Architecture

This document is the skeleton: the platform and tooling, the layers, the
program structure and the rules every change follows. Each module's
public header describes the module itself.

## Goals

- One native executable per game, needing only the original game data.
- Linux and Windows first, macOS and the web later. No x86-only code.
- Replays recorded with the original games play back exactly.
- Game logic at a fixed 60 Hz, rendering at any refresh rate.
- Everything thcrap does, native and optional.

## Platform and tooling

- **SDL3** for the window, OpenGL context, events, input, audio device,
  paths and the main loop. **SDL3_ttf** for text. **FFmpeg** for image
  and audio codecs. OpenGL 3.3 core, OpenGL ES 3.0 and WebGL 2 share one
  set of `.glsl` shaders, compiled by the driver at startup.
- **C23**, **Meson**, one static library per layer and one executable
  per game. Dependencies are built from source, statically, in one
  Docker image per target. clang-format formats, prek runs the checks.
  [BUILDING.md](BUILDING.md) has the details.
- Platform code lives only in a module's `backend_<platform>.c`. Meson
  picks one platform per build. Public headers never include platform
  headers. Only `core` uses SDL.
- The web build ships later but constrains the design now: the main
  loop returns to the browser every frame, threads are optional, and
  files live in browser storage.

## Layers

```
src/core/    every game: platform, rendering, audio, input, text,
             files, patches, maths
src/stg/     what the shooting games share beyond core
src/thXX/    one directory per game
```

- `core` depends on no other layer and has no game concepts.
- `stg` depends on `core`. A game depends on `stg` and `core`. Games
  never depend on each other.
- Code moves into `stg` when a second game needs it, not before.
- The build enforces this: each layer is a static library that sees only
  its own and its dependencies' headers. Code includes `core/app.h`.

Inside a layer, each module has one public header that other modules
include: `src/core/vfs.h`. Its code is one source file beside the
header (`src/stg/rng.c`) or a directory of the same name
(`src/core/vfs/`). A module made of independent parts has one header
per part in its directory (`src/core/gfx/draw.h`), and its public
header only includes them (`src/core/gfx.h`). A layer can keep a
group of related modules together in one folder.

## Program structure

`core/app` owns SDL3's main callbacks and drives the game through one
table that each game defines:

```c
typedef struct {
    const char *id;            // "thXX"
    const char *title;         // window title
    int logical_width;         // 640
    int logical_height;        // 480
    const INPUT_SCHEMA *input_schema;
    RESULT (*init)(void);
    // One 60 Hz step: advance the game and record what to show.
    bool (*tick)(const void *input, FRAME *frame);
    void (*shutdown)(void);
} GAME_DESC;
```

A **tick** is one 60 Hz step of the game. A **display frame** is one
image on screen. Each display frame reads the input devices, runs as
many ticks as the clock allows, and renders the last two recorded
`FRAME`s, interpolated.

`tick` updates the game and records its draw commands into a `FRAME` in
one call, because the originals interleave the two and some of their
drawing code changes state. It never touches the GPU. The renderer pairs
records by id between two frames to interpolate them; at 60 Hz this
costs nothing.

## Simulation state and determinism

These rules apply to every line of code `tick` reaches. How a game
stores its state is the game's own decision.

- The simulation reads only its own state, the input it was given and
  the data files it loaded. It may call `core/math` and other pure
  functions, and post events (sounds, music changes) that it never reads
  back.
- It never reads the clock, the file system after loading, audio or
  window state, or anything another thread writes. A value such as a
  measured frame rate comes from `core/app` as a per-tick value that
  replays record.

### Floating point

The simulation reproduces the original's floating-point results bit for
bit.

- Every build uses `-ffp-contract=off`, no `-ffast-math`, and SSE2 on
  x86, so every platform computes the same thing.
- Simulation code calls only replay-exact maths from `core/math`, never
  the C library's.
- A refactor of simulation code never reorders float operations.

Each game's notes say what replay-exact means for its executable.

### RNG

Gameplay randomness comes from the game's own generator, which is never
seeded from the clock during play. Other randomness uses a separate
generator.

## Modules

```
core/base     types, asserts, logging, allocators, containers, strings, hashes
core/io       readers and writers, files, paths
core/ini      the INI format our settings are stored in
core/vfs      the file namespace the games load from
core/json     JSON5 parsing, lookups, indexes, merging
core/patch    thcrap patch repositories and stacks
core/math     vectors, matrices, replay-exact primitives
core/image    image decoding, encoding and conversion
core/gfx      rendering
core/text     fonts, layout, text to texture
core/audio    device, mixer, sources, DSP
core/input    devices, bindings, per-player input for each tick
core/config   our settings
core/app      main loop, window, frame pacing, the GAME_DESC driver
```

Dependencies inside `core` go down this list, except that `gfx` and
`text` use `image`, and `app` uses everything.

Two rules hold across modules:

- Every file format is parsed field by field through a `READER`, never
  by copying bytes into a struct.
- Text is UTF-8 everywhere. Shift-JIS exists only when original data is
  read or original-format files are written.

`stg` holds ZUN's archive formats, the random number generator, the ANM
sprite engine, MSG dialogue, the ECL script VM, the gameplay managers
and the original file formats, as far as games share them. Each `src/thXX/` holds that game's
`GAME_DESC`, scenes, rules, tables and features.

## Data, saves and config

- Original data is read from the data directory and never written.
- Scores and replays exist in our native format, in the user directory,
  and in the original format, in the data directory. A per-game
  `save_format` setting chooses, `native` by default.
- Our settings live in one INI file per game in the user directory.

## Features

Every change to an original game's behaviour is a named feature with a
default:

- **fix:** does not change gameplay. On by default.
- **gameplay:** changes what happens in play. Off by default, recorded
  in replays, and it disables original-format replay saving.
- **presentation:** changes only what is shown. Off by default.

## Testing

Unit tests cover `core` and the format readers, and run in CI.

### Replay tests

Replay tests check that a game does what its original did, tick for
tick. They run locally against the developer's own game data, so
neither the cases nor the data are in this repository or in CI.

A case is one binary file: a header naming the game and an initial
checkpoint, then one row of named values per calc, written and read through
`replay/core/case.h`. A capture tool per game, in `replay/thXX/`,
produces cases from the original executable. Each game's headless
runner drives its input and reports the first calc where a value differs.
