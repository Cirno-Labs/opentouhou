#pragma once

#include "stg/translation.h"

// Lists the text of the executable that the player sees, as
// X(id, address, text). address is the string's address in the retail
// 1.00b executable, or 0 where it is not known yet. text is the UTF-8
// default. A replacement of a format string keeps its conversions.
// stg/translation replaces the entries with the translations of patches.
#define TH07_STRINGS(X)                                                        \
    X(SHOT_NAME_REIMU_A, 0x4969b0, "博麗 霊夢 (霊)　")                         \
    X(SHOT_NAME_REIMU_B, 0x49699c, "博麗 霊夢 (夢)　")                         \
    X(SHOT_NAME_MARISA_A, 0x496988, "霧雨 魔理沙 (魔)")                        \
    X(SHOT_NAME_MARISA_B, 0x496974, "霧雨 魔理沙 (恋)")                        \
    X(SHOT_NAME_SAKUYA_A, 0x496960, "十六夜 咲夜 (幻)")                        \
    X(SHOT_NAME_SAKUYA_B, 0x49694c, "十六夜 咲夜 (時)")                        \
    X(SHOT_NAME_TOTAL, 0x496938, "全主人公合計  　")                           \
    X(SHOT_TYPE_REIMU_A, 0, "ReimuA ")                                         \
    X(SHOT_TYPE_REIMU_B, 0, "ReimuB ")                                         \
    X(SHOT_TYPE_MARISA_A, 0, "MarisaA")                                        \
    X(SHOT_TYPE_MARISA_B, 0, "MarisaB")                                        \
    X(SHOT_TYPE_SAKUYA_A, 0, "SakuyaA")                                        \
    X(SHOT_TYPE_SAKUYA_B, 0, "SakuyaB")                                        \
    X(RESULT_DIFFICULTY_EASY, 0x495628, "      Easy")                          \
    X(RESULT_DIFFICULTY_NORMAL, 0x49561c, "    Normal")                        \
    X(RESULT_DIFFICULTY_HARD, 0x495610, "      Hard")                          \
    X(RESULT_DIFFICULTY_LUNATIC, 0x495604, "   Lunatic")                       \
    X(RESULT_DIFFICULTY_EXTRA, 0x4955f8, "     Extra")                         \
    X(RESULT_DIFFICULTY_PHANTASM, 0x4955ec, "  Phantasm")                      \
    X(RESULT_UNKNOWN_SPELL_CARD, 0, "？？？？？")                              \
    X(RESULT_RANK, 0x496460, "%2d")                                            \
    X(REPLAY_SAVE_HEADER, 0x49641c, "No.   Name     Date   Player Score")      \
    X(REPLAY_SAVE_SLOT, 0x496400, "No.%.2d %8s %5s  %7s %9d0")                 \
    X(REPLAY_SAVE_EMPTY, 0x4963c8,                                             \
      "No.%.2d -------- --/--  -------          0")                            \
    X(MENU_REPLAY_HEADER, 0x49557c, "No.   Name       Date  Player   Rank")    \
    X(MENU_REPLAY, 0x495564, "%s %8s  %6s %7s  %8s")                           \
    X(MENU_STAGE_HEADER, 0x495540, "Stage    LastScore")                       \
    X(PRACTICE_STAGE_HEADER, 0x4954e4, "Stage    HI-Score")                    \
    X(MENU_STAGE, 0, "%s %9d0")                                                \
    X(MENU_STAGE_EMPTY, 0, "%s ----------")                                    \
    X(PRACTICE_SCORE, 0, "%s %9d0 (%3d)")                                      \
    X(PRACTICE_STAGE_1, 0x495420, "Stage1")                                    \
    X(PRACTICE_STAGE_2, 0x495418, "Stage2")                                    \
    X(PRACTICE_STAGE_3, 0x495410, "Stage3")                                    \
    X(PRACTICE_STAGE_4, 0x495408, "Stage4")                                    \
    X(PRACTICE_STAGE_5, 0x495400, "Stage5")                                    \
    X(PRACTICE_STAGE_6, 0x4954f8, "Stage6")                                    \
    X(MAX_BONUS, 0x496440, "MaxBonus %8d")                                     \
    X(RESULTS_HEADER, 0x4964b4, "No  Name      Score(Stage)  Date   Slow")     \
    X(SPELL_RESULT_HEADER, 0x4967ec,                                           \
      "%s %3d枚中%3d枚取得（キャラ切り替え↓↑）")                               \
    X(STATS_TIME_SINCE_STARTUP, 0x4967a0, "総起動時間   %.2d:%.2d:%.2d")       \
    X(STATS_TOTAL_PLAYTIME, 0x496784, "総プレイ時間 %.2d:%.2d:%.2d")           \
    X(STATS_PLAY_COUNT_HEADER, 0x496700,                                       \
      "プレイ回数　　　 　Easy 　Norm 　Hard 　Luna  Extra  Total")            \
    X(STATS_PLAY_COUNT_HEADER_PHANTASM, 0x496740,                              \
      "プレイ回数　　　 　Easy 　Norm 　Hard 　Luna  Extra Phants  Total")     \
    X(STATS_CLEAR_COUNT_LABEL, 0x496668,                                       \
      "クリア回数  　　 %6d %6d %6d %6d %6d %6d")                              \
    X(STATS_CONTINUE_LABEL, 0x49660c,                                          \
      "コンティニュー   %6d %6d %6d %6d %6d %6d")                              \
    X(STATS_PRACTICE_LABEL, 0x4965b0,                                          \
      "プラクティス　   %6d %6d %6d %6d %6d %6d")                              \
    X(STATS_RETRIES_LABEL, 0x496554, "リトライ回数  　 %6d %6d %6d %6d %6d %6d")

#define TH07_STRING_ID(id_, address_, text_) STRING_##id_,

typedef enum {
    TH07_STRINGS(TH07_STRING_ID) STRING_COUNT,
} STRING;

extern const char *g_Strings[STRING_COUNT];

// Describes th07 to Translation_Init.
extern const TRANSLATION_GAME g_TranslationGame;
