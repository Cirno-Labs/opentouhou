# th07 changelog

## Unreleased

- Added support for zip files to `--data`: a zip of the game folder,
  read without unpacking.

## 1.0.0 (2026-11-20)

- Added th07, compatible with original-format replays.
- Added the optional replay_desync fix, on by default: replays keep
  their saved stage scores, and replays recorded with VsyncPatch no
  longer skip a line of dialogue after a pause.
- Added the optional input_lag fix, off by default: the game acts on a
  button press a frame sooner, as with VsyncPatch.
- Added native packages for Windows, Linux and macOS.
- Added English and other languages through othpatch.
- Added smooth motion between ticks on fast displays.
- Added `[video] scaling` in config.ini: sharp, smooth or integer
  scaling of the screen to the window.
- Added F11 and Alt+Enter to switch between window and fullscreen.
- Added rebindable keys.
- Added portable mode with a `portable.txt` beside the game.
- Added `--user-dir` to keep your files in another folder.
- Added a message box when the game cannot start or stops on an error.
- Added a log of the last start in `oth07.log` in the user folder.
- Added support for paths longer than 260 characters on Windows.
