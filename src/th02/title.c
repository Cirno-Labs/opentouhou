#include "th02/title_private.h"

#include <string.h>

static TITLE m_Title;
TITLE *const p_Title = &m_Title;
static TITLE_FLASH m_Flash;
TITLE_FLASH *const p_TitleFlash = &m_Flash;

static void M_Audio(
    TITLE_EVENTS *const events, const TITLE_AUDIO_ACTION action,
    const bool midi, const uint8_t effect)
{
    events->audio[events->audio_count++] =
        (TITLE_AUDIO_REQUEST) { action, midi, effect };
}

static void M_Image(const TITLE_IMAGE *const image, const uint8_t page)
{
    Video_AccessPage(page);
    Palette_Import(image->palette);
    Palette_Show();
    Video_PutImage8(
        g_Clip, image->view.pixels, image->view.width, image->view.height, 0,
        0);
}

static void M_Door(void)
{
    const uint8_t step = m_Title.door_step++;
    if (step < 40) {
        const TRAM_CHARACTER space = { .code = ' ' };
        for (uint16_t y = 0; y < TRAM_ROWS; y++) {
            Tram_PutCharacter(39 - step, y, &space, TRAM_WHITE);
            Tram_PutCharacter(40 + step, y, &space, TRAM_WHITE);
        }
    }
    Palette_SetTone(200 - step * 2);
}

static void M_Credits(void)
{
    static const uint8_t zun[] = { 0xc3, 0xbe, 0xb6, 0 };
    const GAIJI *const glyphs = m_Title.assets->glyphs;
    Palette_SetTone(200);
    Video_AccessPage(1);
    Draw_Gaiji(glyphs, 548, 384, 0xdf, 0, false);
    Draw_GaijiString(glyphs, 564, 384, 16, zun, 0);
    Draw_Gaiji(glyphs, 544, 380, 0xdf, 6, false);
    Draw_GaijiString(glyphs, 560, 380, 16, zun, 6);
    Video_CopyPage(0);
}

TITLE_FLASH_EVENTS TitleFlash_Begin(
    const TITLE_FLASH_IMAGE images[TITLE_FLASH_IMAGE_COUNT])
{
    memset(&m_Flash, 0, sizeof(m_Flash));

    m_Flash.images = images;
    m_Flash.active = true;
    return (TITLE_FLASH_EVENTS) { .force_effect = SOUND_EFFECT_PILLAR };
}

TITLE_FLASH_EVENTS TitleFlash_Advance(void)
{
    TITLE_FLASH_EVENTS events = { .force_effect = SOUND_EFFECT_NONE };
    if (!m_Flash.active) {
        events.complete = true;
        return events;
    }
    if (m_Flash.frame == 18) {
        Video_ShowPage(0);
        m_Flash.active = false;
        m_Flash.images = nullptr;
        events.release_images = events.complete = true;
        return events;
    }
    const uint8_t frame = m_Flash.frame++;
    Video_ShowPage(1 - frame % 2);
    if (frame == 0 || frame == 5 || frame == 10) {
        const TITLE_FLASH_IMAGE *const image = &m_Flash.images[frame / 5];
        Video_PutImage8(
            g_Clip, image->pixels, image->width, image->height, 0, 0);
    }
    if (frame % 3 < 2) {
        Palette_SetTone(frame % 3 == 0 ? 150 : 100);
    }
    return events;
}

void Title_Init(void)
{
    memset(&m_Title, 0, sizeof(m_Title));
    memset(&m_Flash, 0, sizeof(m_Flash));
}

void Title_Begin(
    const TITLE_ASSETS *const assets, const uint8_t demo_num,
    const bool midi_possible, const bool midi_active)
{
    Title_Init();
    m_Title.assets = assets;
    m_Title.demo_num = demo_num;
    m_Title.midi_possible = midi_possible;
    m_Title.midi_active = midi_active;
}

TITLE_EVENTS Title_Step(void)
{
    TITLE_EVENTS events = { .demo_num = m_Title.demo_num };
    if (m_Title.phase == TITLE_DONE) {
        events.wait = TITLE_COMPLETE;
        return events;
    }
    switch (m_Title.phase) {
    case TITLE_PREPARE:
        Tram_Fill(' ', TRAM_BLACK | TRAM_REVERSE);
        M_Audio(&events, TITLE_LOAD_EFFECTS, false, 0);
        m_Title.phase = TITLE_MENU_BACKGROUND;
        break;
    case TITLE_MENU_BACKGROUND:
        M_Image(&m_Title.assets->menu_background, 1);
        events.release_images = 1;
        m_Title.phase = TITLE_BACKGROUND;
        break;
    case TITLE_BACKGROUND:
        M_Image(&m_Title.assets->title_background, 0);
        events.release_images = 2;
        m_Title.phase = TITLE_DOOR;
        Palette_SetTone(200);
        break;
    case TITLE_DOOR:
        if (m_Title.door_step < 50) {
            M_Door();
            events.wait = TITLE_WAIT_FRAMES;
            events.count = 1;
            break;
        }
        Palette_SetTone(100);
        if (m_Title.demo_num == 0) {
            bool midi = false;
            if (m_Title.midi_possible) {
                midi = m_Title.midi_active;
                m_Title.midi_active = true;
                M_Audio(&events, TITLE_LOAD_MENU_SONG, true, 0);
            }
            m_Title.midi_active = false;
            M_Audio(&events, TITLE_LOAD_MENU_SONG, false, 0);
            m_Title.midi_active = midi;
        }
        events.wait = TITLE_WAIT_FRAMES;
        events.count = 18;
        m_Title.phase = TITLE_HOLD;
        break;
    case TITLE_HOLD: {
        const TITLE_FLASH_EVENTS flash =
            TitleFlash_Begin(m_Title.assets->flash);
        M_Audio(&events, TITLE_FORCE_EFFECT, false, flash.force_effect);
        m_Title.phase = TITLE_FLASHING;
        break;
    }
    case TITLE_FLASHING: {
        const TITLE_FLASH_EVENTS flash = TitleFlash_Advance();
        if (flash.complete) {
            events.release_images = flash.release_images ? 0x1c : 0;
            m_Title.phase = TITLE_CREDITS;
        } else {
            events.wait = TITLE_WAIT_FRAMES;
            events.count = 1;
        }
        break;
    }
    case TITLE_CREDITS:
        M_Credits();
        if (m_Title.demo_num == 0) {
            M_Audio(&events, TITLE_PLAY_SONG, m_Title.midi_active, 0);
        }
        m_Title.demo_num = 0;
        m_Title.phase = TITLE_FADE_BEGIN;
        break;
    case TITLE_FADE_BEGIN:
        Palette_Import(m_Title.assets->menu_palette);
        Palette_Show();
        Palette_BeginFade(&m_Title.fade, PALETTE_WHITE_IN, 6);
        events.wait = TITLE_WAIT_VSYNC;
        events.count = 1;
        m_Title.phase = TITLE_FADE;
        break;
    case TITLE_FADE:
        if (Palette_FadeVSync(&m_Title.fade)) {
            events.wait = TITLE_COMPLETE;
            m_Title.phase = TITLE_DONE;
        } else {
            events.wait = TITLE_WAIT_VSYNC;
            events.count = 1;
        }
        break;
    case TITLE_DONE:
        events.wait = TITLE_COMPLETE;
        break;
    }
    events.demo_num = m_Title.demo_num;
    return events;
}
