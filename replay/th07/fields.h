#pragma once

// Lists the columns of a th07 case, in file order. X(name, kind) for each,
// where kind says how a column is printed: U32, I32, F32 (bits) or HASH.
//
// Scores are in the game's units of ten points. cherry, cherry_max and
// cherry_plus are GameManager's bonusPoints, bonusPointsCap and
// invulnerabilityBonus less GameStats::grazeCount, as a replay stage
// stores them. events is the event word of the replay input played or
// recorded last in the stage, 0 before the first. flags is the low byte
// of GameManager::stateFlags | isPaused << 8 | isInContinueMenu << 16 |
// isFrozen << 24. spell_card is the active flag | the capture flag << 8
// | the card index << 16. deaths and bombs_used are GameStats' float
// counts as integers.

#define TH07_START_FIELDS(X)                                                   \
    X(difficulty, U32)                                                         \
    X(character, U32)                                                          \
    X(shot_type, U32)                                                          \
    X(stage, U32)                                                              \
    X(lives, F32)                                                              \
    X(bombs, F32)                                                              \
    X(power, F32)                                                              \
    X(score, U32)                                                              \
    X(rng_seed, U32)                                                           \
    X(rank, I32)                                                               \
    X(sub_rank, I32)                                                           \
    X(cherry, U32)                                                             \
    X(cherry_max, U32)                                                         \
    X(cherry_plus, U32)

// player_state 4 is the Supernatural Border.
#define TH07_CALC_FIELDS(X)                                                    \
    X(buttons, U32)                                                            \
    X(events, U32)                                                             \
    X(stage, U32)                                                              \
    X(stage_frames, U32)                                                       \
    X(flags, U32)                                                              \
    X(game_speed, F32)                                                         \
    X(score, U32)                                                              \
    X(gui_score, U32)                                                          \
    X(lives, F32)                                                              \
    X(bombs, F32)                                                              \
    X(power, F32)                                                              \
    X(graze, U32)                                                              \
    X(graze_total, U32)                                                        \
    X(point_items, U32)                                                        \
    X(point_items_total, U32)                                                  \
    X(cherry, U32)                                                             \
    X(cherry_max, U32)                                                         \
    X(cherry_plus, U32)                                                        \
    X(rank, I32)                                                               \
    X(sub_rank, I32)                                                           \
    X(deaths, U32)                                                             \
    X(bombs_used, U32)                                                         \
    X(spell_cards_captured, U32)                                               \
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
// float as its bits. A timer folds its current frame, then its fraction.
// A *_count column counts the slots folded.
//
//   - shot: state | damage << 16, pos x y z, velocity x y z, angle,
//     timer.
//   - bullet: state | flags << 16, active flags | grazed << 16, pos x y
//     z, velocity x y z, speed, angle, timer, command index, then each of
//     the five behaviours' timer current and turn count. The spare
//     bullet after the last is left out.
//   - laser: state | flags << 16, pos x y z, angle, start offset, end
//     offset, width, timer.
//   - enemy: pos x y z, life, angle, speed, script timer.
//   - item: type | state << 8, pos x y z, velocity x y z, timer.

#define TH07_FIELD_ENUM(name, kind) TH07_START_##name,
enum { TH07_START_FIELDS(TH07_FIELD_ENUM) TH07_START_COUNT };
#undef TH07_FIELD_ENUM

#define TH07_FIELD_ENUM(name, kind) TH07_CALC_##name,
enum { TH07_CALC_FIELDS(TH07_FIELD_ENUM) TH07_CALC_COUNT };
#undef TH07_FIELD_ENUM

#define TH07_FIELD_NAME(name, kind) #name,
#define TH07_START_NAMES TH07_START_FIELDS(TH07_FIELD_NAME)
#define TH07_CALC_NAMES TH07_CALC_FIELDS(TH07_FIELD_NAME)
