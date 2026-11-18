# th06 changelog

## Unreleased

- Added the frame rate display outside ending scenes.
- Changed the bundled Japanese font to MS Gothic.
- Changed the text of `--help` and of command-line errors to a new
  format.
- Changed the conversion of music and sound effects to the output rate,
  which keeps their high frequencies.
- Fixed a gamepad or joystick button bound to both shoot and focus: it
  now focuses only after it is held for 8 frames, as in the original.
- Fixed the letters and spacing in translated text.
- Fixed the right half of translated Music Room comments.
- Fixed menu highlight colors, difficulty and character selection
  colors, and the selected graphics setting.
- Fixed JPEG background colors and shading.
- Fixed the startup logo being replaced by a black screen.
- Fixed missing results-screen labels and darkened results artwork.
- Fixed score widths on translated results screens.
- Fixed the size, shape, shading, sharpness and placement of text in
  spell names, dialogue, translator notes and Japanese stage and song
  titles.
- Fixed sprite placement, fading backgrounds, fog, texture colors,
  title logo colors, filtering and faint sprite edges.
- Fixed texture distortion and shading seams in sprites and
  backgrounds.
- Fixed transparency in translated textures.
- Fixed gradient sampling in the power bar and dialogue box.
- Fixed the alignment of translated stage labels, bonus notices and
  spell card bonus labels.
- Fixed white flashes on enemies when player shots hit them.
- Fixed the colors of orbiting boss effects.
- Fixed depth ordering for overlapping sprites and clipping at the
  playfield edges.

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
