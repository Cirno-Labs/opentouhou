#include "tests/test.h"
#include "tests/test_dir.h"

#include "shell/game.h"
#include "shell/game/entry.h"

typedef struct {
    bool unused;
    uint64_t frame_us;
    uint64_t origin_us;
} INPUT;

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(INPUT, unused, "z", nullptr, nullptr),
};
static const INPUT_CLOCK_FIELD m_Clocks[] = {
    INPUT_CLOCK(INPUT, frame_us, INPUT_CLOCK_FRAME),
    INPUT_CLOCK(INPUT, origin_us, INPUT_CLOCK_HOST_ORIGIN),
};
static const INPUT_SCHEMA m_InputSchema =
    INPUT_SCHEMA_WITH_CLOCKS_OF(INPUT, m_Actions, m_Clocks, 1);
static int m_Ticks;
static uint64_t m_OriginSeen[2];
static uint64_t m_FrameSeen[2];

static RESULT M_Init(void)
{
    return OK;
}

static bool M_Tick(const void *const input_)
{
    const INPUT *const input = input_;
    m_OriginSeen[m_Ticks] = input->origin_us;
    m_FrameSeen[m_Ticks] = input->frame_us;
    m_Ticks++;
    return m_Ticks < 2;
}

static void M_Shutdown(void)
{
}

static const GAME_DESC m_Desc = {
    .id = "test",
    .title = "Clock fields test",
    .version = "0",
    .logical_width = 16,
    .logical_height = 16,
    .input_schema = &m_InputSchema,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};

TEST(shell_fills_clock_fields_by_role_each_frame)
{
    char *const dir = TestDir_Prepare("clock_fields");
    char *argv[] = { "test", "--data", dir, "--user-dir", dir, nullptr };
    CHECK_EQ_INT(Shell_Init(&m_Desc, 5, argv), SHELL_CONTINUE);
    SHELL_STATUS status = SHELL_CONTINUE;
    for (int frame = 0; frame < 100 && status == SHELL_CONTINUE; frame++) {
        status = Shell_Iterate();
        SDL_Delay(2);
    }
    CHECK_EQ_INT(status, SHELL_SUCCESS);
    CHECK_EQ_INT(m_Ticks, 2);
    CHECK(m_FrameSeen[1] >= m_FrameSeen[0]);
    CHECK_EQ_INT(m_OriginSeen[0], m_OriginSeen[1]);
    Shell_Quit();
    Mem_Free(dir);
}
