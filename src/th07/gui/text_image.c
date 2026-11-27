#include "th07/anm.h"
#include "th07/files.h"
#include "th07/gui/private.h"

#include "core/base/log.h"
#include "core/base/utils.h"
#include "core/gfx.h"
#include "stg/translation.h"

#define M_BOSS_TITLE M_NAMEPLATE_SCRIPT
#define M_BOSS_NAME (M_NAMEPLATE_SCRIPT + 1)
#define M_SCRIPT_FIRST_SPRITE 0x700

typedef struct {
    const char *file;
    int32_t sprite;
    int32_t texture_slot;
    int32_t width;
    int32_t height;
    ANM_INSTR *instrs;
    int32_t instr_count;
} M_IMAGE;

typedef struct {
    int32_t sprite;
    const M_IMAGE *image;
    float texture_width;
    float texture_height;
    int32_t columns;
    int32_t count;
    int32_t shown_row;
    bool replaced;
    ANM_SPRITE game_sprite;
    const ANM_SCRIPT *game_script;
    ANM_SCRIPT script;
} M_SLOT;

// Holds base_tsa's scripts for the nameplate's name and title text
// images, as its textimage_init breakpoint names them. The float
// arguments are IEEE 754 bit patterns.
static ANM_INSTR m_BossTitleScript[] = {
    { .opcode = 3, .args = { { .i = 2 } } },
    { .opcode = 6,
      .args = { { .f = 224.0f }, { .f = 352.0f }, { .f = 0.0f } } },
    { .opcode = 8, .args = { { .i = 0 } } },
    { .opcode = 15, .args = { { .i = 240 }, { .i = 60 } } },
    { .opcode = 15, .time = 360, .args = { { .i = 0 }, { .i = 60 } } },
    { .opcode = 1, .time = 420 },
};
static ANM_INSTR m_BossNameScript[] = {
    { .opcode = 3, .args = { { .i = 3 } } },
    { .opcode = 6,
      .args = { { .f = 224.0f }, { .f = 352.0f }, { .f = 0.0f } } },
    { .opcode = 8, .args = { { .i = 0 } } },
    { .opcode = 15, .args = { { .i = 240 }, { .i = 60 } } },
    { .opcode = 15, .time = 360, .args = { { .i = 0 }, { .i = 60 } } },
    { .opcode = 1, .time = 420 },
};
// Reproduces base_tsa's text image setup for th07: the two pre-drawn
// nameplate images (the speaker's name and their title), their texture
// slots and their scripts. Each is a strip of one row per stage.
static const M_IMAGE m_Images[] = {
    { "ti_bosstitle.png", M_BOSS_TITLE, 50, 384, 64, m_BossTitleScript,
      ARRAY_SIZE(m_BossTitleScript) },
    { "ti_bossname.png", M_BOSS_NAME, 51, 384, 64, m_BossNameScript,
      ARRAY_SIZE(m_BossNameScript) },
};

static M_SLOT m_Slots[] = {
    { .sprite = M_BOSS_TITLE },
    { .sprite = M_BOSS_NAME },
};

static M_SLOT *M_GetSlot(const int32_t sprite)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Slots); i++) {
        if (m_Slots[i].sprite == sprite) {
            return &m_Slots[i];
        }
    }
    return nullptr;
}

// Loads image into its texture slot. Reports failure when the image does
// not load or its size is not a whole number of rows and columns.
static RESULT M_LoadImage(const M_IMAGE *const image, M_SLOT *const slot)
{
    MUST(Anm_LoadTexture(
        image->texture_slot, image->file, ANM_TEXTURE_RGBA8, 0));
    GFX_TEXTURE *const texture = Anm_GetTexture(image->texture_slot);
    const int32_t width = GfxTexture_GetWidth(texture);
    const int32_t height = GfxTexture_GetHeight(texture);
    if (width % image->width != 0 || height % image->height != 0) {
        Anm_ReleaseTexture(image->texture_slot);
        return FAIL(
            "%s: the size must be a multiple of %dx%d, got %dx%d", image->file,
            image->width, image->height, width, height);
    }
    slot->image = image;
    slot->texture_width = (float)width;
    slot->texture_height = (float)height;
    slot->columns = width / image->width;
    slot->count = slot->columns * (height / image->height);
    slot->script = (ANM_SCRIPT) {
        .instrs = image->instrs,
        .count = image->instr_count,
        .sprite_offset = M_SCRIPT_FIRST_SPRITE,
    };
    return OK;
}

static bool M_WouldReplace(const M_SLOT *const slot)
{
    return slot->image != nullptr && slot->shown_row >= 0
        && slot->shown_row < slot->count;
}

static void M_Commit(M_SLOT *const slot, const bool replace)
{
    if (replace) {
        if (!slot->replaced) {
            slot->game_sprite = *Anm_GetSpriteEntry(slot->sprite);
            slot->game_script = Anm_GetScript(slot->sprite);
        }
        const M_IMAGE *const image = slot->image;
        ANM_SPRITE sprite = {
            .slot = image->texture_slot,
            .x0 = (float)(slot->shown_row % slot->columns * image->width),
            .y0 = (float)(slot->shown_row / slot->columns * image->height),
            .texture_width = slot->texture_width,
            .texture_height = slot->texture_height,
            .width = (float)image->width,
            .height = (float)image->height,
            .id = slot->game_sprite.id,
        };
        sprite.x1 = sprite.x0 + sprite.width;
        sprite.y1 = sprite.y0 + sprite.height;
        sprite.u0 = sprite.x0 / sprite.texture_width;
        sprite.v0 = sprite.y0 / sprite.texture_height;
        sprite.u1 = sprite.x1 / sprite.texture_width;
        sprite.v1 = sprite.y1 / sprite.texture_height;
        Anm_SetSpriteEntry(slot->sprite, &sprite);
        Anm_SetScriptEntry(slot->sprite, &slot->script);
    } else if (slot->replaced) {
        Anm_SetSpriteEntry(slot->sprite, &slot->game_sprite);
        Anm_SetScriptEntry(slot->sprite, slot->game_script);
    }
    slot->replaced = replace;
}

// Replaces the nameplate's name and title images together or neither,
// as retail shows a speaker's name and title together.
static void M_CommitAll(void)
{
    const bool both = M_WouldReplace(M_GetSlot(M_BOSS_TITLE))
        && M_WouldReplace(M_GetSlot(M_BOSS_NAME));
    for (size_t i = 0; i < ARRAY_SIZE(m_Slots); i++) {
        M_Commit(&m_Slots[i], both);
    }
}

void GuiTextImage_Load(void)
{
    if (!Translation_HasBaseTsa()) {
        return;
    }
    for (size_t i = 0; i < ARRAY_SIZE(m_Images); i++) {
        M_SLOT *const slot = M_GetSlot(m_Images[i].sprite);
        if (slot->image == nullptr && Files_IsPatched(m_Images[i].file)) {
            Result_Log(M_LoadImage(&m_Images[i], slot), LOG_LEVEL_WARNING);
        }
    }
}

void GuiTextImage_Release(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Slots); i++) {
        M_SLOT *const slot = &m_Slots[i];
        M_Commit(slot, false);
        if (slot->image != nullptr) {
            Anm_ReleaseTexture(slot->image->texture_slot);
        }
        *slot = (M_SLOT) { .sprite = slot->sprite };
    }
}

void GuiTextImage_ShowStage(const int32_t stage)
{
    M_GetSlot(M_BOSS_TITLE)->shown_row = stage - 1;
    M_GetSlot(M_BOSS_NAME)->shown_row = stage - 1;
    M_CommitAll();
}

bool GuiTextImage_Replaces(const int32_t sprite)
{
    const M_SLOT *const slot = M_GetSlot(sprite);
    return slot != nullptr && slot->replaced;
}
