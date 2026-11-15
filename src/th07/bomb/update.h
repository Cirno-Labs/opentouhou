#pragma once

// Runs the twelve bombs, one per shot type unfocused and focused, on the
// bomb state of g_Player (player.h). Each bomb moves its parts, sets the
// damage boxes and clear areas of g_Player that act this frame, keeps the
// player invulnerable and ends after its duration. Parts move and timers
// run at g_GameSpeed.

// Runs the bomb of g_GameManager.run.shot_index, the focused one while
// g_Player.bomb.focused is set, for a frame while g_Player.bomb.running is
// set. On its first frame, the bomb shows its name with Gui_ShowBombName,
// collects every item, and sets its duration, its cherry drain, the state
// timer and the bomb aura. When the duration is over, it ends the name,
// restores the movement speed and clears g_Player.bomb.running.
void Bomb_Update(void);
