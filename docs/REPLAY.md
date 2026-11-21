# Replay harness

The replay harness checks that a game does the same thing on every run,
frame for frame. It drives the game with scripted input, samples game
state after every calc, and takes screenshots on request. Shared code
knows no game: a game adds one description and one port per backend.

## Running

`tools/harness capture` runs a game with an input script or a person's
play and writes a new case; `tools/harness replay` runs a case and
compares each state row with the case's. `tools/harness --help` and the
help of each verb give their options and exit status, and
[backend.h](../replay/harness/backend.h) the command line of the backend
programs that the tool starts.

A capture can stage files among the game's, such as a replay in a replay
slot, and the case keeps them, so every replay starts from the same
files. A replay with screenshots draws nothing after its last one. A
fast replay draws only the frames just before each screenshot, and has
the game repaint its whole screen as they start, so its screenshots are
close to a full run's, not exact.

`tools/stage_rpy` captures a case that plays one of the game's own
replay files: it stages the file at the slot that `replay/GAME/rpy.ini`
names and plays the input script that it names, which selects the slot
and lets the replay play. The harness itself never reads a replay file.

The recorder, such as `th06record`, plays one practice stage with a bot
on the native game and saves it as the game's own replay file, for new
replay cases. The bot's shared part is in `replay/bot/`; a game gives
its choices, such as difficulties and stages, as data.

## Recipes

[replay/justfile](../replay/justfile) runs the tools for the usual
checks:

- `replay::native` checks the native backend on its own captures, rows
  and screenshots.
- `replay::check` replays every case of a directory on the native
  backend, in parallel.
- `replay::retail` checks the port against the original, and with
  screenshots compares the two backends' frames. It needs msgothic.ttc
  in the Fonts folder of the Wine prefix as a real file, not a link
  into WSL's /mnt/c, which the original's 32-bit FreeType cannot open.
- `replay::rpy-cases` captures the original playing each `.rpy` file of
  a directory.
- `replay::live` captures a person's play of the original.
- `replay::dosbox-x` builds the patched DOSBox-X of the dosbox backend
  into the local image `opentouhou-dosbox-x`, `replay::dosbox-x-test`
  checks its hooks, and `replay::dosbox-test` checks the backend end to
  end with its test game.

## Output

A replay writes into its output directory:

```
state.bin        the rows of this run
report.ini       its frames, rows and screenshots, the first row that
                 differs with each differing column's expected and
                 actual value and the lists that differ at the next
                 check, and with --against the count of pairs that
                 differ
shots/F.png      the screenshot of frame F, F with 6 digits
xor/, delta/     with --against: images of each pair that differs
shots.tsv        with --against: one line per pair
work/            every file that the game wrote
```

Screenshots are PNG files of the logical screen and never go into a
case. The game's directory stays unchanged: the game reads the staged
files, never the player's own saves, and writes into `work/`.

## Code

- [backend.h](../replay/harness/backend.h): the command line, frames, the
  port that each game gives per backend, and the driver's calls.
- [game.h](../replay/harness/game.h): what a game describes, such as its
  bindings, state columns and choices.
- [native.h](../replay/harness/native.h): the native backend, `GAMEnative`.
- [retail.h](../replay/retail/retail.h): the retail backend,
  `GAMEretail.exe` and `GAMEretail.dll`, for an original Windows game.
- [dosbox.h](../replay/dosbox/dosbox.h): the dosbox backend, `GAMEdosbox`
  and `GAMEdosbox.so`, for an original PC-98 DOS game in a patched
  DOSBox-X ([hook.h](../replay/dosbox/hook.h)). `tools/harness --backend
  dosbox` runs it in a container of `opentouhou-dosbox-x`.

## Cases

A case is one ZIP file, `NAME.othtc`, with every entry deflated, so
`unzip -l` lists it and `unzip -p NAME.othtc case.ini` prints an entry:

```
case.ini     manifest
inputs.txt   frame-indexed input and frame rate script
state.bin    the rows, one per calc
files/NAME   each file that a staging step serves
```

`case.ini` holds the case format, the game and its version, the
capturing backend, the columns version, the frame and row counts and a
note. Its `[choices]` section holds the run's choices, such as th06's
difficulty and character, once rather than in every row, and its
`[preconditions]` the staging steps in order.

`state.bin` packs the rows into chunks that each decode on their own,
with a running hash that a replay checks on every row.

[case.h](../replay/harness/case.h) gives `case.ini` and the case
format, [script.h](../replay/harness/script.h) the script format of
`inputs.txt` and [state.h](../replay/harness/state.h) the layout of
`state.bin`, with its chunk size and the rows between checks. A case
never holds screenshots or original game data.
