#include "th07/gui.h"
#include "th07/gui/private.h"

#include "test.h"
#include "th07_gui_setup.h"
#include "th07_msg_builder.h"

#include "th07/anm.h"
#include "th07/bullet.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/stage.h"

#include "core/base/color.h"
#include "core/io/reader.h"

#define M_KOUMA "\xe7\xb4\x85\xe9\xad\x94" // 紅魔 in UTF-8
#define M_SHOOT 0x1
#define M_SKIP 0x100
#define M_CAPTION_SLOT 0x30

// Reads the bytes of msg into file.
static RESULT M_Read(const MSG_BUILDER *const msg, GUI_MSG_FILE *const file)
{
    static uint8_t bytes[MSG_BUILDER_MAX_SIZE];
    const size_t size = MsgBuilder_Build(msg, bytes);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return GuiMsg_Read(&reader, file);
}

// Checks that msg fails to read and leaves the file empty.
static void M_CheckRejected(const MSG_BUILDER *const msg)
{
    GUI_MSG_FILE file = { .count = 7 };
    CHECK(!IS_OK(M_Read(msg, &file)));
    CHECK_EQ_INT(file.count, 0);
    CHECK(file.scripts == nullptr);
}

TEST(a_good_file_reads_every_instruction)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpWho(&msg, s, 0, GUI_MSG_OP_PORTRAIT_SCRIPT, 1, 3);
    MsgBuilder_OpWho(&msg, s, 1, GUI_MSG_OP_PORTRAIT_SPRITE, 0, 2);
    MsgBuilder_OpText(&msg, s, 2, GUI_MSG_OP_LINE, 1, 1, M_KOUMA);
    MsgBuilder_OpInt(&msg, s, 3, GUI_MSG_OP_WAIT, 90);
    MsgBuilder_OpWho(&msg, s, 4, GUI_MSG_OP_INTERRUPT, 3, 0x102);
    MsgBuilder_Op(&msg, s, 5, GUI_MSG_OP_RELEASE_TIMELINE);
    MsgBuilder_OpInt(&msg, s, 6, GUI_MSG_OP_SONG, 3);
    MsgBuilder_OpText(&msg, s, 7, GUI_MSG_OP_NAMEPLATE, 0, 0, "Reimu");
    MsgBuilder_Op(&msg, s, 8, GUI_MSG_OP_STAGE_CLEAR);
    MsgBuilder_Op(&msg, s, 9, GUI_MSG_OP_FADE_OUT_MUSIC);
    MsgBuilder_OpByte(&msg, s, 10, GUI_MSG_OP_SKIPPABLE, 1);
    MsgBuilder_Op(&msg, s, 11, GUI_MSG_OP_FADE_OUT_SCREEN);
    MsgBuilder_Op(&msg, s, 12, GUI_MSG_OP_HALT);
    // Past the halt, nothing is read.
    MsgBuilder_Op(&msg, s, 13, 0xff);
    const int32_t t = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, t, 0, GUI_MSG_OP_NEXT_SCENE);
    const int32_t u = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, u, 0, GUI_MSG_OP_END);
    GUI_MSG_FILE file;
    CHECK(Result_Log(M_Read(&msg, &file), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(file.count, 3);
    CHECK_EQ_INT(file.scripts[0].count, 13);
    CHECK_EQ_INT(file.scripts[1].count, 1);
    CHECK_EQ_INT(file.scripts[2].count, 1);
    const GUI_MSG_INSTR *const in = file.scripts[0].instrs;
    for (int32_t i = 0; i < 13; i++) {
        CHECK_EQ_INT(in[i].time, i);
    }
    CHECK_EQ_INT(in[0].opcode, GUI_MSG_OP_PORTRAIT_SCRIPT);
    CHECK_EQ_INT(in[0].who, 1);
    CHECK_EQ_INT(in[0].index, 3);
    CHECK_EQ_INT(in[1].who, 0);
    CHECK_EQ_INT(in[1].index, 2);
    CHECK_EQ_INT(in[2].who, 1);
    CHECK_EQ_INT(in[2].index, 1);
    CHECK_EQ_STR(in[2].text, M_KOUMA);
    CHECK_EQ_INT(in[3].value, 90);
    CHECK_EQ_INT(in[4].who, 3);
    CHECK_EQ_INT(in[4].index, 2); // the low byte only
    CHECK_EQ_INT(in[6].value, 3);
    CHECK_EQ_STR(in[7].text, "Reimu");
    CHECK_EQ_INT(in[10].value, 1);
    CHECK_EQ_INT(in[11].opcode, GUI_MSG_OP_FADE_OUT_SCREEN);
    CHECK_EQ_INT(in[12].opcode, GUI_MSG_OP_HALT);
    CHECK_EQ_INT(file.scripts[1].instrs[0].opcode, GUI_MSG_OP_NEXT_SCENE);
    GuiMsg_Free(&file);
    CHECK_EQ_INT(file.count, 0);
}

TEST(a_short_header_fails_the_read)
{
    GUI_MSG_FILE file = { .count = 7 };
    static const uint8_t bytes[2] = { 1, 0 };
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(!IS_OK(GuiMsg_Read(&reader, &file)));
    CHECK_EQ_INT(file.count, 0);
    // A count of offsets that the file cannot hold fails too.
    static const uint8_t many[8] = { 9, 0, 0, 0, 8, 0, 0, 0 };
    Reader_OpenBuffer(&reader, many, sizeof(many), nullptr);
    CHECK(!IS_OK(GuiMsg_Read(&reader, &file)));
    CHECK(file.scripts == nullptr);
}

TEST(a_bad_offset_fails_the_read)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_END);
    MsgBuilder_SetOffset(&msg, 0, -4);
    M_CheckRejected(&msg);
    MsgBuilder_SetOffset(&msg, 0, 0x1000);
    M_CheckRejected(&msg);
}

TEST(a_script_without_an_end_fails_the_read)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_RELEASE_TIMELINE);
    M_CheckRejected(&msg);
    // A second script that is good does not keep the first one.
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_END);
    MsgBuilder_Op(
        &msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_RELEASE_TIMELINE);
    M_CheckRejected(&msg);
}

TEST(short_arguments_fail_the_read)
{
    static const struct {
        uint8_t opcode;
        uint8_t size;
    } cases[] = {
        { GUI_MSG_OP_PORTRAIT_SCRIPT, 3 },
        { GUI_MSG_OP_PORTRAIT_SPRITE, 3 },
        { GUI_MSG_OP_LINE, 4 },
        { GUI_MSG_OP_WAIT, 3 },
        { GUI_MSG_OP_INTERRUPT, 2 },
        { GUI_MSG_OP_SONG, 3 },
        { GUI_MSG_OP_NAMEPLATE, 4 },
        { GUI_MSG_OP_SKIPPABLE, 0 },
    };
    static const uint8_t zeros[8] = {};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        MSG_BUILDER msg;
        MsgBuilder_Init(&msg);
        const int32_t s = MsgBuilder_BeginScript(&msg);
        MsgBuilder_Instr(&msg, s, 0, cases[i].opcode, zeros, cases[i].size);
        MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
        M_CheckRejected(&msg);
        // One more byte reads.
        MsgBuilder_Init(&msg);
        const int32_t t = MsgBuilder_BeginScript(&msg);
        MsgBuilder_Instr(
            &msg, t, 0, cases[i].opcode, zeros, (uint8_t)(cases[i].size + 1));
        MsgBuilder_Op(&msg, t, 0, GUI_MSG_OP_END);
        GUI_MSG_FILE file;
        CHECK(Result_Log(M_Read(&msg, &file), LOG_LEVEL_ERROR));
        GuiMsg_Free(&file);
    }
}

TEST(an_unknown_opcode_fails_the_read)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_FADE_OUT_SCREEN + 1);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
    M_CheckRejected(&msg);
}

TEST(a_bad_text_fails_the_read)
{
    // Without its NUL, with a lead byte that no trail byte follows, and in
    // UTF-8.
    static const uint8_t no_end[] = { 0, 0, 0, 0, 'a', 'b' };
    static const uint8_t bad[] = { 0, 0, 0, 0, 0x81, 0x20, 0 };
    static const uint8_t utf8[] = { 0, 0, 0, 0, 0xe7, 0xb4, 0x85, 0 };
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Instr(&msg, s, 0, GUI_MSG_OP_LINE, no_end, sizeof(no_end));
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
    M_CheckRejected(&msg);
    MsgBuilder_Init(&msg);
    s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Instr(&msg, s, 0, GUI_MSG_OP_NAMEPLATE, bad, sizeof(bad));
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
    M_CheckRejected(&msg);
    MsgBuilder_Init(&msg);
    s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Instr(&msg, s, 0, GUI_MSG_OP_LINE, utf8, sizeof(utf8));
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
    M_CheckRejected(&msg);
}

TEST(names_out_of_range_fail_the_read)
{
    static const struct {
        uint8_t opcode;
        int16_t who, index;
        bool good;
    } cases[] = {
        { GUI_MSG_OP_PORTRAIT_SCRIPT, 2, 0, false },
        { GUI_MSG_OP_PORTRAIT_SPRITE, -1, 0, false },
        { GUI_MSG_OP_PORTRAIT_SPRITE, 1, 9, true },
        { GUI_MSG_OP_LINE, 2, 0, false },
        { GUI_MSG_OP_LINE, 0, 2, false },
        { GUI_MSG_OP_NAMEPLATE, 1, -1, false },
        { GUI_MSG_OP_NAMEPLATE, 1, 1, true },
        { GUI_MSG_OP_INTERRUPT, 4, 1, false },
        { GUI_MSG_OP_INTERRUPT, 3, 1, true },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        MSG_BUILDER msg;
        MsgBuilder_Init(&msg);
        const int32_t s = MsgBuilder_BeginScript(&msg);
        if (cases[i].opcode == GUI_MSG_OP_LINE
            || cases[i].opcode == GUI_MSG_OP_NAMEPLATE) {
            MsgBuilder_OpText(
                &msg, s, 0, cases[i].opcode, cases[i].who, cases[i].index, "a");
        } else {
            MsgBuilder_OpWho(
                &msg, s, 0, cases[i].opcode, cases[i].who, cases[i].index);
        }
        MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
        if (cases[i].good) {
            GUI_MSG_FILE file;
            CHECK(Result_Log(M_Read(&msg, &file), LOG_LEVEL_ERROR));
            GuiMsg_Free(&file);
        } else {
            M_CheckRejected(&msg);
        }
    }
}

TEST(a_song_out_of_range_fails_the_read)
{
    static const int32_t songs[] = { -1, STAGE_SONG_COUNT };
    for (size_t i = 0; i < 2; i++) {
        MSG_BUILDER msg;
        MsgBuilder_Init(&msg);
        const int32_t s = MsgBuilder_BeginScript(&msg);
        MsgBuilder_OpInt(&msg, s, 0, GUI_MSG_OP_SONG, songs[i]);
        MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_END);
        M_CheckRejected(&msg);
    }
}

TEST(a_rejected_file_fails_the_add)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(
        &msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_RELEASE_TIMELINE);
    CHECK(!IS_OK(GuiSetup_SetUpStage("rejected", 1, &msg)));
    CHECK(Anm_GetScript(0x700) == nullptr);
    CHECK(Anm_GetScript(0x4a0) == nullptr);
    GuiSetup_TearDown();
}

TEST(registering_loads_the_files_of_the_stage)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_END);
    GuiSetup_SetUp("register", 1, &msg);
    CHECK(Anm_GetScript(0x700) != nullptr);
    CHECK(Anm_GetScript(0x725) != nullptr);
    CHECK(Anm_GetScript(0x4a2) != nullptr);
    CHECK(Anm_GetScript(0x805) != nullptr);
    CHECK(!Gui_IsMsgRunning());
    CHECK_EQ_INT(g_Gui.loading.icon.sprite, -1);
    CHECK_EQ_INT(g_Gui.loading.snapshot.sprite, -1);
    GuiSetup_TearDown();
}

TEST(starting_a_missing_script_does_nothing)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_END);
    GuiSetup_SetUp("missing", 1, &msg);
    Gui_StartMsg(1);
    CHECK(!Gui_IsMsgRunning());
    CHECK_EQ_INT(g_GuiCalls.bullets_cleared, 0);
    CHECK_EQ_INT(g_GuiCalls.items_collected, 0);
    GuiSetup_TearDown();
}

TEST(starting_a_script_clears_the_play_area)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("start", 1, &msg);
    g_Gui.msg.timeline_release = 3;
    g_Gui.msg.wait_frames = 3;
    Gui_StartMsg(0);
    CHECK(Gui_IsMsgRunning());
    CHECK(Gui_MsgHoldsTimeline());
    CHECK(Gui_IsMsgSkippable());
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 0);
    CHECK_EQ_INT(g_Gui.msg.lines[0].script_index, -1);
    CHECK_EQ_INT(g_Gui.msg.lines[1].script_index, -1);
    CHECK_EQ_INT(g_GuiCalls.bullets_cleared, 1);
    CHECK_EQ_INT(g_GuiCalls.enemies_cleared, 1);
    CHECK_EQ_INT(g_GuiCalls.enemy_max_bonus, 0);
    CHECK_EQ_INT(g_GuiCalls.enemy_start, 0);
    CHECK_EQ_INT(g_GuiCalls.items_collected, 1);
    CHECK(g_Bullets.clear_item == ITEM_STAR);
    GuiSetup_TearDown();
}

TEST(the_last_stages_set_the_spell_background)
{
    static const struct {
        int32_t stage;
        int32_t offset, count;
    } cases[] = { { 6, 2, 0 }, { 7, 1, 2 }, { 8, 2, 0 } };
    for (size_t i = 0; i < 3; i++) {
        MSG_BUILDER msg;
        MsgBuilder_Init(&msg);
        for (int32_t j = 0; j < 11; j++) {
            MsgBuilder_Op(
                &msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_HALT);
        }
        GuiSetup_SetUp("last_stages", cases[i].stage, &msg);
        Gui_StartMsg(1);
        CHECK_EQ_INT(g_Stage.spell_background.offset, 0);
        CHECK(g_Bullets.clear_item == ITEM_STAR);
        Gui_StartMsg(10);
        CHECK_EQ_INT(g_Stage.spell_background.offset, cases[i].offset);
        CHECK_EQ_INT(g_Stage.spell_background.count, cases[i].count);
        CHECK(g_Bullets.clear_item == ITEM_BOSS_STAR);
        GuiSetup_TearDown();
    }
    // Stage 5 sets neither.
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("stage_5", 5, &msg);
    Gui_StartMsg(0);
    CHECK_EQ_INT(g_Stage.spell_background.offset, 0);
    CHECK(g_Bullets.clear_item == ITEM_STAR);
    GuiSetup_TearDown();
}

TEST(an_instruction_runs_on_its_frame)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, s, 3, GUI_MSG_OP_RELEASE_TIMELINE);
    MsgBuilder_Op(&msg, s, 3, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("frame", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(3);
    CHECK_EQ_INT(g_Gui.msg.pc, 0);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 3);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 1);
    // The halt holds the script and its timer.
    GuiSetup_Run(5);
    CHECK_EQ_INT(g_Gui.msg.pc, 1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 3);
    GuiSetup_TearDown();
}

TEST(the_timer_runs_at_the_game_speed)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 10, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("speed", 1, &msg);
    GuiSetup_StartMsg(0);
    g_GameSpeed = 0.5f;
    GuiSetup_Run(4);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 2);
    GuiSetup_TearDown();
}

TEST(each_frame_ends_the_border_and_collects_the_items)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("border", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(2);
    CHECK_EQ_INT(g_GuiCalls.borders_ended, 0);
    CHECK_EQ_INT(g_GuiCalls.items_collected, 2);
    g_Player.border_state = PLAYER_BORDER_PENDING;
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.borders_ended, 1);
    g_Player.border_state = PLAYER_BORDER_RUNNING;
    g_Player.state.value = PLAYER_DYING;
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.borders_ended, 2);
    CHECK_EQ_INT(g_GuiCalls.items_collected, 3);
    GuiSetup_TearDown();
}

TEST(a_release_lets_the_timeline_run_for_one_frame)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, s, 1, GUI_MSG_OP_RELEASE_TIMELINE);
    MsgBuilder_Op(&msg, s, 1, GUI_MSG_OP_RELEASE_TIMELINE);
    MsgBuilder_Op(&msg, s, 1, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("release", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(2);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 2);
    CHECK(!Gui_MsgHoldsTimeline());
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 1);
    GuiSetup_Run(1);
    CHECK(Gui_MsgHoldsTimeline());
    GuiSetup_TearDown();
}

TEST(a_wait_ends_after_its_frames)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpInt(&msg, s, 0, GUI_MSG_OP_WAIT, 3);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("wait", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(3);
    CHECK_EQ_INT(g_Gui.msg.pc, 0);
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 3);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 1);
    GuiSetup_TearDown();
}

TEST(the_shoot_button_ends_a_wait_after_12_frames)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpInt(&msg, s, 0, GUI_MSG_OP_WAIT, 1000);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("shoot", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(11);
    GuiSetup_Press(M_SHOOT);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 0);
    // A held button does not end it.
    GuiSetup_Press(M_SHOOT);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 0);
    GuiSetup_Press(0);
    GuiSetup_Run(1);
    GuiSetup_Press(M_SHOOT);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 1);
    GuiSetup_TearDown();
}

TEST(skip_fast_forwards_a_skippable_script)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpInt(&msg, s, 0, GUI_MSG_OP_WAIT, 1000);
    MsgBuilder_Op(&msg, s, 40, GUI_MSG_OP_RELEASE_TIMELINE);
    MsgBuilder_Op(&msg, s, 200, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("skip", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Press(M_SKIP);
    // The wait ends, the timer jumps to the release and then opens the
    // box.
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, M_BOX_OPEN_FRAMES);
    // The next frame jumps to the release, and the box keeps the timer
    // at 60 until the halt.
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.pc, 2);
    CHECK_EQ_INT(g_Gui.msg.timer.current, M_BOX_OPEN_FRAMES);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 200);
    GuiSetup_TearDown();
}

TEST(an_unskippable_script_ignores_skip)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpByte(&msg, s, 0, GUI_MSG_OP_SKIPPABLE, 0);
    MsgBuilder_OpInt(&msg, s, 0, GUI_MSG_OP_WAIT, 1000);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("unskippable", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Press(M_SKIP);
    GuiSetup_Run(3);
    CHECK(!Gui_IsMsgSkippable());
    CHECK_EQ_INT(g_Gui.msg.pc, 1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    GuiSetup_TearDown();
}

TEST(the_portrait_instructions_place_wide_sprites)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpWho(&msg, s, 0, GUI_MSG_OP_PORTRAIT_SCRIPT, 1, 0);
    MsgBuilder_OpWho(&msg, s, 1, GUI_MSG_OP_PORTRAIT_SPRITE, 0, 2);
    MsgBuilder_OpWho(&msg, s, 2, GUI_MSG_OP_PORTRAIT_SPRITE, 0, 3);
    MsgBuilder_OpWho(&msg, s, 3, GUI_MSG_OP_PORTRAIT_SPRITE, 0, 0);
    MsgBuilder_OpWho(&msg, s, 4, GUI_MSG_OP_PORTRAIT_SPRITE, 1, 1);
    MsgBuilder_Op(&msg, s, 4, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("portraits", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(1);
    CHECK(g_Gui.msg.portraits[1].script == Anm_GetScript(0x4a2));
    CHECK(g_Gui.msg.portraits[1].pos_offset.x == 0.0f);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.portraits[0].sprite, 0x4a2);
    CHECK(g_Gui.msg.portraits[0].pos_offset.x == -80.0f);
    GuiSetup_Run(1);
    CHECK(g_Gui.msg.portraits[0].pos_offset.x == -208.0f);
    CHECK(g_Gui.msg.portraits[0].pos_offset.y == -50.0f);
    GuiSetup_Run(1);
    // A narrow sprite keeps the y offset of a wider one.
    CHECK(g_Gui.msg.portraits[0].pos_offset.x == 0.0f);
    CHECK(g_Gui.msg.portraits[0].pos_offset.y == -50.0f);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.portraits[1].sprite, 0x4ae);
    GuiSetup_TearDown();
}

TEST(a_first_line_blanks_a_shown_second_line)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpText(&msg, s, 0, GUI_MSG_OP_LINE, 0, 1, "second");
    MsgBuilder_OpText(&msg, s, 1, GUI_MSG_OP_LINE, 1, 0, "first");
    MsgBuilder_Op(&msg, s, 1, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("lines", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(1);
    CHECK(g_Gui.msg.lines[1].script == Anm_GetScript(0x701));
    CHECK(g_Gui.msg.lines[0].script == nullptr);
    CHECK_EQ_INT(g_Gui.msg.lines[1].font_width, 15);
    g_Gui.msg.lines[1].shown = false;
    g_Gui.msg.wait_frames = 5;
    GuiSetup_Run(1);
    CHECK(g_Gui.msg.lines[0].script == Anm_GetScript(0x700));
    CHECK(g_Gui.msg.lines[1].shown);
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 0);
    GuiSetup_TearDown();
}

TEST(a_nameplate_starts_its_script)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpText(&msg, s, 0, GUI_MSG_OP_NAMEPLATE, 1, 1, "Yuyuko");
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("nameplate", 1, &msg);
    GuiSetup_StartMsg(0);
    g_Gui.msg.wait_frames = 5;
    GuiSetup_Run(1);
    CHECK(g_Gui.msg.nameplates[1].script == Anm_GetScript(0x703));
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 0);
    GuiSetup_TearDown();
}

TEST(an_interrupt_reaches_a_portrait_or_a_line)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_OpWho(&msg, s, 0, GUI_MSG_OP_PORTRAIT_SCRIPT, 1, 0);
    MsgBuilder_OpText(&msg, s, 0, GUI_MSG_OP_LINE, 0, 1, "a");
    MsgBuilder_OpWho(&msg, s, 1, GUI_MSG_OP_INTERRUPT, 1, GUI_SETUP_LABEL);
    MsgBuilder_OpWho(&msg, s, 1, GUI_MSG_OP_INTERRUPT, 3, GUI_SETUP_LABEL);
    MsgBuilder_Op(&msg, s, 1, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("interrupt", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.portraits[1].sprite, 0x4a0);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.msg.portraits[1].sprite, 0x4a1);
    CHECK_EQ_INT(g_Gui.msg.lines[1].sprite, 0x601);
    GuiSetup_TearDown();
}

TEST(a_song_shows_its_title_and_plays)
{
    static const struct {
        int32_t stage;
        int32_t script;
    } cases[] = { { 1, 0x804 }, { 6, 0x805 } };
    for (size_t i = 0; i < 2; i++) {
        MSG_BUILDER msg;
        MsgBuilder_Init(&msg);
        const int32_t s = MsgBuilder_BeginScript(&msg);
        MsgBuilder_OpInt(&msg, s, 0, GUI_MSG_OP_SONG, 1);
        MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
        GuiSetup_SetUp("song", cases[i].stage, &msg);
        char path[] = "bgm/th07_02.mid";
        g_Stage.data.song_paths[1] = path;
        GuiSetup_StartMsg(0);
        GuiSetup_Run(1);
        CHECK(g_Gui.banners.titles[0].script == Anm_GetScript(cases[i].script));
        CHECK_EQ_INT(g_Gui.banners.titles[0].sprite, 0x804);
        CHECK(g_SoundEvents.music == SOUND_MUSIC_PLAY);
        CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th07_02.wav");
        g_Stage.data.song_paths[1] = nullptr;
        GuiSetup_TearDown();
    }
}

TEST(stage_6_plays_its_first_song_on_frame_300)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_END);
    GuiSetup_SetUp("frame_300", 6, &msg);
    char path[] = "bgm/th07_11.mid";
    g_Stage.data.song_paths[0] = path;
    GuiSetup_Run(299);
    CHECK(g_SoundEvents.music == SOUND_MUSIC_KEEP);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.frames, 300);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th07_11.wav");
    g_Stage.data.song_paths[0] = nullptr;
    GuiSetup_TearDown();
}

TEST(the_fade_instructions_fade_the_music_and_the_screen)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_FADE_OUT_MUSIC);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_FADE_OUT_SCREEN);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp("fades", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(1);
    CHECK(g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.fade_seconds, 4);
    CHECK_EQ_INT(g_GuiCalls.screen_fades, 1);
    CHECK_EQ_INT(g_GuiCalls.screen_fade_duration, 0x192);
    CHECK_EQ_INT(
        g_GuiCalls.screen_fade_color, COLOR_ARGB(0x00, 0xff, 0xff, 0xff));
    GuiSetup_TearDown();
}

static void M_SetUpStageClear(const char *const case_name, const int32_t stage)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    const int32_t s = MsgBuilder_BeginScript(&msg);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_STAGE_CLEAR);
    MsgBuilder_Op(&msg, s, 0, GUI_MSG_OP_HALT);
    GuiSetup_SetUp(case_name, stage, &msg);
    g_GameManager.player.point_items = 120;
    g_GameManager.player.graze = 300;
    g_GameManager.cherry.offset = 1000;
    g_GameManager.cherry.max = 1000 + 70000;
    g_GameManager.player.extends = 2;
    GuiSetup_StartMsg(0);
}

TEST(the_stage_clear_starts_the_loading_screen)
{
    M_SetUpStageClear("stage_clear", 5);
    GuiSetup_Run(1);
    CHECK(g_Gui.stage_clear.state == GUI_STAGE_CLEAR_PENDING);
    CHECK_EQ_INT(g_Gui.stage_clear.point_items, 120);
    CHECK_EQ_INT(g_Gui.stage_clear.graze, 300);
    CHECK_EQ_INT(g_Gui.stage_clear.cherry, 70000);
    CHECK(g_Gui.loading.icon.script == Anm_GetScript(0x61e));
    CHECK(g_Gui.loading.snapshot.script == Anm_GetScript(0x725));
    // The snapshot's copy is pending, so another request is refused.
    CHECK(!Anm_SetPendingCopy(4, (GFX_RECT) {}, (GFX_RECT) {}));
    CHECK_EQ_INT(g_GameManager.player.extends, 2);
    GuiSetup_Run(1);
    CHECK(g_Gui.stage_clear.state == GUI_STAGE_CLEAR_SHOWN);
    CHECK(Gui_IsLoadingShown());
    GuiSetup_TearDown();
}

TEST(the_last_stage_clear_ends_the_extends)
{
    M_SetUpStageClear("last_clear", 6);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GameManager.player.extends, -1);
    CHECK(g_Gui.loading.icon.script == nullptr);
    CHECK(Anm_SetPendingCopy(4, (GFX_RECT) {}, (GFX_RECT) {}));
    GuiSetup_TearDown();
}

static void M_SetUpNextScene(
    const char *const case_name, const int32_t stage, const int32_t difficulty)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_NEXT_SCENE);
    GuiSetup_SetUp(case_name, stage, &msg);
    g_GameManager.run.difficulty = difficulty;
    g_GameManager.run.shot_index = 3;
    g_GameManager.score.value = 12345;
    GuiSetup_StartMsg(0);
}

TEST(the_scene_instruction_goes_on_to_the_next_stage)
{
    M_SetUpNextScene("next_stage", 3, 1);
    CHECK(Result_Log(
        Anm_Load(M_CAPTION_SLOT, "data/caption.anm", 0x10c), LOG_LEVEL_ERROR));
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GameManager.score.shown, 12345);
    CHECK_EQ_INT(g_Gui.msg.script, GUI_MSG_TO_NEXT_STAGE);
    CHECK(Gui_IsMsgRunning());
    CHECK(!Gui_MsgHoldsTimeline());
    CHECK_EQ_INT(g_Gui.loading.caption.sprite, 0x10c);
    CHECK_EQ_INT(g_GuiCalls.scene_requests, 0);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.scene_requests, 1);
    CHECK(g_GuiCalls.requested == SCENE_NEXT_STAGE);
    CHECK(!g_Gui.next_stage_requested);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.scene_requests, 1);
    CHECK(Gui_IsMsgRunning());
    GuiSetup_TearDown();
}

TEST(the_scene_instruction_picks_the_scene_by_mode)
{
    static const struct {
        int32_t stage, difficulty;
        bool practice, replay, has_next;
        SCENE scene;
        bool cleared;
    } cases[] = {
        { 3, 1, true, false, false, SCENE_GAME_RESULT, false },
        { 3, 1, false, true, false, SCENE_REPLAY_END, false },
        { 6, 1, false, false, false, SCENE_ENDING, true },
        { 6, 1, false, true, false, SCENE_REPLAY_END, false },
        { 7, 4, false, false, false, SCENE_GAME_RESULT, true },
        { 8, 5, false, false, false, SCENE_GAME_RESULT, true },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        M_SetUpNextScene("by_mode", cases[i].stage, cases[i].difficulty);
        g_GameManager.mode.in_practice = cases[i].practice;
        g_GameManager.mode.in_replay = cases[i].replay;
        GuiSetup_Run(1);
        CHECK(g_GuiCalls.requested == cases[i].scene);
        CHECK(g_GameManager.mode.game_cleared == cases[i].cleared);
        CHECK_EQ_INT(g_GameManager.score.shown, 12345);
        CHECK(!g_Gui.next_stage_requested);
        // The script holds at the instruction.
        CHECK_EQ_INT(g_Gui.msg.script, 0);
        GuiSetup_TearDown();
    }
}

TEST(a_replay_with_the_next_stage_goes_on)
{
    M_SetUpNextScene("replay_next", 3, 1);
    g_GameManager.mode.in_replay = true;
    g_GuiFake.replay_stages[3] = true;
    GuiSetup_Run(1);
    CHECK(g_GuiCalls.requested == SCENE_REPLAY_END);
    GuiSetup_TearDown();
    M_SetUpNextScene("replay_next", 3, 1);
    g_GameManager.mode.in_replay = true;
    g_GuiFake.replay_stages[4] = true;
    GuiSetup_Run(1);
    CHECK(g_Gui.next_stage_requested);
    CHECK_EQ_INT(g_GuiCalls.scene_requests, 0);
    GuiSetup_TearDown();
}

TEST(clearing_extra_and_phantasm_counts_the_clear)
{
    M_SetUpNextScene("extra", 7, 4);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[4], 99);
    CHECK_EQ_INT(g_GameManager.records.play_stats.counts[4].clears, 1);
    GuiSetup_TearDown();
    M_SetUpNextScene("phantasm", 8, 5);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[4], 0);
    CHECK_EQ_INT(g_GameManager.records.play_stats.counts[5].clears, 1);
    GuiSetup_TearDown();
    M_SetUpNextScene("ending", 6, 3);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GameManager.records.play_stats.counts[3].clears, 0);
    GuiSetup_TearDown();
}

TEST(only_a_phantasm_replay_is_saved_again)
{
    M_SetUpNextScene("resave", 8, 5);
    g_GameManager.mode.in_replay = true;
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.resaves, 1);
    CHECK(g_GuiCalls.requested == SCENE_REPLAY_END);
    GuiSetup_TearDown();
    M_SetUpNextScene("no_resave", 7, 4);
    g_GameManager.mode.in_replay = true;
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.resaves, 0);
    CHECK_EQ_INT(g_GameManager.records.play_stats.counts[4].clears, 0);
    GuiSetup_TearDown();
}

TEST(an_end_stops_the_dialogue)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 2, GUI_MSG_OP_END);
    GuiSetup_SetUp("end", 1, &msg);
    GuiSetup_StartMsg(0);
    GuiSetup_Run(3);
    CHECK(!Gui_IsMsgRunning());
    CHECK(!Gui_MsgHoldsTimeline());
    const int32_t collected = g_GuiCalls.items_collected;
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GuiCalls.items_collected, collected);
    GuiSetup_TearDown();
}
