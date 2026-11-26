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

A case can start the game's first run at a chosen state instead of
playing up to it, such as a boss stage with spare lives: the script's
`start NAME VALUE` lines set the game's start fields, which the case
keeps in `[start]`. Each backend's port writes the same values in memory
at the same point, as the run begins, and never changes a file. th01's
fields are in [fields.h](../replay/th01/fields.h); its port sets them
in the resident block before REIIDEN's first instruction, and in the
session as native gameplay begins. Its boss_hp and stage_time fields
differ: the port writes them over the HP that each boss's setup sets and
the time that each stage's timer starts with, right after the game sets
them, on both backends. Cases that set every field to what the run
would have anyway match the cases that set none, row for row.

`tools/stage_rpy` captures a case that plays one of the game's own
replay files: it stages the file at the slot that `replay/GAME/rpy.ini`
names and plays the input script that it names, which selects the slot
and lets the replay play. The harness itself never reads a replay file.

The recorder, such as `th06record`, `th07record` or `th01record`, plays
a run with a bot on the native game and saves it in the game's own
format: th06 and th07 write a `.rpy`, th01 a case script for
`tools/harness capture`. The recorder plays holds of roles (shoot,
focus, bomb, strike and the four directions), and no behavior of its
own: it searches the holds directly against the game's score.
`replay/GAME/bot.ini` maps each role to the game's buttons, with one
word per frame, so a role can be a sequence such as th01's bomb.
[bot.h](../replay/bot/bot.h) gives the format.

```
th06record --data DATA --out OUT.rpy --start difficulty=normal \
    --start stage=1 --start character=reimu --start shot=a --seed 1
th07record --data DATA --out OUT.rpy --start difficulty=normal \
    --start stage=1 --start character=reimu --start shot=a --minimize
th01record --data DATA --out OUT.txt --start stage=19 --start route=0 \
    --start lives=6 --start bombs=9 --start boss_hp=18 --until 8
```

`--start` sets the run's start values: th06's and th07's practice
choices, or the start fields of a th01 case. The recorder grows the run
by Monte Carlo tree search over prefixes without a setback, such as a
lost life, made of random and mutated segments of holds; `--seed` seeds
the search. Each game scores its runs: th01 by the boss's phase or the
cards removed, th06 and th07 by the stage frames. The run ends with the
game, at the game's frame cap, or with `--until` once the first value of
its score reaches a value; when the search runs out first, the recorder
saves its best run. With `--minimize`, the search keeps only
segments that add a setback, for a game over.
[recorder.h](../replay/bot/recorder.h) describes the search. The same
arguments always give the same run.

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
- `replay::dosbox` checks a PC-98 port against its original on the
  dosbox backend, and `replay::live-dosbox` does it for a person's play.
- `replay::dosbox-cases` captures one case per script of a directory on
  the original; `replay::check` then replays them all on the native
  backend. Both walk subdirectories, and the scripts live next to their
  cases in the cases repository, so both take its game directory:

  ```
  just replay::dosbox-cases th01 DATA CASES/th01 CASES/th01
  just replay::check th01 DATA CASES/th01
  ```
- `replay::coverage` replays every case of a directory on a coverage
  build of the native backend, and lists each source file's share of
  lines run, lowest first, with the functions that no case reaches. It
  needs `llvm-cov` on the machine.
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
files, never the player's own saves, and writes into `work/`. A run
fails when `work/` already holds files, since the game would read an
earlier run's saves and settings there.

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
difficulty and character, once rather than in every row, its
`[preconditions]` the staging steps in order, and `[start]` the start
values.

`state.bin` packs the rows into chunks that each decode on their own,
with a running hash that a replay checks on every row.

[case.h](../replay/harness/case.h) gives `case.ini` and the case
format, [script.h](../replay/harness/script.h) the script format of
`inputs.txt` and [state.h](../replay/harness/state.h) the layout of
`state.bin`, with its chunk size and the rows between checks. A HASH column
holds a value only on those check rows, every 60th; `tools/harness rows` shows
`-` on the others. A case
never holds screenshots or original game data.
