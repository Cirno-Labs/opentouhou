# th06 changelog

## Unreleased

- Added an optional frame rate display at the bottom of the screen,
  outside ending scenes, enabled by default.
- Changed package documents to `oth06_readme.txt` and
  `oth06_licenses.txt` so they do not replace the original game's readme.
- Changed the conversion of music and sound effects to the output rate,
  which keeps their high frequencies.
- Changed the text of `--help` and of command-line errors to a new
  format.
- Fixed a gamepad or joystick button bound to both shoot and focus: it
  now focuses only after it is held for 8 frames, as in the original.
- Fixed texture colors.
- Fixed transparency in translated textures.
- Fixed JPEG background colors and shading.
- Fixed a black screen showing instead of the startup logo.
- Fixed the timing of the startup logo's fade into the title screen.
- Fixed how stage backgrounds fade into fog with distance.
- Fixed faint, almost transparent sprite edges that the original does
  not draw.
- Fixed texture distortion and colour seams across sprites and
  backgrounds, which split along the wrong diagonal.
- Fixed overlapping sprites drawing in the wrong order, and items, the
  player, bombs and spell card backgrounds showing outside the
  playfield.
- Fixed background fades starting from black instead of from the
  previous frame.
- Fixed sprites, the power bar, the dialogue box and translator notes
  sitting half a pixel off.
- Fixed enemies flashing white when player shots hit them, and sprites
  sampled without the original's smoothing.
- Fixed orbiting boss effects in dark, saturated colours instead of the
  original's pale tints.
- Fixed the spacing of bold translated text.
- Fixed the shading of text in spell names, dialogue and stage titles.
- Fixed the placement of translated stage and song titles.
- Fixed the shape of translator notes, and their weight in fonts that
  patches bring.

## 1.0.1 (2026-11-15)

- Fixed the message about missing game data on macOS when macOS starts
  a temporary copy of the program: it now tells how to stop that.

## 1.0.0 (2026-11-13)

- Added th06, compatible with the original down to its replays: the 17
  world record replays play back frame for frame.
- Added native packages for Windows, Linux and macOS.
- Added English and other languages through othpatch.
- Added smooth motion between ticks on fast displays.
- Added `[video] scaling` in config.ini: sharp, smooth or integer
  scaling of the screen to the window.
- Added F11 and Alt+Enter to switch between window and fullscreen.
- Added rebindable keys and gamepad buttons.
- Added portable mode with a `portable.txt` beside the game.
- Added `--user-dir` to keep your files in another folder.
- Added a message box when the game cannot start or stops on an error.
- Added a log of the last start in `oth06.log` in the user folder.
- Added support for paths longer than 260 characters on Windows.
