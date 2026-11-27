#include "tests/test.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/game_over.c"
#include "th02/input.h"

#include <string.h>

typedef struct {
    TH02_SESSION session;
} FIXTURE;

static void M_Begin(FIXTURE *const f)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    Tram_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(f, 0, sizeof(*f));
    f->session.rank = 1;
    m_Palette.tone = 100;
    g_VSync.count2 = 65520;
    GameOver_Begin(&f->session);
}

static GAME_OVER_EVENT M_Frame(const uint16_t keys)
{
    VSyncIrq_VSync();
    return GameOver_Update(keys);
}

static void M_Register(void)
{
    for (size_t frame = 0; frame < 124; frame++) {
        CHECK_EQ_INT(M_Frame(0), GAME_OVER_WAIT);
    }
    CHECK_EQ_INT(m_GameOver.phase, GAME_OVER_KEY);
    CHECK_EQ_INT(GameOver_Update(GAME_KEY_BOMB), GAME_OVER_NAME);
}

TEST(overlays_and_animation_wait_before_accepting_held_input)
{
    FIXTURE f;
    M_Begin(&f);
    CHECK_EQ_INT(m_Palette.tone, 100);
    for (size_t frame = 0; frame < 124; frame++) {
        CHECK_EQ_INT(GameOver_Update(GAME_KEY_SHOT), GAME_OVER_WAIT);
        const GAME_OVER saved = m_GameOver;
        CHECK_EQ_INT(GameOver_Update(GAME_KEY_SHOT), GAME_OVER_WAIT);
        CHECK(memcmp(&saved, &m_GameOver, sizeof(saved)) == 0);
        const GAME_OVER_EVENT event = M_Frame(GAME_KEY_SHOT);
        CHECK_EQ_INT(m_Palette.tone, frame < 41 ? 100 : 50);
        CHECK_EQ_INT(event, frame == 123 ? GAME_OVER_NAME : GAME_OVER_WAIT);
    }
    CHECK_EQ_INT(g_VSync.count2, 108);
    CHECK_EQ_INT(GameOver_Update(0), GAME_OVER_NAME);
    GameOver_Resume();
    CHECK_EQ_INT(m_GameOver.phase, GAME_OVER_CHOICE);
    CHECK(m_GameOver.yes);
    CHECK_EQ_INT(m_Tram.attributes[14 * 80 + 24], 0xe5);
    CHECK_EQ_INT(GameOver_Update(GAME_KEY_SHOT), GAME_OVER_WAIT);
    CHECK_EQ_INT(M_Frame(GAME_KEY_OK), GAME_OVER_WAIT);
    CHECK_EQ_INT(M_Frame(0), GAME_OVER_WAIT);
    CHECK_EQ_INT(M_Frame(GAME_KEY_OK), GAME_OVER_RESUME);
    CHECK_EQ_INT(GameOver_Update(GAME_KEY_CANCEL), GAME_OVER_RESUME);
    CHECK_EQ_INT(f.session.continues_used, 0);
}

TEST(down_wins_over_up_and_cancel_wins_over_confirmation)
{
    for (size_t cancel = 0; cancel < 2; cancel++) {
        FIXTURE f;
        M_Begin(&f);
        M_Register();
        GameOver_Resume();
        CHECK_EQ_INT(GameOver_Update(0), GAME_OVER_WAIT);
        CHECK_EQ_INT(M_Frame(GAME_KEY_UP | GAME_KEY_DOWN), GAME_OVER_WAIT);
        CHECK(!m_GameOver.yes);
        CHECK_EQ_INT(m_Tram.attributes[15 * 80 + 25], 0xe5);
        const uint16_t keys = cancel != 0
            ? GAME_KEY_UP | GAME_KEY_SHOT | GAME_KEY_CANCEL
            : GAME_KEY_OK;
        CHECK_EQ_INT(M_Frame(keys), GAME_OVER_EXIT);
    }
}

TEST(extra_and_three_used_credits_skip_the_prompt)
{
    for (size_t extra = 0; extra < 2; extra++) {
        FIXTURE f;
        M_Begin(&f);
        f.session.rank = extra != 0 ? SESSION_EXTRA_RANK : 1;
        f.session.continues_used = extra != 0 ? 0 : 3;
        M_Register();
        GameOver_Resume();
        CHECK_EQ_INT(GameOver_Update(GAME_KEY_SHOT), GAME_OVER_EXIT);
        CHECK_EQ_INT(m_Palette.tone, 50);
    }
}
