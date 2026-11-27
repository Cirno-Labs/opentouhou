# th02 changelog

## Unreleased

- Added th02, a native port that plays the original game from its own
  files.
- Added native packages for Windows, Linux and macOS.
- Added support for zip and PC-98 hard disk image (.hdi) files to
  `--data`: a zip of the game folder, or an .hdi of the game, bare or
  in a zip, read without unpacking.
- Added `[video] scaling` in config.ini: sharp, smooth or integer
  scaling of the screen to the window.
- Added F11 and Alt+Enter to switch between window and fullscreen.
- Added rebindable keys and gamepad buttons, including the punctuation
  and international keys of Japanese keyboards.
- Added screenshots with F12 or Print Screen.
- Added portable mode with a `portable.txt` beside the game.
- Added `--user-dir` to keep your files in another folder.
- Added settings in config.ini, taken from HUUMA.CFG on the first
  start.
- Added score files in the user folder, read from the game folder
  until the first new score.
- Added the built-in Shinonome 16 font for the text of the PC-98
  font ROM, or a dump of the ROM in `opentouhou/fonts/`.
- Added a message box when the game cannot start or stops on an error.
- Added a log of the last start in `oth02.log` in the user folder.
- Added a default-on fix that omits the missing final bomb transition
  mask; disabling `[fixes] bomb_transition_overread` draws the memory that
  the original reads in its place.
