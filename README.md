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

Replays from the original games are not expected to play back: the
original floating-point math cannot be reproduced exactly on modern
compilers and other architectures.

## Status

Nothing yet. Work starts once the decompilations up to and including
th10 are complete. th10 is built on a different engine from th06–th09,
so the shared layer will be designed with both engines in view instead
of being fitted to the older one.

## Layout

```
lib/platform/   shared layer: window, rendering, input, audio, text,
                translations, frame pacing
src/games/      one port per game (th06plus, th07plus, ...)
```

## Legal

Touhou Project is by ZUN / Team Shanghai Alice. This is an unofficial
fan project. It contains no assets or code from the original games, and
it is not affiliated with or endorsed by ZUN or Team Shanghai Alice.
