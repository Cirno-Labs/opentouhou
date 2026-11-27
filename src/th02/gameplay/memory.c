#include "th02/gameplay/memory.h"
#include "th02/bomb.h"

#include "th02/demo.h"
#include "th02/video.h"
#include "core/base/assert.h"

#include <string.h>

#define M_PARAGRAPHS (288000 / 16)
#define M_BLOCK_LIMIT 1024

typedef struct {
    uint16_t first;
    uint16_t end;
    bool used;
} M_BLOCK;

typedef struct {
    M_BLOCK blocks[M_BLOCK_LIMIT];
    uint16_t count;
    uint16_t top;
    uint16_t patterns[PATTERN_LIMIT];
    uint16_t columns[49];
    uint16_t tiles;
    uint16_t files[2];
    uint16_t faces[6];
    bool active;
    bool sprite_buffer;
} M_MEMORY;

static M_MEMORY m_Memory;

static void M_Insert(const uint16_t index, const M_BLOCK block)
{
    ASSERT(m_Memory.count < M_BLOCK_LIMIT);
    memmove(
        &m_Memory.blocks[index + 1], &m_Memory.blocks[index],
        (m_Memory.count - index) * sizeof(M_BLOCK));
    m_Memory.blocks[index] = block;
    m_Memory.count++;
}

static void M_Remove(const uint16_t index)
{
    m_Memory.count--;
    memmove(
        &m_Memory.blocks[index], &m_Memory.blocks[index + 1],
        (m_Memory.count - index) * sizeof(M_BLOCK));
}

static uint16_t M_Allocate(const size_t bytes)
{
    if (bytes == 0) {
        return 0;
    }
    const size_t paragraphs = (bytes + 15) / 16 + 1;
    for (uint16_t i = 0; i < m_Memory.count; i++) {
        M_BLOCK *const block = &m_Memory.blocks[i];
        if (block->used || (size_t)(block->end - block->first) < paragraphs) {
            continue;
        }
        const uint16_t first = block->first;
        const uint16_t end = block->end;
        block->used = true;
        if (end - first - paragraphs > 1) {
            block->end = first + paragraphs;
            M_Insert(i + 1, (M_BLOCK) { block->end, end, false });
        }
        return first + 1;
    }
    ASSERT(paragraphs <= m_Memory.top);
    m_Memory.top -= paragraphs;
    M_Insert(0, (M_BLOCK) { m_Memory.top, m_Memory.top + paragraphs, true });
    return m_Memory.top + 1;
}

static void M_Free(const uint16_t handle)
{
    if (handle == 0) {
        return;
    }
    uint16_t index = 0;
    while (index < m_Memory.count
           && m_Memory.blocks[index].first + 1 != handle) {
        index++;
    }
    ASSERT(index < m_Memory.count && m_Memory.blocks[index].used);
    m_Memory.blocks[index].used = false;
    if (index > 0 && !m_Memory.blocks[index - 1].used) {
        m_Memory.blocks[index - 1].end = m_Memory.blocks[index].end;
        M_Remove(index--);
    }
    if (index + 1 < m_Memory.count && !m_Memory.blocks[index + 1].used) {
        m_Memory.blocks[index].end = m_Memory.blocks[index + 1].end;
        M_Remove(index + 1);
    }
    if (index == 0) {
        m_Memory.top = m_Memory.blocks[0].end;
        M_Remove(0);
    }
}

static void M_OpenFile(void)
{
    m_Memory.files[0] = M_Allocate(32);
    m_Memory.files[1] = M_Allocate(528);
}

static void M_CloseFile(void)
{
    M_Free(m_Memory.files[1]);
    M_Free(m_Memory.files[0]);
    memset(m_Memory.files, 0, sizeof(m_Memory.files));
}

static void M_ReadFile(void)
{
    M_OpenFile();
    M_CloseFile();
}

static void M_Image(const PI_IMAGE *const image)
{
    M_OpenFile();
    M_Allocate((size_t)image->width * (image->height + 2) / 2);
    M_CloseFile();
}

static void M_Patterns(
    const PATTERN_BANK *const bank, const uint16_t first, const uint16_t end)
{
    bool open = false;
    for (uint16_t slot = first; slot < end; slot++) {
        const PATTERN *const pattern = &bank->entries[slot];
        if (pattern->size == 0) {
            continue;
        }
        if (!open || pattern->sheet_start) {
            if (open) {
                M_CloseFile();
            }
            M_OpenFile();
            open = true;
        }
        const size_t bytes =
            (size_t)(pattern->size >> 8) * (pattern->size & 255) * 5;
        const uint16_t handle = M_Allocate(bytes);
        if (!m_Memory.sprite_buffer) {
            M_Allocate(9216);
            m_Memory.sprite_buffer = true;
        }
        M_Free(m_Memory.patterns[slot]);
        m_Memory.patterns[slot] = handle;
    }
    if (open) {
        M_CloseFile();
    }
}

void GameplayMemory_Begin(
    const GAMEPLAY_ASSETS *const assets, const BOMB_ASSETS *const bomb,
    const bool demo)
{
    memset(&m_Memory, 0, sizeof(m_Memory));
    m_Memory.active = true;
    m_Memory.top = M_PARAGRAPHS;
    for (size_t i = 0; i < ARRAY_SIZE(m_Memory.faces); i++) {
        m_Memory.faces[i] = assets->bosses.stages[i].faces.count;
    }
    if (demo) {
        M_Allocate(DEMO_INPUT_COUNT * 2);
    }
    M_Image(&assets->backdrop.image);
    M_Patterns(&assets->patterns, 0, 128);
    M_Allocate(8192);
    M_ReadFile();
    M_OpenFile();
    M_Allocate(0x438);
    M_CloseFile();
    M_Image(&bomb->illustration);
    if (bomb->shot_type == 1) {
        M_OpenFile();
        M_Allocate(0x1440);
        M_CloseFile();
    }
}

void GameplayMemory_LoadStage(
    const STAGE_ASSETS *const stage, const PATTERN_BANK *const patterns)
{
    if (!m_Memory.active) {
        return;
    }
    for (uint16_t slot = 193; slot-- > 128;) {
        M_Free(m_Memory.patterns[slot]);
        m_Memory.patterns[slot] = 0;
    }
    M_Patterns(
        patterns, stage->pattern_first,
        stage->pattern_first + stage->pattern_count);
    M_ReadFile();
    M_OpenFile();
    M_Free(m_Memory.tiles);
    m_Memory.tiles = M_Allocate((size_t)stage->background.tiles.count * 128);
    M_CloseFile();
    for (size_t i = 0; i < ARRAY_SIZE(m_Memory.columns); i++) {
        M_Free(m_Memory.columns[i]);
    }
    M_OpenFile();
    const uint16_t data = M_Allocate(stage->data.payload_size);
    M_CloseFile();
    for (size_t i = 0; i < ARRAY_SIZE(m_Memory.columns); i++) {
        m_Memory.columns[i] =
            M_Allocate((size_t)stage->data.declared_line_count * 2);
    }
    M_Free(data);
    M_ReadFile();
    M_Free(m_Memory.tiles);
    M_OpenFile();
    m_Memory.tiles = M_Allocate((size_t)m_Memory.faces[stage->stage] * 128);
    M_CloseFile();
}

void GameplayMemory_SelectPatterns(const PATTERN_BANK *const patterns)
{
    if (!m_Memory.active) {
        return;
    }
    for (uint16_t slot = 128; slot < PATTERN_LIMIT; slot++) {
        M_Free(m_Memory.patterns[slot]);
        m_Memory.patterns[slot] = 0;
    }
    M_Patterns(patterns, 128, patterns->next);
}

void GameplayMemory_Shutdown(void)
{
    memset(&m_Memory, 0, sizeof(m_Memory));
}

RESULT GameplayMemory_CopyPage(const uint8_t destination)
{
    FAIL_IF(
        m_Memory.active && m_Memory.top * 16u < 32000,
        "Insufficient page-copy scratch memory");
    Video_CopyPage(destination);
    return OK;
}
