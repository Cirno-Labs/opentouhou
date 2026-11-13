// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Runs the bombs of the four shot types on the bomb state of g_Player
// (player.h): the orbs of Reimu A, the pillars of Reimu B, the stars of
// Marisa A and the beam of Marisa B. Each bomb sets the boxes that damage
// enemies and clear bullets, keeps the player invulnerable, and ends after
// its duration.

// Runs the bomb of the shot type for a frame, while g_Player.bomb.running
// is set. On its first frame, the bomb shows its name with
// Gui_ShowBombName, collects every item and sets its duration. When the
// duration is over, it ends the name and clears g_Player.bomb.running.
void Bomb_Update(void);
