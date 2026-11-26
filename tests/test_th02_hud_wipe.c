#include "th02_tram_fixture.h"
#include "th02/hud.h"

#include "test.h"

TEST(wipe_draws_all_cels_with_two_count_waits_and_preserves_border)
{
    for (unsigned leave = 0; leave < 2; leave++) {
        Tram_Init();
        for (unsigned i = 0; i < TRAM_PLANE_CELLS; i++) {
            m_Tram.codes[i] = 0x1234;
            m_Tram.attributes[i] = 0xabcd;
        }
        memset(&g_VSync, 0, sizeof(g_VSync));
        g_VSync.count1 = 91;
        g_VSync.count2 = 37;
        g_VSync.edges = 105;
        HUD_WIPE overlay;
        Hud_BeginWipe(&overlay, leave != 0);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, 37);
        CHECK_EQ_INT(g_VSync.edges, 105);
        for (unsigned frame = 0; frame < 21; frame++) {
            TRAM expected;
            for (unsigned i = 0; i < TRAM_PLANE_CELLS; i++) {
                expected.codes[i] = 0x1234;
                expected.attributes[i] = 0xabcd;
            }
            for (int x = 0; x < 24; x++) {
                const int step = (int)frame - x / 2;
                const unsigned cel = step < 0 ? 0 : step > 8 ? 8 : step;
                for (unsigned y = 1; y <= 23; y++) {
                    const uint16_t glyph = (leave != 0 ? 0xcf : 0xf1) + cel;
                    const uint16_t code = ((glyph - 128) << 8) | 0x57;
                    const size_t cell = y * TRAM_COLUMNS + 4 + x * 2;
                    expected.codes[cell] = code;
                    expected.codes[cell + 1] = code | 0x8000;
                    expected.attributes[cell] = expected.attributes[cell + 1] =
                        1;
                }
            }
            CHECK(memcmp(&m_Tram, &expected, sizeof(m_Tram)) == 0);
            for (unsigned i = 0; i < 4; i++) {
                CHECK(!Hud_UpdateWipe(&overlay));
            }
            VSyncIrq_VSync();
            CHECK(!Hud_UpdateWipe(&overlay));
            CHECK(memcmp(&m_Tram, &expected, sizeof(m_Tram)) == 0);
            VSyncIrq_VSync();
            CHECK_EQ_INT(Hud_UpdateWipe(&overlay), frame == 20);
        }
        CHECK_EQ_INT(g_VSync.count2, 79);
        CHECK_EQ_INT(g_VSync.edges, 147);
        CHECK_EQ_INT(g_VSync.count1, 2);
        const TRAM final = m_Tram;
        CHECK(Hud_UpdateWipe(&overlay));
        CHECK(memcmp(&m_Tram, &final, sizeof(m_Tram)) == 0);
        CHECK_EQ_INT(overlay.frame, 21);
    }
}
