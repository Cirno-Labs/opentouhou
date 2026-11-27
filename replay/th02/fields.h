#pragma once

// Samples each reference VSync before it is delivered. Logo fields are zero
// outside the logo; opening fields are zero outside OP. program is 1 for
// the logo, 2 for OP, 3 for MAIN and 4 for MAINE.
// Samples live MAIN scalars and both MAINE VSync counters.
// MAIN and MAINE VRAM hashes fold both raw 640 by 400 color-index pages;
// palette hashes fold sixteen RGBA entries.
// Each page and the palette use byte-wise FNV-1a; the two page hashes are
// folded as words into vram_hash, which in MAIN leaves out the unloaded
// tile sources. All MAIN and MAINE scenes sample both hashes.
#define TH02_CALC_FIELDS(X)                                                    \
    X(buttons, U32)                                                            \
    X(program, U32)                                                            \
    X(logo_frame, U32)                                                         \
    X(count1, U32)                                                             \
    X(count2, U32)                                                             \
    X(frame, I32)                                                              \
    X(selection, I32)                                                          \
    X(in_options, U32)                                                         \
    X(idle, U32)                                                               \
    X(main_ready, U32)                                                         \
    X(main_input, U32)                                                         \
    X(options_ready, U32)                                                      \
    X(options_input, U32)                                                      \
    X(difficulty, U32)                                                         \
    X(music, U32)                                                              \
    X(bombs, U32)                                                              \
    X(lives, U32)                                                              \
    X(reduce_effects, U32)                                                     \
    X(stage, U32)                                                              \
    X(score, I32)                                                              \
    X(score_lives, I32)                                                        \
    X(score_bombs, I32)                                                        \
    X(power, U32)                                                              \
    X(rng, U32)                                                                \
    X(rng_cursor, U32)                                                         \
    X(player_x, I32)                                                           \
    X(player_y, I32)                                                           \
    X(vram_hash, HASH)                                                         \
    X(palette_hash, HASH)

// Profile 1 uses the shipped Shinonome 16-pixel BDF pair on both backends.
#define TH02_FONT_PROFILE_SHINONOME 1
#define TH02_CHOICE_FIELDS(X) X(font_profile, U32)
#define TH02_COLUMNS(X) TH02_CALC_FIELDS(X) TH02_CHOICE_FIELDS(X)

// Applies values on the first Start handoff. Stage 127 enters MAINE.
// Lives and bombs set both remaining and starting counters.
// boss_hp applies after each boss/phase damage reset; original phase
// timing, invulnerability and scripted damage caps remain in effect.
#define TH02_START_FIELDS(X)                                                   \
    X(stage, U32)                                                              \
    X(shot_type, U32)                                                          \
    X(rank, I32)                                                               \
    X(score, I32)                                                              \
    X(continues, U32)                                                          \
    X(lives, I32)                                                              \
    X(bombs, I32)                                                              \
    X(power, I32)                                                              \
    X(skill, I32)                                                              \
    X(boss_hp, I32)
