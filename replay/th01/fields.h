#pragma once

// Lists the columns of a th01 case, in file order. X(name, kind) for each,
// where kind says how a column is printed: U32, I32, F32 (bits) or HASH.
// A row is written on each VSync, before the game handles it. program is
// 1 for the logo, 2 the opening, 3 gameplay and its Game Over scores, 4
// the ending and its scores. Columns of another program are 0, and
// so are all but buttons before a program has set up its data. The orb's
// doubles are split into their low and high 32 bits.

#define TH01_CALC_FIELDS(X)                                                    \
    X(buttons, U32)                                                            \
    X(program, U32)                                                            \
    X(vsync_count1, I32)                                                       \
    X(frame_rand, U32)                                                         \
    X(rng, U32)                                                                \
    X(track, U32)                                                              \
    X(track_starts, U32)                                                       \
    X(game_frames, U32)                                                        \
    X(stage, U32)                                                              \
    X(route, U32)                                                              \
    X(score, U32)                                                              \
    X(lives, I32)                                                              \
    X(bombs, I32)                                                              \
    X(pellet_speed, I32)                                                       \
    X(stage_time, U32)                                                         \
    X(player_left, I32)                                                        \
    X(player_flags, U32)                                                       \
    X(invincibility_time, I32)                                                 \
    X(bomb_frame, U32)                                                         \
    X(orb_left, I32)                                                           \
    X(orb_top, I32)                                                            \
    X(orb_velocity_x, I32)                                                     \
    X(orb_velocity_y_lo, U32)                                                  \
    X(orb_velocity_y_hi, U32)                                                  \
    X(orb_force_lo, U32)                                                       \
    X(orb_force_hi, U32)                                                       \
    X(orb_force_frame, I32)                                                    \
    X(card_combo, I32)                                                         \
    X(card_flip_cycle, U32)                                                    \
    X(card_count, U32)                                                         \
    X(card_hash, HASH)                                                         \
    X(boss, U32)                                                               \
    X(boss_hp, I32)                                                            \
    X(boss_phase, I32)                                                         \
    X(boss_phase_frame, I32)                                                   \
    X(pellet_count, U32)                                                       \
    X(pellet_hash, HASH)                                                       \
    X(shot_count, U32)                                                         \
    X(shot_hash, HASH)

// The run's choices, which a case keeps once, after the calc columns.
#define TH01_CHOICE_FIELDS(X)                                                  \
    X(difficulty, U32)                                                         \
    X(lives_extra, U32)                                                        \
    X(start_bombs, U32)                                                        \
    X(music, U32)

#define TH01_COLUMNS(X) TH01_CALC_FIELDS(X) TH01_CHOICE_FIELDS(X)

// The state that a case's [start] sets as the first Start enters gameplay:
// in the resident block before REIIDEN's first instruction, and in the
// session before native gameplay begins. stage is the stage id from 0 (4
// is SinGyoku's), route 0 for Makai and 1 for Jigoku, difficulty 0 for
// Easy to 3 for Lunatic, continues the run's total, scene_continues those
// of the start stage's scene, which the ending reads. boss_hp replaces the
// HP that each boss's setup sets, right after the setup sets it; Sariel's
// second form and Kikuri's timeout keep their own.
#define TH01_START_FIELDS(X)                                                   \
    X(stage, U32)                                                              \
    X(route, U32)                                                              \
    X(difficulty, U32)                                                         \
    X(lives, I32)                                                              \
    X(bombs, I32)                                                              \
    X(score, U32)                                                              \
    X(continues, U32)                                                          \
    X(pellet_speed, I32)                                                       \
    X(boss_hp, I32)                                                            \
    X(scene_continues, U32)

// The opening fills vsync_count1, frame_rand, rng, track (the music
// test's track) and track_starts (the tracks that the music test has
// started since the opening began); the ending fills
// vsync_count1, rng, route (its end flag) and score.
//
// player_flags: is_hit, invincible << 1, deflecting << 2, sliding << 3.
//
// A HASH column folds one kind's slots in slot order with State_Hash, from
// STATE_HASH_INIT: the slot index, then the values below as u32s.
//
//   - card: every card, flag | hp << 8, flip frame.
//   - pellet: moving slots, moving | motion_type << 8, left, top, velocity
//     x y, as subpixels.
//   - shot: moving slots, left, top.
