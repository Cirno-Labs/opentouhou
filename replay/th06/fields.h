#pragma once

// Lists the columns of a th06 case, in file order. X(name, kind) for each,
// where kind says how a column is printed: U32, I32, F32 (bits) or HASH.

#define TH06_START_FIELDS(X)                                                   \
    X(difficulty, U32)                                                         \
    X(character, U32)                                                          \
    X(shot_type, U32)                                                          \
    X(stage, U32)                                                              \
    X(lives, I32)                                                              \
    X(bombs, I32)                                                              \
    X(power, U32)                                                              \
    X(score, U32)                                                              \
    X(rng_seed, U32)                                                           \
    X(rank, I32)                                                               \
    X(sub_rank, I32)                                                           \
    X(power_item_combo, U32)

#define TH06_CALC_FIELDS(X)                                                    \
    X(buttons, U32)                                                            \
    X(stage, U32)                                                              \
    X(stage_frames, U32)                                                       \
    X(flags, U32)                                                              \
    X(score, U32)                                                              \
    X(gui_score, U32)                                                          \
    X(lives, I32)                                                              \
    X(bombs, I32)                                                              \
    X(power, U32)                                                              \
    X(graze, U32)                                                              \
    X(point_items, U32)                                                        \
    X(rank, I32)                                                               \
    X(sub_rank, I32)                                                           \
    X(next_extend, U32)                                                        \
    X(rng_seed, U32)                                                           \
    X(rng_generated, U32)                                                      \
    X(player_x, F32)                                                           \
    X(player_y, F32)                                                           \
    X(player_state, U32)                                                       \
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

// A HASH column folds one kind's live slots in slot order with Case_Hash,
// from CASE_HASH_INIT: the slot index, then the values below as u32s, a
// float as its bits.
//
//   - shot: state | damage << 16, pos x, pos y, pos z.
//   - bullet: state | flags << 16, pos x y z, velocity x y z, speed,
//     angle, timer current, grazed.
//   - laser: state, pos x y z, angle, start_offset, end_offset, timer
//     current.
//   - enemy: pos x y z, life.
//   - item: type | state << 8, pos x y z.

#define TH06_FIELD_ENUM(name, kind) TH06_START_##name,
enum { TH06_START_FIELDS(TH06_FIELD_ENUM) TH06_START_COUNT };
#undef TH06_FIELD_ENUM

#define TH06_FIELD_ENUM(name, kind) TH06_CALC_##name,
enum { TH06_CALC_FIELDS(TH06_FIELD_ENUM) TH06_CALC_COUNT };
#undef TH06_FIELD_ENUM

#define TH06_FIELD_NAME(name, kind) #name,
#define TH06_START_NAMES TH06_START_FIELDS(TH06_FIELD_NAME)
#define TH06_CALC_NAMES TH06_CALC_FIELDS(TH06_FIELD_NAME)
