# th01 changelog

## Unreleased

- Added folder selection to the web player.
- Added support for more editions of the game, such as fan translations.
- Added support for zip and PC-98 hard disk image (.hdi) files to
  `--data`: a zip of the game folder, or an .hdi of the game, bare or
  in a zip, read without unpacking.
- Added the punctuation and international keys of Japanese keyboards to
  the keys that `config.ini` bindings can name.
- Changed web audio playback to reduce interruptions during slow frames.
- Changed the music_restart fix into the debouncing fix, which also
  stops the press that confirms a high-score name from choosing Yes at
  the Continue prompt.
- Fixed the start failing with "Truncated GRF streams" on a MASK.GRF
  whose header gives the wrong plane sizes, as fan translations write
  it.
- Fixed the Orb hitting Kikuri up to 16 pixels too far to her right;
  it now hits her only where it does in the original.

## 1.0.0 (2026-11-22)

- Added th01, a native port that plays the original game from its own
  files.
- Added the optional music_restart fix, on by default: the music room
  starts a track once per confirm press instead of restarting it while
  the key is held.
- Added native packages for Windows, Linux and macOS.
- Added `[video] scaling` in config.ini: sharp, smooth or integer
  scaling of the screen to the window.
- Added F11 and Alt+Enter to switch between window and fullscreen.
- Added rebindable keys and gamepad buttons.
- Added screenshots with F12 or Print Screen.
- Added portable mode with a `portable.txt` beside the game.
- Added `--user-dir` to keep your files in another folder.
- Added settings in config.ini, taken from REIIDEN.CFG on the first
  start.
- Added score files in the user folder, read from the game folder
  until the first new score.
- Added the built-in Shinonome 16 font for the text of the PC-98
  font ROM, or a dump of the ROM in `opentouhou/fonts/`.
- Added a message box when the game cannot start or stops on an error.
- Added a log of the last start in `oth01.log` in the user folder.
