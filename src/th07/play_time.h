#pragma once

// Tracks play_stats.run_* and play_stats.play_* in GAME_MANAGER's records,
// as App::AddPlayTime and App::AddTotalPlayTime do, from the wall clock
// that shell/game's requests.h gives, never from the simulation.

// Adds the time since the last call to the game's run time, which the
// original counts from the program's start.
void PlayTime_AddRun(void);

// Adds the time since the last call or restart to the game's played time.
void PlayTime_Add(void);

// Restarts the played time clock to now, without adding.
void PlayTime_Restart(void);

// Stops the played time clock.
void PlayTime_Stop(void);
