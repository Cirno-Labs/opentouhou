#pragma once

// Lists the columns of a th07 case, in file order. X(name, kind) for each,
// where kind says how a column is printed: U32, I32, F32 (bits) or HASH.
// Lives, bombs and power are floats in th07. The cherry columns are the
// values the game shows, without the offset kept against tampering.
// Outside the gameplay scene, the columns from score to cherry_plus are 0,
// as the original's title holds an uninitialised copy of them.

#define TH07_CALC_FIELDS(X)                                                    \
    X(buttons, U32)                                                            \
    X(stage, U32)                                                              \
    X(stage_frames, U32)                                                       \
    X(game_speed, F32)                                                         \
    X(score, U32)                                                              \
    X(gui_score, U32)                                                          \
    X(lives, F32)                                                              \
    X(bombs, F32)                                                              \
    X(power, F32)                                                              \
    X(graze, I32)                                                              \
    X(point_items, I32)                                                        \
    X(next_extend_items, I32)                                                  \
    X(cherry, I32)                                                             \
    X(cherry_max, I32)                                                         \
    X(cherry_plus, I32)                                                        \
    X(rank, I32)                                                               \
    X(sub_rank, I32)                                                           \
    X(rng_seed, U32)                                                           \
    X(rng_generated, U32)                                                      \
    X(player_x, F32)                                                           \
    X(player_y, F32)                                                           \
    X(player_state, U32)                                                       \
    X(border_state, U32)                                                       \
    X(shot_count, U32)                                                         \
    X(shot_hash, HASH)                                                         \
    X(bullet_count, U32)                                                       \
    X(bullet_hash, HASH)                                                       \
    X(laser_count, U32)                                                        \
    X(laser_hash, HASH)                                                        \
    X(spell_card, U32)                                                         \
    X(spell_card_bonus, U32)                                                   \
    X(enemy_count, U32)                                                        \
    X(enemy_hash, HASH)                                                        \
    X(item_count, U32)                                                         \
    X(item_hash, HASH)

// The run's choices, which a case keeps once, after the calc columns.
// Difficulty 4 is Extra and 5 Phantasm.
#define TH07_CHOICE_FIELDS(X)                                                  \
    X(difficulty, U32)                                                         \
    X(character, U32)                                                          \
    X(shot_type, U32)

#define TH07_COLUMNS(X) TH07_CALC_FIELDS(X) TH07_CHOICE_FIELDS(X)

// A HASH column folds one kind's live slots in slot order with State_Hash,
// from STATE_HASH_INIT: the slot index, then the values below as u32s, a
// float as its bits.
//
//   - shot: state | damage << 16, pos x y z, velocity x y z.
//   - bullet: state | flags << 16, pos x y z, velocity x y z, speed,
//     angle, timer current, grazed.
//   - laser: state, pos x y z, angle, start_offset, end_offset, timer
//     current.
//   - enemy: pos x y z, life.
//   - item: type | state << 8, pos x y z.
