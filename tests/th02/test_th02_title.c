#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/title.c"

#include "core/base/arena.h"

#include <string.h>

TEST(flash_emits_audio_before_drawing_and_finishes_after_eighteen_waits)
{
    for (uint8_t drawing_page = 0; drawing_page < 2; drawing_page++) {
        ARENA arena;
        Arena_Init(&arena, "Title flash test", 1024 * 1024);
        Video_Init(Arena_Allocator(&arena));
        memset(g_Video.pages[0], 2, VIDEO_PAGE_SIZE);
        memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
        Video_AccessPage(drawing_page);
        Video_ShowPage(drawing_page ^ 1);
        Video_SetSplit(150, 20, 70);

        Palette_Init();
        TestClip_Set(100, 1, 110, 1);
        uint8_t pixels[3][32];
        TITLE_FLASH_IMAGE images[3];
        for (size_t i = 0; i < 3; i++) {
            memset(pixels[i], i + 5, sizeof(pixels[i]));
            images[i] = (TITLE_FLASH_IMAGE) { pixels[i], 16, 2 };
        }
        const VIDEO initial = g_Video;
        TITLE_FLASH_EVENTS events = TitleFlash_Begin(images);
        CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_PILLAR);
        CHECK(!events.release_images && !events.complete);
        CHECK(memcmp(&g_Video, &initial, sizeof(g_Video)) == 0);
        CHECK_EQ_INT(g_Video.pages[drawing_page][640], drawing_page + 2);
        for (size_t frame = 0; frame < 18; frame++) {
            events = TitleFlash_Advance();
            CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_NONE);
            CHECK(!events.release_images && !events.complete);
            CHECK_EQ_INT(g_Video.visible_page, 1 - frame % 2);
            CHECK_EQ_INT(g_Video.drawing_page, drawing_page);
            CHECK_EQ_INT(p_Palette->tone, frame % 3 == 0 ? 150 : 100);
            const uint8_t color = frame < 5 ? 5 : frame < 10 ? 6 : 7;
            for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
                CHECK_EQ_INT(
                    g_Video.pages[drawing_page][i],
                    i >= 640 && i < 656 ? color : drawing_page + 2);
                CHECK_EQ_INT(
                    g_Video.pages[drawing_page ^ 1][i], (drawing_page ^ 1) + 2);
            }
            CHECK_EQ_INT(g_Video.first_lines, 150);
            CHECK_EQ_INT(g_Video.first_address, 20);
            CHECK_EQ_INT(g_Video.second_address, 70);
        }
        CHECK(m_Flash.active);
        events = TitleFlash_Advance();
        CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_NONE);
        CHECK(events.release_images && events.complete);
        CHECK(!m_Flash.active);
        CHECK(m_Flash.images == nullptr);
        CHECK_EQ_INT(g_Video.visible_page, 0);
        CHECK_EQ_INT(p_Palette->tone, 100);
        const VIDEO complete = g_Video;
        for (size_t i = 0; i < 3; i++) {
            events = TitleFlash_Advance();
            CHECK(events.complete && !events.release_images);
            CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_NONE);
            CHECK(memcmp(&g_Video, &complete, sizeof(g_Video)) == 0);
        }
        Arena_Free(&arena);
    }
}

TEST(title_sequence_preserves_demo_branches_and_distinct_waits)
{
    for (uint8_t options = 0; options < 8; options++) {
        ARENA arena;
        Arena_Init(&arena, "Title sequence test", 1024 * 1024);
        Video_Init(Arena_Allocator(&arena));
        memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
        memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);

        memset(&m_Tram, 0x39, sizeof(m_Tram));
        TestClip_Set(0, 0, 639, 399);

        Grcg_Init();

        Palette_Init();
        GAIJI glyphs;
        memset(&glyphs, 0xff, sizeof(glyphs));
        const uint8_t pixels[16] = { 0 };
        TITLE_ASSETS assets = {
            .menu_background.view = { pixels, 16, 1 },
            .title_background.view = { pixels, 16, 1 },
            .flash = { { pixels, 16, 1 },
                       { pixels, 16, 1 },
                       { pixels, 16, 1 } },
            .glyphs = &glyphs,
        };
        for (size_t i = 0; i < 16; i++) {
            assets.menu_palette[i] = (GFX_COLOR) { i * 17, 34, 51, 255 };
        }
        const uint8_t demo_num = (options & 1) != 0 ? 3 : 0;
        const bool midi_possible = (options & 2) != 0;
        const bool initial_midi = (options & 4) != 0;
        const bool cold = demo_num == 0;
        Title_Begin(&assets, demo_num, midi_possible, initial_midi);
        uint32_t frame_waits = 0;
        uint32_t vsync_waits = 0;
        uint8_t releases = 0;
        uint8_t loads = 0;
        uint8_t plays = 0;
        uint8_t forced = 0;
        uint8_t effects = 0;
        size_t door_frames = 0;
        bool complete = false;
        for (size_t calls = 0; calls < 256; calls++) {
            const TITLE_EVENTS events = Title_Step();
            CHECK((releases & events.release_images) == 0);
            releases |= events.release_images;
            for (size_t i = 0; i < events.audio_count; i++) {
                const TITLE_AUDIO_REQUEST request = events.audio[i];
                switch (request.action) {
                case TITLE_LOAD_EFFECTS:
                    effects++;
                    break;
                case TITLE_LOAD_MENU_SONG:
                    CHECK_EQ_INT(
                        request.midi, m_Title.midi_possible && loads == 0);
                    loads++;
                    break;
                case TITLE_FORCE_EFFECT:
                    CHECK_EQ_INT(request.effect, 6);
                    forced++;
                    break;
                case TITLE_STOP_SONG:
                    CHECK(false);
                    break;
                case TITLE_PLAY_SONG:
                    CHECK_EQ_INT(
                        request.midi, initial_midi && m_Title.midi_possible);
                    plays++;
                    break;
                }
            }
            if (events.wait == TITLE_WAIT_FRAMES) {
                frame_waits += events.count;
                if (door_frames < 50) {
                    CHECK_EQ_INT(events.count, 1);
                    CHECK_EQ_INT(p_Palette->tone, 200 - door_frames * 2);
                    for (size_t y = 0; y < 25; y++) {
                        for (size_t x = 0; x < 80; x++) {
                            const bool open = door_frames >= 39
                                || (x >= 39 - door_frames
                                    && x <= 40 + door_frames);
                            CHECK_EQ_INT(m_Tram.codes[y * 80 + x], ' ');
                            CHECK_EQ_INT(
                                m_Tram.attributes[y * 80 + x], open ? 0xe1 : 5);
                        }
                    }
                    door_frames++;
                }
            } else if (events.wait == TITLE_WAIT_VSYNC) {
                CHECK_EQ_INT(events.count, 1);
                vsync_waits += events.count;
            } else if (events.wait == TITLE_COMPLETE) {
                complete = true;
                break;
            }
        }
        CHECK(complete);
        CHECK_EQ_INT(door_frames, 50);
        CHECK_EQ_INT(frame_waits, 86);
        CHECK_EQ_INT(vsync_waits, 103);
        CHECK_EQ_INT(effects, 1);
        CHECK_EQ_INT(forced, 1);
        CHECK_EQ_INT(loads, cold ? m_Title.midi_possible ? 2 : 1 : 0);
        CHECK_EQ_INT(plays, cold ? 1 : 0);
        CHECK_EQ_INT(releases, 31);
        CHECK_EQ_INT(m_Title.demo_num, 0);
        CHECK_EQ_INT(
            m_Title.midi_active,
            cold ? initial_midi && m_Title.midi_possible : initial_midi);
        CHECK_EQ_INT(g_Video.visible_page, 0);
        CHECK_EQ_INT(g_Video.drawing_page, 0);
        CHECK_EQ_INT(p_Palette->tone, 100);
        CHECK_EQ_INT(g_Grcg->mode, 0);
        for (size_t i = 0; i < 16; i++) {
            CHECK_EQ_INT(g_Video.palette[i].red, i * 17);
            CHECK_EQ_INT(g_Video.palette[i].green, 34);
            CHECK_EQ_INT(g_Video.palette[i].blue, 51);
            CHECK_EQ_INT(g_Video.palette[i].alpha, 255);
        }
        CHECK(
            memcmp(g_Video.pages[0], g_Video.pages[1], VIDEO_VISIBLE_SIZE)
            == 0);
        for (size_t i = VIDEO_VISIBLE_SIZE; i < VIDEO_PAGE_SIZE; i++) {
            CHECK_EQ_INT(g_Video.pages[0][i], 6);
            CHECK_EQ_INT(g_Video.pages[1][i], 9);
        }
        for (size_t i = 2000; i < TRAM_PLANE_CELLS; i++) {
            CHECK_EQ_INT(m_Tram.codes[i], 0x3939);
            CHECK_EQ_INT(m_Tram.attributes[i], 0x3939);
        }
        m_Title.assets = nullptr;
        const TITLE_EVENTS done = Title_Step();
        CHECK_EQ_INT(done.wait, TITLE_COMPLETE);
        CHECK_EQ_INT(done.audio_count, 0);
        CHECK_EQ_INT(done.release_images, 0);
        Arena_Free(&arena);
    }
}
