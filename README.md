# opentouhou

Unofficial cross-platform ports of the Touhou Project games, starting
with 東方紅魔郷 ~ the Embodiment of Scarlet Devil (Touhou 6).

Each port is built from a matching decompilation of the original game:

- [th06](https://github.com/Cirno-Labs/th06)
- [th07](https://github.com/Cirno-Labs/th07)

The decompilations reproduce the original executables byte for byte.
This repository takes their source once it is complete and turns it into
a native game for Linux, macOS and Windows, free of the matching
constraints.

This repository contains no game code or data. You need your own copy of
each game.

## Goals

- Native builds for Linux, macOS and Windows, 64-bit included
- Rendering, input and audio through SDL3 instead of Direct3D 8,
  DirectInput, DirectSound and the Win32 API
- Smooth frame rates: game logic stays at a fixed 60 Hz and rendering
  is interpolated between ticks
- In-game translations, switchable from the game's menus, with text
  rendered by the port instead of GDI
- Replays recorded with the original games play back, on every
  platform: the game logic reproduces the original floating-point math

## Status

The port of th06 in `src/th06/` is starting, written from its
decompilation. The shared layers are designed up front. th10
is built on a different engine from th06–th09, and the shared layer is
reviewed against it before its interfaces settle.

## Layout

```
src/core/   shared by every game: window, rendering, input, audio,
            text, file IO, patches, frame pacing
src/stg/    shared by the shooting games: archives, sprite scripts,
            dialogue, enemy scripts, bullets, scores, replays
src/th06/   one directory per game
docs/       documentation, starting with ARCHITECTURE.md
```

## Legal

Touhou Project is by ZUN / Team Shanghai Alice. This is an unofficial
fan project. It contains no assets or code from the original games, and
it is not affiliated with or endorsed by ZUN or Team Shanghai Alice.

## License

The source in this repository is licensed under the GNU General Public
License, version 3 only (`GPL-3.0-only`), see [`LICENSE`](LICENSE). The
license covers our code, not the original games or their data.
