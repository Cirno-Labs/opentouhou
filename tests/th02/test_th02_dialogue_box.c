#include "th02/dialogue.h"

#include "test.h"

typedef struct {
    uint16_t bytes[128];
    uint8_t lines[128];
    uint8_t boxes[128];
    int16_t faces[128];
    uint16_t face_count;
    uint16_t text_count;
    uint16_t wipes;
    char order[256];
    uint16_t order_count;
} REQUESTS;

static void M_Wipe(void *const context)
{
    REQUESTS *const requests = context;
    requests->wipes++;
    requests->order[requests->order_count++] = 'w';
}

static void M_Face(void *const context, const int16_t face)
{
    REQUESTS *const requests = context;
    requests->faces[requests->face_count++] = face;
    requests->order[requests->order_count++] = 'f';
}

static void M_Text(
    void *const context, const uint8_t box, const uint8_t line,
    const uint16_t bytes)
{
    REQUESTS *const requests = context;
    requests->boxes[requests->text_count] = box;
    requests->lines[requests->text_count] = line;
    requests->bytes[requests->text_count++] = bytes;
    requests->order[requests->order_count++] = 't';
}

static void M_Reveal(
    DIALOGUE *const dialogue, const TH02_INPUT *const sample,
    uint16_t *const frames)
{
    DIALOGUE_RESULT result = DialogueBox_Update(dialogue, nullptr);
    while (dialogue->phase != DIALOGUE_CONFIRM_INPUT) {
        if (result == DIALOGUE_FRAME) {
            (*frames)++;
        }
        result = DialogueBox_Update(dialogue, sample);
    }
    CHECK_EQ_INT(result, DIALOGUE_INPUT);
}

static void M_Trace(const REQUESTS *const requests, const uint16_t repeats)
{
    CHECK_EQ_INT(requests->wipes, 1);
    CHECK_EQ_INT(requests->face_count, 38 * repeats);
    CHECK_EQ_INT(requests->text_count, 34 * repeats);
    CHECK_EQ_INT(requests->order_count, 1 + 72 * repeats);
    CHECK_EQ_INT(requests->order[0], 'w');
    size_t text = 0;
    size_t order = 1;
    for (uint16_t cursor = 6; cursor <= 80; cursor += 2) {
        for (uint16_t repeat = 0; repeat < repeats; repeat++) {
            CHECK_EQ_INT(requests->order[order++], 'f');
            if (cursor <= 72) {
                CHECK_EQ_INT(requests->order[order++], 't');
                CHECK_EQ_INT(requests->boxes[text], 9);
                CHECK_EQ_INT(requests->lines[text], cursor <= 36 ? 0 : 1);
                CHECK_EQ_INT(
                    requests->bytes[text++],
                    cursor <= 36 ? cursor : cursor - 36);
            }
        }
    }
}

TEST(unheld_reveal_waits_three_frames_per_pair_and_keeps_four_face_only_steps)
{
    DIALOGUE_RUNTIME runtime = { .keys = 0xabcd, .box = 9 };
    REQUESTS requests = {};
    const DIALOGUE_VIEW view = { M_Wipe, M_Face, M_Text, &requests };
    DIALOGUE dialogue;
    DialogueBox_Begin(&dialogue, &runtime, &view, 255);
    CHECK_EQ_INT(runtime.keys, 0xabcd);
    CHECK_EQ_INT(runtime.box, 9);
    CHECK_EQ_INT(dialogue.cursor, 6);
    CHECK_EQ_INT(dialogue.iterations, 0);
    CHECK_EQ_INT(requests.wipes, 1);
    const TH02_INPUT input = {};
    uint16_t frames = 0;
    M_Reveal(&dialogue, &input, &frames);
    M_Trace(&requests, 3);
    CHECK_EQ_INT(frames, 114);
    CHECK_EQ_INT(dialogue.iterations, 114);
    CHECK_EQ_INT(dialogue.cursor, 82);
    CHECK_EQ_INT(runtime.keys, 0);
    CHECK_EQ_INT(runtime.box, 9);
    for (size_t i = 0; i < requests.face_count; i++) {
        CHECK_EQ_INT(requests.faces[i], 255);
    }
}

TEST(space_accelerates_but_confirmation_ignores_space_and_movement)
{
    DIALOGUE_RUNTIME runtime = { .box = 9 };
    REQUESTS requests = {};
    const DIALOGUE_VIEW view = { M_Wipe, M_Face, M_Text, &requests };
    DIALOGUE dialogue;
    DialogueBox_Begin(&dialogue, &runtime, &view, 57);
    const TH02_INPUT input = { .space = true };
    uint16_t frames = 0;
    M_Reveal(&dialogue, &input, &frames);
    M_Trace(&requests, 1);
    CHECK_EQ_INT(frames, 0);
    CHECK_EQ_INT(runtime.keys, GAME_KEY_SHOT);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &input), DIALOGUE_FRAME);
    CHECK_EQ_INT(dialogue.wait.phase, KEY_WAIT_RELEASE);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &input), DIALOGUE_FRAME);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &input), DIALOGUE_FRAME);
    CHECK_EQ_INT(dialogue.wait.phase, KEY_WAIT_PRESS);
    const TH02_INPUT movement = { .up = true, .q = true };
    for (size_t i = 0; i < 6; i++) {
        CHECK_EQ_INT(DialogueBox_Update(&dialogue, &movement), DIALOGUE_FRAME);
    }
    CHECK_EQ_INT(runtime.box, 9);
    const TH02_INPUT shot = { .shot = true };
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &shot), DIALOGUE_FRAME);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &shot), DIALOGUE_DONE);
    CHECK_EQ_INT(runtime.box, 10);
    CHECK_EQ_INT(runtime.keys, GAME_KEY_SHOT);
    const DIALOGUE completed = dialogue;
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, nullptr), DIALOGUE_DONE);
    CHECK(memcmp(&completed, &dialogue, sizeof(dialogue)) == 0);
    M_Trace(&requests, 1);
}

TEST(speed_changes_keep_loop_count_and_frame_resume_does_not_sample_keys)
{
    DIALOGUE_RUNTIME runtime = {};
    DIALOGUE dialogue;
    DialogueBox_Begin(&dialogue, &runtime, nullptr, 0);
    const TH02_INPUT idle = {};
    const TH02_INPUT held = { .right = true };
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &held), DIALOGUE_INPUT);
    CHECK_EQ_INT(runtime.keys, 0);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &idle), DIALOGUE_FRAME);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &held), DIALOGUE_INPUT);
    CHECK_EQ_INT(runtime.keys, 0);
    CHECK_EQ_INT(dialogue.cursor, 6);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &held), DIALOGUE_INPUT);
    CHECK_EQ_INT(dialogue.cursor, 8);
    CHECK_EQ_INT(dialogue.iterations, 2);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &idle), DIALOGUE_FRAME);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, nullptr), DIALOGUE_INPUT);
    CHECK_EQ_INT(dialogue.cursor, 10);
    CHECK_EQ_INT(dialogue.iterations, 3);
}

TEST(confirmation_releases_held_key_and_keeps_byte_wrap_and_last_reveal_mask)
{
    DIALOGUE_RUNTIME runtime = { .box = 255 };
    DIALOGUE dialogue;
    DialogueBox_Begin(&dialogue, &runtime, nullptr, 0);
    const TH02_INPUT held = { .cancel = true };
    uint16_t frames = 0;
    M_Reveal(&dialogue, &held, &frames);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &held), DIALOGUE_FRAME);
    const TH02_INPUT idle = {};
    for (size_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(DialogueBox_Update(&dialogue, &idle), DIALOGUE_FRAME);
    }
    CHECK_EQ_INT(dialogue.wait.phase, KEY_WAIT_RELEASE);
    for (size_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(DialogueBox_Update(&dialogue, &idle), DIALOGUE_FRAME);
    }
    CHECK_EQ_INT(dialogue.wait.phase, KEY_WAIT_PRESS);
    const TH02_INPUT extra = { .extra_group_3 = true };
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &extra), DIALOGUE_FRAME);
    CHECK_EQ_INT(DialogueBox_Update(&dialogue, &extra), DIALOGUE_DONE);
    CHECK_EQ_INT(runtime.box, 0);
    CHECK_EQ_INT(runtime.keys, GAME_KEY_CANCEL);
}
