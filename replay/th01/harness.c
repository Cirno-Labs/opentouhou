#include "replay/th01/harness.h"

#include "replay/th01/files.h"

#include "core/base/utils.h"

// Holds the default score table: ten names 東方★靈異伝　　 in CP932, each
// byte plus 9Ch, scores 1000 down to 100, stages 10 down to 1 and the
// route 無.
#define M_NAME                                                                 \
    "\x2f\x28\x31\x97\x1d\x36\x84\x67\x24\x75\x2f\xfc\x1d\xdc\x1d\xdc"
#define M_NAMES M_NAME M_NAME M_NAME M_NAME M_NAME
#define M_ROUTES "\x96\xb3\x96\xb3\x96\xb3\x96\xb3\x96\xb3"
static const char m_Scores[] =
    "HISCORE" M_NAMES M_NAMES
    "\xe8\x03\x00\x00\x84\x03\x00\x00\x20\x03\x00\x00\xbc\x02\x00\x00"
    "\x58\x02\x00\x00\xf4\x01\x00\x00\x90\x01\x00\x00\x2c\x01\x00\x00"
    "\xc8\x00\x00\x00\x64\x00\x00\x00"
    "\x0a\x00\x09\x00\x08\x00\x07\x00\x06\x00\x05\x00\x04\x00\x03\x00"
    "\x02\x00\x01\x00" M_ROUTES M_ROUTES;
static_assert(sizeof(m_Scores) - 1 == 247);
#undef M_NAME
#undef M_NAMES
#undef M_ROUTES

// Holds the difficulty, music mode, bombs and extra lives after the ID.
static const char m_Config[] = "REIIDEN\x01\x01\x01\x02";
static_assert(sizeof(m_Config) - 1 == 11);

static const BINDING m_Bindings[] = {
#define M_BINDING(name, mask) { #name, mask },
    TH01_BUTTONS(M_BINDING)
#undef M_BINDING
};

#define M_COLUMN(name, kind) { #name, COLUMN_##kind },
static const COLUMN m_Columns[] = { TH01_CALC_FIELDS(M_COLUMN) };
static const COLUMN m_Choices[] = { TH01_CHOICE_FIELDS(M_COLUMN) };
static const COLUMN m_Starts[] = { TH01_START_FIELDS(M_COLUMN) };
#undef M_COLUMN

static_assert(
    ARRAY_SIZE(m_Columns) + ARRAY_SIZE(m_Choices) == TH01_COLUMN_COUNT);

const HARNESS_GAME g_Th01Harness = {
    .id = "th01",
    .version = "1.10",
    .frame_rate = 60.0,
    .width = 640,
    .height = 400,
    .draw_frames = 1,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .columns = m_Columns,
    .column_count = ARRAY_SIZE(m_Columns),
    .choices = m_Choices,
    .choice_count = ARRAY_SIZE(m_Choices),
    .columns_version = 2,
    .starts = m_Starts,
    .start_count = ARRAY_SIZE(m_Starts),
};

#define M_FILE(path, data) { path, (const uint8_t *)data, sizeof(data) - 1 }
const STAGED_FILE g_Th01Files[5] = {
    M_FILE("reiiden.cfg", m_Config), M_FILE("reyhies.dat", m_Scores),
    M_FILE("reyhino.dat", m_Scores), M_FILE("reyhiha.dat", m_Scores),
    M_FILE("reyhilu.dat", m_Scores),
};
#undef M_FILE
