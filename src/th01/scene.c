#include "th01/scene_private.h"
#include "th01/stage/objects.h"

#include "core/base/assert.h"

#include <string.h>

static SCENE_STATE m_Scene;
SCENE_STATE *const p_Scene = &m_Scene;

void Scene_Init(void)
{
    memset(&m_Scene, 0, sizeof(m_Scene));
}

RESULT Scene_LoadReader(
    READER *const reader, const uint8_t id, const ALLOCATOR *const allocator,
    SCENE_DATA *const out)
{
    *out = (SCENE_DATA) {};
    FAIL_IF(id >= SCENE_COUNT, "Invalid scene ID");
    SCENE_DATA loaded = { .id = id };
    MUST(StageData_Load(reader, allocator, &loaded.layouts));
    memcpy(loaded.background, "ST0.GRP", sizeof(loaded.background));
    memcpy(loaded.music, "ST0.MDT", sizeof(loaded.music));
    loaded.background[2] = loaded.music[2] = (char)('0' + id);
    *out = loaded;
    return OK;
}

RESULT Scene_Load(
    VFS *const vfs, const uint8_t id, const ALLOCATOR *const allocator,
    SCENE_DATA *const out)
{
    *out = (SCENE_DATA) {};
    FAIL_IF(id >= SCENE_COUNT, "Invalid scene ID");
    char name[] = "STAGE0.DAT";
    name[5] = (char)('0' + id);
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    RESULT result = Scene_LoadReader(&reader, id, allocator, out);
    Reader_Close(&reader);
    return result;
}

void Scene_Activate(const SCENE_DATA *const scene)
{
    ASSERT(scene != nullptr && scene->id < SCENE_COUNT);
    ASSERT(scene->layouts.stages != nullptr);
    m_Scene.active = scene;
}

void Scene_BeginStage(
    const int16_t stage_id, const bool first_in_scene, const PTN *const sprites)
{
    StageObjects_InitPrepared(
        &m_Scene.active->layouts, stage_id, first_in_scene, sprites);
}
