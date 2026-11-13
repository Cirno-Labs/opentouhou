// SPDX-License-Identifier: GPL-3.0-only

#include "th06/ascii/pause_menu.h"

#include "th06/ascii/text.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"

static void M_SetViewport(void)
{
    VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    viewport->x = (float)(uint32_t)g_GameManager.play_area.x;
    viewport->y = (float)(uint32_t)g_GameManager.play_area.y;
    viewport->width = (float)(uint32_t)g_GameManager.play_area.width;
    viewport->height = (float)(uint32_t)g_GameManager.play_area.height;
}

void PauseMenu_UpdatePauseMenu(PAUSE_MENU *const menu)
{
    if (Buttons_WasPressed(&g_Buttons, 8)) {
        menu->state = 3;
        for (int32_t i = 0; i < 6; i++) {
            if (menu->sprites[i].shown) {
                menu->sprites[i].pending_interrupt = 2;
            }
        }
        menu->state_frames = 0;
        menu->background.pending_interrupt = 1;
    }
    if (Buttons_WasPressed(&g_Buttons, 0x200)) {
        menu->state = 6;
        for (int32_t i = 0; i < 6; i++) {
            if (menu->sprites[i].shown) {
                menu->sprites[i].pending_interrupt = 2;
            }
        }
        menu->state_frames = 0;
    }
    switch (menu->state) {
    case 0:
        for (int32_t i = 0; i < 6; i++) {
            Anm_StartScript(&menu->sprites[i], i + 2);
        }
        for (int32_t i = 0; i < 3; i++) {
            menu->sprites[i].pending_interrupt = 1;
        }
        menu->state++;
        menu->state_frames = 0;
        if (g_AsciiText.capture_background) {
            Anm_SetPendingCopy(3, (GFX_RECT) { 32, 16, 384, 448 });
            Anm_StartScript(&menu->background, 0x718);
            menu->background.pos = (VEC3) { 32.0f, 16.0f, 0.0f };
        }
        [[fallthrough]];
    case 1:
        menu->sprites[1].color = 0xffff8080;
        menu->sprites[2].color = 0x80808080;
        menu->sprites[1].scale_y = 1.7f;
        menu->sprites[1].scale_x = 1.7f;
        menu->sprites[2].scale_y = 1.5f;
        menu->sprites[2].scale_x = 1.5f;
        menu->sprites[1].pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        menu->sprites[2].pos_offset = (VEC3) {};
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(&g_Buttons, 0x10)
                || Buttons_WasPressed(&g_Buttons, 0x20)) {
                menu->state = 2;
            }
            if (Buttons_WasPressed(&g_Buttons, 1)) {
                for (int32_t i = 0; i < 3; i++) {
                    menu->sprites[i].pending_interrupt = 2;
                }
                menu->state = 3;
                menu->state_frames = 0;
                menu->background.pending_interrupt = 1;
            }
        }
        break;
    case 2:
        menu->sprites[1].color = 0x80808080;
        menu->sprites[2].color = 0xffff8080;
        menu->sprites[1].scale_y = 1.5f;
        menu->sprites[1].scale_x = 1.5f;
        menu->sprites[2].scale_y = 1.7f;
        menu->sprites[2].scale_x = 1.7f;
        menu->sprites[1].pos_offset = (VEC3) {};
        menu->sprites[2].pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(&g_Buttons, 0x10)
                || Buttons_WasPressed(&g_Buttons, 0x20)) {
                menu->state = 1;
            }
            if (Buttons_WasPressed(&g_Buttons, 1)) {
                for (int32_t i = 0; i < 3; i++) {
                    menu->sprites[i].pending_interrupt = 2;
                }
                for (int32_t i = 3; i < 6; i++) {
                    menu->sprites[i].pending_interrupt = 1;
                }
                menu->state = 5;
                menu->state_frames = 0;
            }
        }
        break;
    case 3:
        if (menu->state_frames >= 20) {
            menu->state = 0;
            g_GameManager.mode.paused = 0;
            for (int32_t i = 0; i < 6; i++) {
                menu->sprites[i].shown = false;
            }
        }
        break;
    case 4:
        menu->sprites[4].color = 0xffff8080;
        menu->sprites[5].color = 0x80808080;
        menu->sprites[4].scale_y = 1.7f;
        menu->sprites[4].scale_x = 1.7f;
        menu->sprites[5].scale_y = 1.5f;
        menu->sprites[5].scale_x = 1.5f;
        menu->sprites[4].pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        menu->sprites[5].pos_offset = (VEC3) {};
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(&g_Buttons, 0x10)
                || Buttons_WasPressed(&g_Buttons, 0x20)) {
                menu->state = 5;
            }
            if (Buttons_WasPressed(&g_Buttons, 1)) {
                for (int32_t i = 3; i < 6; i++) {
                    menu->sprites[i].pending_interrupt = 2;
                }
                menu->state = 6;
                menu->state_frames = 0;
            }
        }
        break;
    case 5:
        menu->sprites[4].color = 0x80808080;
        menu->sprites[5].color = 0xffff8080;
        menu->sprites[4].scale_y = 1.5f;
        menu->sprites[4].scale_x = 1.5f;
        menu->sprites[5].scale_y = 1.7f;
        menu->sprites[5].scale_x = 1.7f;
        menu->sprites[4].pos_offset = (VEC3) {};
        menu->sprites[5].pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(&g_Buttons, 0x10)
                || Buttons_WasPressed(&g_Buttons, 0x20)) {
                menu->state = 4;
            }
            if (Buttons_WasPressed(&g_Buttons, 1)) {
                for (int32_t i = 0; i < 3; i++) {
                    menu->sprites[i].pending_interrupt = 1;
                }
                for (int32_t i = 3; i < 6; i++) {
                    menu->sprites[i].pending_interrupt = 2;
                }
                menu->state = 2;
                menu->state_frames = 0;
            }
        }
        break;
    case 6:
        if (menu->state_frames >= 20) {
            menu->state = 0;
            g_GameManager.mode.paused = 0;
            Scene_Request(SCENE_TITLE);
            for (int32_t i = 0; i < 6; i++) {
                menu->sprites[i].shown = false;
            }
        }
        break;
    }
    for (int32_t i = 0; i < 6; i++) {
        Anm_Step(&menu->sprites[i]);
    }
    if (g_AsciiText.capture_background) {
        Anm_Step(&menu->background);
    }
    menu->state_frames++;
}

void PauseMenu_DrawPauseMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.paused != 0) {
        M_SetViewport();
        ANM_DRAW draw = Anm_ScreenDraw();
        draw.interpolate = true;
        draw.part = ASCII_MENU_SPRITE_COUNT;
        draw.view =
            Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
        if (g_AsciiText.capture_background && menu->state != 0) {
            ANM_VM background = menu->background;
            background.z_write_off = true;
            Anm_DrawUnrotated(&background, &draw);
        }
        for (int32_t i = 0; i < ASCII_MENU_SPRITE_COUNT; i++) {
            if (menu->sprites[i].shown) {
                draw.part = (uint32_t)i;
                Anm_DrawUnrotated(&menu->sprites[i], &draw);
            }
        }
    }
}

void PauseMenu_UpdateContinueMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.in_practice) {
        g_GameManager.mode.in_continue_menu = false;
        g_GameManager.score.shown = g_GameManager.score.value;
        Scene_Request(SCENE_GAME_RESULT);
        return;
    }
    if (g_GameManager.mode.in_replay) {
        g_GameManager.mode.in_continue_menu = false;
        Scene_Request(SCENE_REPLAY_END);
        g_GameManager.score.shown = g_GameManager.score.value;
        return;
    }
    if (g_GameManager.stats.continues_used >= 3
        || g_GameManager.run.difficulty >= 4) {
        g_GameManager.mode.in_continue_menu = false;
        Scene_Request(SCENE_GAME_RESULT);
        g_GameManager.score.shown = g_GameManager.score.value;
        return;
    }
    switch (menu->state) {
    case 0:
        if (menu->state_frames == 0) {
            for (int32_t i = 0; i < 4; i++) {
                if (i < 2) {
                    Anm_StartScript(&menu->sprites[i], i + 8);
                } else {
                    Anm_StartScript(&menu->sprites[i], i + 4);
                }
                menu->sprites[i].pending_interrupt = 1;
            }
            if (g_AsciiText.capture_background) {
                Anm_SetPendingCopy(3, (GFX_RECT) { 32, 16, 384, 448 });
                Anm_StartScript(&menu->background, 0x718);
                menu->background.pos = (VEC3) { 32.0f, 16.0f, 0.0f };
            }
        }
        if (menu->state_frames > 8) {
            break;
        }
        menu->state += 2;
        menu->state_frames = 0;
        [[fallthrough]];
    case 1:
        menu->sprites[2].color = 0xffff8080;
        menu->sprites[3].color = 0x80808080;
        menu->sprites[2].scale_y = 1.7f;
        menu->sprites[2].scale_x = 1.7f;
        menu->sprites[3].scale_y = 1.5f;
        menu->sprites[3].scale_x = 1.5f;
        menu->sprites[2].pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        menu->sprites[3].pos_offset = (VEC3) {};
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(&g_Buttons, 0x10)
                || Buttons_WasPressed(&g_Buttons, 0x20)) {
                menu->state = 2;
            }
            if (Buttons_WasPressed(&g_Buttons, 1)) {
                for (int32_t i = 0; i < 4; i++) {
                    menu->sprites[i].pending_interrupt = 2;
                }
                menu->state = 3;
                menu->background.pending_interrupt = 1;
                menu->state_frames = 0;
            }
        }
        break;
    case 2:
        menu->sprites[3].color = 0xffff8080;
        menu->sprites[2].color = 0x80808080;
        menu->sprites[2].scale_y = 1.5f;
        menu->sprites[2].scale_x = 1.5f;
        menu->sprites[3].scale_y = 1.7f;
        menu->sprites[3].scale_x = 1.7f;
        menu->sprites[3].pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        menu->sprites[2].pos_offset = (VEC3) {};
        if (menu->state_frames >= 30) {
            if (Buttons_WasPressed(&g_Buttons, 0x10)
                || Buttons_WasPressed(&g_Buttons, 0x20)) {
                menu->state = 1;
            }
            if (Buttons_WasPressed(&g_Buttons, 1)) {
                for (int32_t i = 0; i < 4; i++) {
                    menu->sprites[i].pending_interrupt = 2;
                }
                menu->state = 4;
                menu->state_frames = 0;
            }
        }
        break;
    case 4:
        if (menu->state_frames >= 20) {
            menu->state = 0;
            menu->state_frames = 0;
            g_GameManager.mode.in_continue_menu = false;
            Scene_Request(SCENE_GAME_RESULT);
            for (int32_t i = 0; i < 4; i++) {
                menu->sprites[i].shown = false;
            }
            g_GameManager.score.shown = g_GameManager.score.value;
            return;
        }
        break;
    case 3:
        if (menu->state_frames >= 30) {
            menu->state = 0;
            menu->state_frames = 0;
            g_GameManager.mode.in_continue_menu = false;
            for (int32_t i = 0; i < 4; i++) {
                menu->sprites[i].shown = false;
            }
            g_GameManager.stats.continues_used++;
            g_GameManager.score.shown = g_GameManager.stats.continues_used;
            g_GameManager.score.step = 0;
            g_GameManager.score.value = g_GameManager.score.shown;
            g_GameManager.player.lives = (int8_t)g_GameManager.run.start_lives;
            g_GameManager.player.bombs = (int8_t)g_GameManager.run.start_bombs;
            g_GameManager.player.graze = 0;
            g_GameManager.player.point_items = 0;
            g_GameManager.player.power = 0;
            g_GameManager.score.next_extend = 0;
            return;
        }
        break;
    }
    for (int32_t i = 0; i < 4; i++) {
        Anm_Step(&menu->sprites[i]);
    }
    if (g_AsciiText.capture_background) {
        Anm_Step(&menu->background);
    }
    menu->state_frames++;
}

void PauseMenu_DrawContinueMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.in_continue_menu) {
        M_SetViewport();
        ANM_DRAW draw = Anm_ScreenDraw();
        draw.interpolate = true;
        draw.part = ASCII_MENU_SPRITE_COUNT;
        draw.view =
            Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
        if (g_AsciiText.capture_background
            && (menu->state != 0 || menu->state_frames > 2)) {
            Anm_DrawUnrotated(&menu->background, &draw);
        }
        if (menu->state == 1 || menu->state == 2) {
            menu->sprites[4] = menu->sprites[1];
            menu->sprites[4].pos.x += 8.0f * menu->sprites[4].scale_x;
            Anm_SetSprite(
                &menu->sprites[4], 0x1e - g_GameManager.stats.continues_used);
            ANM_VM counter = menu->sprites[4];
            counter.presentation_generation =
                menu->sprites[1].presentation_generation;
            draw.part = 4;
            Anm_DrawUnrotated(&counter, &draw);
        }
        for (int32_t i = 0; i < 4; i++) {
            if (menu->sprites[i].shown) {
                draw.part = (uint32_t)i;
                Anm_DrawUnrotated(&menu->sprites[i], &draw);
            }
        }
    }
}
