#pragma once

// Lists the text of the executable that the player sees, as
// X(id, address, text). address is the string's address in the retail
// 1.02h executable, or 0 where it is not known yet. text is the UTF-8
// default. A replacement of a format string keeps its conversions.
// th06/translation replaces the entries with the translations of patches.
#define TH06_STRINGS(X)                                                        \
    X(SHOT_NAME_REIMU_A, 0x46bb98, "博麗 霊夢 (霊)")                           \
    X(SHOT_NAME_REIMU_B, 0x46bb88, "博麗 霊夢 (夢)")                           \
    X(SHOT_NAME_MARISA_A, 0x46bb74, "霧雨 魔理沙 (魔)")                        \
    X(SHOT_NAME_MARISA_B, 0x46bb60, "霧雨 魔理沙 (恋)")                        \
    X(SHOT_TYPE_REIMU_A, 0x46bd04, "ReimuA ")                                  \
    X(SHOT_TYPE_REIMU_B, 0x46bcfc, "ReimuB ")                                  \
    X(SHOT_TYPE_MARISA_A, 0x46bcf4, "MarisaA")                                 \
    X(SHOT_TYPE_MARISA_B, 0x46bcec, "MarisaB")                                 \
    X(RESULT_DIFFICULTY_EASY, 0x46bc70, "     Easy")                           \
    X(RESULT_DIFFICULTY_NORMAL, 0x46bc64, "   Normal")                         \
    X(RESULT_DIFFICULTY_HARD, 0x46bc58, "     Hard")                           \
    X(RESULT_DIFFICULTY_LUNATIC, 0x46bc4c, "  Lunatic")                        \
    X(RESULT_DIFFICULTY_EXTRA, 0x46bc40, "    Extra")                          \
    X(RESULT_RANK, 0x46bd94, "%2d")                                            \
    X(RESULT_SCORE, 0x46bdc0, "%8s %9d(%d)")                                   \
    X(RESULT_SCORE_EXTRA, 0x46bdb4, "%8s %9d(1)")                              \
    X(RESULT_SCORE_CLEAR, 0x46bda8, "%8s %9d(C)")                              \
    X(RESULT_UNKNOWN_SPELL_CARD, 0, "？？？？？")                              \
    X(REPLAY_SAVE_HEADER, 0x46bd5c, "No.   Name     Date     Player Score")    \
    X(REPLAY_SAVE_SLOT, 0x46bd44, "No.%.2d %8s %8s %7s %9d")                   \
    X(REPLAY_SAVE_EMPTY, 0x46bd0c,                                             \
      "No.%.2d -------- --/--/-- -------         0")                           \
    X(MENU_REPLAY_HEADER, 0x46c49c, "No.   Name      Date     Player   Rank")  \
    X(MENU_REPLAY, 0x46c484, "%s %8s  %8s %7s  %7s")                           \
    X(MENU_STAGE_HEADER, 0x46c460, "Stage  LastScore")                         \
    X(MENU_STAGE, 0x46c458, "%s %9d")                                          \
    X(MENU_STAGE_EMPTY, 0x46c448, "%s ---------")                              \
    X(MENU_STAGE_1, 0x46c418, "Stage1")                                        \
    X(MENU_STAGE_2, 0x46c410, "Stage2")                                        \
    X(MENU_STAGE_3, 0x46c408, "Stage3")                                        \
    X(MENU_STAGE_4, 0x46c400, "Stage4")                                        \
    X(MENU_STAGE_5, 0x46c3f8, "Stage5")                                        \
    X(MENU_STAGE_6, 0x46c3f0, "Stage6")                                        \
    X(MENU_STAGE_EXTRA, 0x46c3e8, "Extra ")                                    \
    X(MENU_SHOT_TYPE_REIMU_A, 0, "ReimuA ")                                    \
    X(MENU_SHOT_TYPE_REIMU_B, 0, "ReimuB ")                                    \
    X(MENU_SHOT_TYPE_MARISA_A, 0, "MarisaA")                                   \
    X(MENU_SHOT_TYPE_MARISA_B, 0, "MarisaB")                                   \
    X(MENU_DIFFICULTY_EASY, 0x46c440, "Easy   ")                               \
    X(MENU_DIFFICULTY_NORMAL, 0x46c438, "Normal ")                             \
    X(MENU_DIFFICULTY_HARD, 0x46c430, "Hard   ")                               \
    X(MENU_DIFFICULTY_LUNATIC, 0x46c428, "Lunatic")                            \
    X(MENU_DIFFICULTY_EXTRA, 0x46c420, "Extra  ")                              \
    X(MENU_PRACTICE_SCORE, 0x46c4c8, "STAGE %d  %.9d")                         \
    X(SONG_NAME_FORMAT, 0x46a98c, "♪%s")                                       \
    X(BOMB_NAME_REIMU_A, 0x46a3f4, "霊符「夢想封印」")                         \
    X(BOMB_NAME_REIMU_B, 0x46a410, "夢符「封魔陣」")                           \
    X(BOMB_NAME_MARISA_A, 0x46a420, "魔符「スターダストレヴァリエ」")          \
    X(BOMB_NAME_MARISA_B, 0x46a450, "恋符「マスタースパーク」")

#define TH06_STRING_ID(id_, address_, text_) STRING_##id_,

typedef enum {
    TH06_STRINGS(TH06_STRING_ID) STRING_COUNT,
} STRING;

extern const char *g_Strings[STRING_COUNT];
