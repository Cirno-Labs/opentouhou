#pragma once

// Holds animation and screen-effect timers during title initialization.
void GameTime_Hold(void);

// Restores one frame of timer advance per calculation.
void GameTime_Resume(void);

// Returns zero while timers are held, otherwise one.
float GameTime_Step(void);
