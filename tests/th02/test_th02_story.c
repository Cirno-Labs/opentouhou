#include "th02_palette_fixture.h"
#include "th02_clip_fixture.h"
#include "th02/ending/story.c"

#include "test.h"

typedef struct {
    ENDING_ASSETS assets;
    DRAW_TEXT lines[94];
    DRAW_TEXT_GLYPH glyphs[22];
    TRAM_CHARACTER characters[22];
    uint16_t rows[16];
    STORY_MUSIC songs[2];
    size_t song_count;
} FIXTURE;

static void M_RecordMusic(void *const context, const STORY_MUSIC song)
{
    FIXTURE *const f = context;
    CHECK(f->song_count < 2);
    if (f->song_count < 2) {
        f->songs[f->song_count++] = song;
    }
}

static void M_Init(FIXTURE *const f, const TH02_SESSION *const session)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    Grcg_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(f, 0, sizeof(*f));
    Video_Init(&g_HeapAllocator);
    Clip_Reset();
    Palette_Init();
    VSyncIrq_BeginProgram();
    g_VSync.count1 = 65534;
    g_VSync.count2 = 65530;
    f->rows[0] = 0x8000;
    for (size_t i = 0; i < 22; i++) {
        f->glyphs[i] = (DRAW_TEXT_GLYPH) { f->rows, true };
        f->characters[i].source_end = (uint16_t)((i + 1) * 2);
    }
    for (size_t i = 0; i < 94; i++) {
        f->lines[i].glyphs = f->glyphs;
        f->lines[i].count = 22;
        f->lines[i].cells.characters = f->characters;
    }
    f->assets.story[ENDING_STORY_BAD] = (ENDING_TEXT_SCRIPT) { f->lines, 51 };
    f->assets.story[ENDING_STORY_GOOD] = (ENDING_TEXT_SCRIPT) { f->lines, 94 };
    f->assets.images.allocator = g_HeapAllocator;
    for (size_t i = 0; i < 5; i++) {
        PI_IMAGE *const image = &f->assets.images.images[i];
        image->width = 640;
        image->height = 400;
        image->pixels = Mem_Alloc(VIDEO_VISIBLE_SIZE);
        for (size_t y = 0; y < 400; y++) {
            for (size_t x = 0; x < 640; x++) {
                image->pixels[y * 640 + x] =
                    (uint8_t)(1 + i + x / 320 + y / 200 * 2);
            }
        }
    }
    Story_Begin(&f->assets, session, M_RecordMusic, f);
    CHECK(m_Story.assets == &f->assets);
}

TEST(both_stories_cover_all_shot_branches_with_counter_wrap_and_fast_forward)
{
    static const uint8_t last[2][3] = { { 33, 41, 50 }, { 47, 70, 93 } };
    for (size_t route = 0; route < 2; route++) {
        for (uint8_t shot = 0; shot < 3; shot++) {
            size_t elapsed[2];
            for (size_t fast = 0; fast < 2; fast++) {
                TH02_SESSION session = { .shot_type = shot,
                                         .continues_used = route == 0 ? 2 : 0 };
                const TH02_SESSION before = session;
                FIXTURE f;
                M_Init(&f, &session);
                CHECK_EQ_INT(g_VSync.count1, 65534);
                CHECK_EQ_INT(f.song_count, 0);
                const TH02_INPUT input = { .shot = fast != 0 };
                CHECK(!Story_Update(&input, fast != 0, UINT16_MAX));
                size_t accepted = 0;
                bool done = false;
                for (size_t tick = 0; tick < 15000 && !done; tick++) {
                    VSyncIrq_VSync();
                    accepted++;
                    done = Story_Update(&input, fast != 0, UINT16_MAX);
                    if (!done) {
                        const STORY saved = m_Story;
                        CHECK(!Story_Update(&input, fast != 0, UINT16_MAX));
                        CHECK(memcmp(&saved, &m_Story, sizeof(saved)) == 0);
                    }
                }
                CHECK(done);
                elapsed[fast] = accepted;
                CHECK_EQ_INT(g_VSync.count2, (uint16_t)(65530 + accepted));
                CHECK(memcmp(&session, &before, sizeof(session)) == 0);
                CHECK_EQ_INT(m_Story.line, last[route][shot]);
                CHECK_EQ_INT(m_Story.scroll, route == 0 ? 100 : 0);
                CHECK_EQ_INT(m_Story.left, 36);
                CHECK_EQ_INT(f.song_count, 2);
                CHECK_EQ_INT(f.songs[0], STORY_MUSIC_INTRO);
                CHECK_EQ_INT(f.songs[1], STORY_MUSIC_CREDITS);
                CHECK_EQ_INT(g_Video.drawing_page, 0);
                CHECK_EQ_INT(g_Video.visible_page, 0);
                CHECK_EQ_INT(m_Palette.tone, 100);
                CHECK_EQ_INT(g_Grcg->mode, 0);
                CHECK_EQ_INT(
                    g_Video.pages[0][100 * 640 + 36],
                    route == 0 ? (shot == 0 ? 3 : 3 + shot) : 5 + shot);
                CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 352], 0);
                CHECK(Story_Update(nullptr, false, 0));
                EndingImages_Free(&f.assets.images);
                Mem_Free(g_Video.pages[0]);
            }
            CHECK(elapsed[1] < elapsed[0]);
        }
    }
}
