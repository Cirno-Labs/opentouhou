# th06 changelog

Each release lists what changed for players, newest first. A change in
core shows up here when the next th06 release includes it.

## Unreleased

- The game writes its log to `oth06.log` in the user folder, and
  replaces it at each start. The error message boxes name this file, so
  that a player can send it with a bug report.
- The game shows a message box when it cannot start or stops because
  of an error. Before this, a black window opened and closed at once.
  When the game data is missing, the box names the missing file, the
  folder that the game searched and how to fix it.
- Added Linux, Windows and macOS packages. Each package includes
  othpatch.
- Added English and other languages through othpatch. It downloads the
  translation patches of thpatch.net, and the game shows their fonts,
  their text markup, and the translated menus and title images.
- Added portable mode: with a `portable.txt` beside the game, the
  settings, scores and replays stay in the game's folder. `--user-dir`
  puts them into another folder.
- The Linux AppImage uses the folder that holds the AppImage file, not
  its own internal folder.
- On macOS, the game looks for the game data and `portable.txt` in the
  folder that holds `oth06.app`.
- The result screen and saved replays show the real slow rate of the
  game.
- Added screenshots, fullscreen and smooth motion between ticks.
- Ported the whole game: every stage with dialogue, sound and music,
  the menus, the result screen with scores and replays, the music room
  and the endings.
