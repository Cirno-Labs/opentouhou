#pragma once

#include "stg/translation.h"

// Lists the text of the executable that the player sees, as
// X(id, address, text). address is the string's address in the retail
// 1.00b executable, or 0 where it is not known yet. text is the UTF-8
// default. A replacement of a format string keeps its conversions.
// stg/translation replaces the entries with the translations of patches.
#define TH07_STRINGS(X)                                                        \
    X(STAGE_CLEAR_TITLE, 0x498518, "Stage Clear")                              \
    X(ALL_CLEAR_TITLE, 0x49850c, "All Clear!")                                 \
    X(CLEAR_STAGE, 0x4984fc, "Clear  = %8d")                                   \
    X(CLEAR_POINT, 0x4984ec, "Point  = %8d")                                   \
    X(CLEAR_GRAZE, 0x4984dc, "Graze  = %8d")                                   \
    X(CLEAR_CHERRY, 0x4984cc, "Cherry = %8d")                                  \
    X(CLEAR_PLAYER, 0x4984c0, "Player =%9d")                                   \
    X(CLEAR_BOMB, 0x4984b0, "Bomb   = %8d")                                    \
    X(CLEAR_EASY, 0x49849c, "Easy Rank    *0.5")                               \
    X(CLEAR_NORMAL, 0x498488, "Normal Rank  *1.0")                             \
    X(CLEAR_HARD, 0x498474, "Hard Rank    *1.2")                               \
    X(CLEAR_LUNATIC, 0x498460, "Lunatic Rank *1.5")                            \
    X(CLEAR_EXTRA, 0x49844c, "Extra Rank   *2.0")                              \
    X(CLEAR_PHANTASM, 0x498438, "Phantasm Rank*2.0")                           \
    X(CLEAR_PENALTY_HALF, 0x498424, "Player Penalty*0.5")                      \
    X(CLEAR_PENALTY_FIFTH, 0x498410, "Player Penalty*0.2")                     \
    X(CLEAR_TOTAL, 0x498400, "Total = %8d0")                                   \
    X(SPELL_BONUS_NOTICE, 0x49838c, "Spell Card Bonus!")                       \
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
    X(BOMB_REIMU_A_UNFOCUSED, 0x498874, "霊符「夢想封印　散」")                \
    X(BOMB_REIMU_A_FOCUSED, 0x49885c, "霊符「夢想封印　集」")                  \
    X(BOMB_REIMU_B_UNFOCUSED, 0x49884c, "夢符「封魔陣」")                      \
    X(BOMB_REIMU_B_FOCUSED, 0x498838, "夢符「二重結界」")                      \
    X(BOMB_MARISA_A_UNFOCUSED, 0x498818, "魔符「スターダストレヴァリエ」")     \
    X(BOMB_MARISA_A_FOCUSED, 0x498800, "魔符「ミルキーウェイ」")               \
    X(BOMB_MARISA_B_UNFOCUSED, 0x4987d8,                                       \
      "恋符「ノンディレクショナルレーザー」")                                  \
    X(BOMB_MARISA_B_FOCUSED, 0x4987bc, "恋符「マスタースパーク」")             \
    X(BOMB_SAKUYA_A_UNFOCUSED, 0x49879c, "幻符「インディスクリミネイト」")     \
    X(BOMB_SAKUYA_A_FOCUSED, 0x498788, "幻符「殺人ドール」")                   \
    X(BOMB_SAKUYA_B_UNFOCUSED, 0x498768, "時符「パーフェクトスクウェア」")     \
    X(BOMB_SAKUYA_B_FOCUSED, 0x498748, "時符「プライベートスクウェア」")       \
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
    X(NOTICE_FULL_POWER, 0x4983e0, "Full Power Mode!")                         \
    X(NOTICE_BORDER_START, 0x4983c8, "Supernatural Border!!")                  \
    X(NOTICE_CHERRY_MAX, 0x4983b4, "CherryPoint Max!")                         \
    X(NOTICE_BORDER_BONUS, 0x4983a0, "Border Bonus %7d")                       \
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
    X(STATS_RETRIES_LABEL, 0x496554,                                           \
      "リトライ回数  　 %6d %6d %6d %6d %6d %6d")                              \
    X(HELP_MENU_START, 0x495aa4, "ゲームを開始します")                         \
    X(HELP_MENU_EXTRA_START, 0x495a84, "エキストラステージを開始します")       \
    X(HELP_MENU_PRACTICE_START, 0x495a60,                                      \
      "ステージを選択し、練習を開始します")                                    \
    X(HELP_MENU_REPLAY, 0x495a48, "リプレイを鑑賞できます")                    \
    X(HELP_MENU_RESULT, 0x495a18,                                              \
      "過去のスコアやスペルカードの取得歴を見られます")                        \
    X(HELP_MENU_MUSIC_ROOM, 0x495a08, "音楽を聴けます")                        \
    X(HELP_MENU_OPTION, 0x4959f4, "各種設定できます")                          \
    X(HELP_MENU_QUIT, 0x4959dc, "いろいろと終了します")                        \
    X(HELP_OPTION_PLAYER, 0x49596c,                                            \
      "プレイヤーの初期数を変更します。（初期設定　３）")                      \
    X(HELP_OPTION_GRAPHIC, 0x49592c,                                           \
      "画面の色数を変更します。３２ＢＩＴだと最も綺麗に表示されます。")        \
    X(HELP_OPTION_BGM, 0x4958f8,                                               \
      "ＢＧＭの再生方法を変更します。（初期設定　ＷＡＶ）")                    \
    X(HELP_OPTION_SOUND, 0x4958d8, "効果音を再生するか選択します")             \
    X(HELP_OPTION_WINDOW_MODE, 0x4958b0,                                       \
      "ウィンドウかフルスクリーンか選択します")                                \
    X(HELP_OPTION_SLOW_MODE, 0x495870,                                         \
      "弾が多い場面でわざと処理落ちさせます(スコア、リプレイ記録不可)")        \
    X(HELP_OPTION_RESET, 0x495858, "全て初期設定にします")                     \
    X(HELP_OPTION_KEY_CONFIG, 0x495834, "パッド操作のボタン配置を変更します")  \
    X(HELP_OPTION_QUIT, 0x49581c, "おいそれと終了します")                      \
    X(HELP_KEY_SHOT, 0x4957e8, "ショット、決定ボタンを設定します")             \
    X(HELP_KEY_BOMB, 0x4957c4, "ボム、キャンセルボタンを設定します")           \
    X(HELP_KEY_SLOW, 0x4957a8, "低速移動ボタンを設定します")                   \
    X(HELP_KEY_SKIP, 0x495780, "メッセージスキップボタンを設定します")         \
    X(HELP_KEY_PAUSE, 0x495764, "ポーズボタンを設定します")                    \
    X(HELP_KEY_UP, 0x495748, "上移動ボタンを設定します")                       \
    X(HELP_KEY_DOWN, 0x49572c, "下移動ボタンを設定します")                     \
    X(HELP_KEY_LEFT, 0x495710, "左移動ボタンを設定します")                     \
    X(HELP_KEY_RIGHT, 0x4956f4, "右移動ボタンを設定します")                    \
    X(HELP_KEY_SHOTSLOW, 0x4956c0,                                             \
      "ショット押しっぱなしで低速移動になるようにします")                      \
    X(HELP_KEY_RESET, 0x4956ac, "初期設定に戻します")                          \
    X(HELP_KEY_QUIT, 0x495698, "おおよそ終了します")

#define TH07_STRING_ID(id_, address_, text_) STRING_##id_,

typedef enum {
    TH07_STRINGS(TH07_STRING_ID) STRING_COUNT,
} STRING;

extern const char *g_Strings[STRING_COUNT];

// Describes th07 to Translation_Init.
extern const TRANSLATION_GAME g_TranslationGame;
