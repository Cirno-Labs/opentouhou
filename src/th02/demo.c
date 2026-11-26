#include "th02/demo.h"

#include "core/base/assert.h"

#include <string.h>

RESULT Demo_Read(READER *const reader, DEMO_RECORD *const out)
{
    memset(out, 0, sizeof(*out));
    MUST(Reader_Result(reader));
    FAIL_IF(
        Reader_Remaining(reader) != DEMO_INPUT_COUNT * 2,
        "Invalid demo recording size");
    for (size_t i = 0; i < DEMO_INPUT_COUNT; i++) {
        out->keys[i] = Reader_ReadU16(reader);
    }
    const RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        memset(out, 0, sizeof(*out));
    }
    return result;
}

RESULT Demo_Load(VFS *const files, DEMO_RECORD out[3])
{
    static const char *const paths[] = { "demo1.rec", "demo2.rec",
                                         "demo3.rec" };
    memset(out, 0, sizeof(*out) * 3);
    RESULT result = OK;
    for (size_t i = 0; i < ARRAY_SIZE(paths); i++) {
        READER reader;
        result = Vfs_Open(files, paths[i], VFS_IN_MEMORY, &reader);
        if (!IS_OK(result)) {
            break;
        }
        result = Demo_Read(&reader, &out[i]);
        Reader_Close(&reader);
        if (!IS_OK(result)) {
            break;
        }
    }
    if (!IS_OK(result)) {
        memset(out, 0, sizeof(*out) * 3);
    }
    return result;
}

void Demo_ConfigureSession(TH02_SESSION *const session)
{
    static const uint8_t stages[] = { 3, 2, 1 };
    static const uint8_t shots[] = { 0, 2, 1 };
    ASSERT(session->demo_num >= 1 && session->demo_num <= 3);
    session->stage = stages[session->demo_num - 1];
    session->shot_type = shots[session->demo_num - 1];
    session->frame = 18;
}

void Demo_Begin(DEMO *const demo, const DEMO_RECORD *const record)
{
    memset(demo, 0, sizeof(*demo));
    demo->record = record;
}

bool Demo_Input(DEMO *const demo, uint16_t *const keys)
{
    if (!demo->ending) {
        ASSERT(demo->frame < DEMO_PLAY_COUNT);
        if (*keys == 0) {
            *keys = demo->record->keys[demo->frame++];
            if (demo->frame < DEMO_PLAY_COUNT) {
                return true;
            }
        }
        demo->ending = true;
    }
    *keys = 0;
    return false;
}
