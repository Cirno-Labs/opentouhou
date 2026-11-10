#include "th06/stage.h"

#include "th06/chain.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/math/x87.h"

#include <string.h>

#define M_ANM_SLOT 4
#define M_FIRST_SPRITE 0x300
#define M_SPELL_CARD_SCRIPT 0x2b3
#define M_CALC_PRIORITY 6
#define M_DRAW_PRIORITY 3
#define M_DRAW2_PRIORITY 4
#define M_NAME_SIZE 0x80
#define M_OBJECTS_OFFSET 0x490
#define M_QUAD_SIZE 28
#define M_INSTANCE_SIZE 16
#define M_INSTR_SIZE 20

STAGE g_Stage;

static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;
static CHAIN_ELEM m_Draw2Elem;

static VEC3 M_ReadVec3(READER *const reader)
{
    const float x = Reader_ReadF32(reader);
    const float y = Reader_ReadF32(reader);
    const float z = Reader_ReadF32(reader);
    return (VEC3) { x, y, z };
}

static RESULT M_ReadName(READER *const reader, char **const out)
{
    char bytes[M_NAME_SIZE];
    Reader_ReadBytes(reader, bytes, sizeof(bytes));
    FAIL_IF(Reader_Failed(reader), "names run past the file");
    const char *const end = memchr(bytes, '\0', sizeof(bytes));
    const size_t length =
        end != nullptr ? (size_t)(end - bytes) : sizeof(bytes);
    return Cp932_Decode(bytes, length, out);
}

static RESULT M_ReadObject(
    READER *const reader, const size_t offset, STAGE_OBJECT *const object)
{
    Reader_Seek(reader, offset + 2);
    object->layer = Reader_ReadI8(reader);
    Reader_Skip(reader, 1);
    object->pos = M_ReadVec3(reader);
    object->size = M_ReadVec3(reader);
    const size_t quads = Reader_Tell(reader);
    for (;;) {
        const int16_t type = Reader_ReadI16(reader);
        const int16_t size = Reader_ReadI16(reader);
        FAIL_IF(Reader_Failed(reader), "a quad list runs past the file");
        if (type < 0) {
            break;
        }
        FAIL_IF(size < M_QUAD_SIZE, "a quad is %d bytes", size);
        Reader_Skip(reader, (size_t)size - 4);
        object->quad_count++;
    }
    object->quads =
        Mem_AllocArrZ((size_t)object->quad_count, sizeof(STAGE_QUAD));
    Reader_Seek(reader, quads);
    for (int32_t i = 0; i < object->quad_count; i++) {
        const size_t start = Reader_Tell(reader);
        STAGE_QUAD *const quad = &object->quads[i];
        quad->type = Reader_ReadI16(reader);
        const int16_t size = Reader_ReadI16(reader);
        quad->script = Reader_ReadI16(reader);
        Reader_Skip(reader, 2);
        quad->pos = M_ReadVec3(reader);
        quad->width = Reader_ReadF32(reader);
        quad->height = Reader_ReadF32(reader);
        Reader_Seek(reader, start + (size_t)size);
    }
    return OK;
}

// Checks that the script stops on its last instruction and that every
// keyframe has a keyframe after it, which the camera moves towards.
static RESULT M_CheckScript(void)
{
    const STAGE_INSTR *const script = g_Stage.data.script;
    const int32_t count = g_Stage.data.script_count;
    FAIL_IF(count == 0, "the script is empty");
    const STAGE_INSTR *const last = &script[count - 1];
    FAIL_IF(
        last->opcode >= STAGE_OP_KEYFRAME && last->opcode <= STAGE_OP_WAIT
            && !(last->opcode == STAGE_OP_KEYFRAME && last->frame == -1),
        "the script runs past its end");
    bool keyframe_after = false;
    for (int32_t i = count - 1; i >= 0; i--) {
        if (script[i].opcode != STAGE_OP_KEYFRAME) {
            continue;
        }
        FAIL_IF(
            script[i].frame != -1 && !keyframe_after,
            "the keyframe at %d has none after it", script[i].frame);
        keyframe_after = true;
    }
    return OK;
}

static RESULT M_Parse(READER *const reader)
{
    const int32_t object_count = Reader_ReadI16(reader);
    Reader_Skip(reader, 2);
    const size_t instances = (size_t)Reader_ReadI32(reader);
    const size_t script = (size_t)Reader_ReadI32(reader);
    Reader_Skip(reader, 4);
    FAIL_IF(Reader_Failed(reader) || object_count < 0, "bad header");
    MUST(M_ReadName(reader, &g_Stage.data.name));
    for (int32_t i = 0; i < STAGE_SONG_COUNT; i++) {
        MUST(M_ReadName(reader, &g_Stage.data.song_names[i]));
    }
    for (int32_t i = 0; i < STAGE_SONG_COUNT; i++) {
        MUST(M_ReadName(reader, &g_Stage.data.song_paths[i]));
    }

    g_Stage.data.objects =
        Mem_AllocArrZ((size_t)object_count, sizeof(STAGE_OBJECT));
    g_Stage.data.object_count = object_count;
    for (int32_t i = 0; i < object_count; i++) {
        Reader_Seek(reader, M_OBJECTS_OFFSET + 4 * (size_t)i);
        const size_t offset = (size_t)Reader_ReadI32(reader);
        FAIL_IF(Reader_Failed(reader), "object %d runs past the file", i);
        MUST(
            M_ReadObject(reader, offset, &g_Stage.data.objects[i]), "object %d",
            i);
        g_Stage.data.sprite_count += g_Stage.data.objects[i].quad_count;
    }

    Reader_Seek(reader, instances);
    while (Reader_ReadI16(reader) >= 0) {
        Reader_Skip(reader, M_INSTANCE_SIZE - 2);
        g_Stage.data.instance_count++;
    }
    FAIL_IF(Reader_Failed(reader), "the instances run past the file");
    g_Stage.data.instances = Mem_AllocArrZ(
        (size_t)g_Stage.data.instance_count, sizeof(STAGE_INSTANCE));
    Reader_Seek(reader, instances);
    for (int32_t i = 0; i < g_Stage.data.instance_count; i++) {
        STAGE_INSTANCE *const instance = &g_Stage.data.instances[i];
        instance->object = Reader_ReadI16(reader);
        Reader_Skip(reader, 2);
        instance->pos = M_ReadVec3(reader);
        FAIL_IF(
            instance->object >= object_count, "instance %d places object %d", i,
            instance->object);
    }

    FAIL_IF(script > Reader_Size(reader), "the script starts past the file");
    g_Stage.data.script_count =
        (int32_t)((Reader_Size(reader) - script) / M_INSTR_SIZE);
    g_Stage.data.script =
        Mem_AllocArrZ((size_t)g_Stage.data.script_count, sizeof(STAGE_INSTR));
    Reader_Seek(reader, script);
    for (int32_t i = 0; i < g_Stage.data.script_count; i++) {
        STAGE_INSTR *const instr = &g_Stage.data.script[i];
        instr->frame = Reader_ReadI32(reader);
        instr->opcode = Reader_ReadI16(reader);
        Reader_Skip(reader, 2);
        instr->args = M_ReadVec3(reader);
    }
    return M_CheckScript();
}

// Starts a VM for each quad, at its script, and points the quad at it.
static void M_StartSprites(void)
{
    g_Stage.data.sprites =
        Mem_AllocArrZ((size_t)g_Stage.data.sprite_count, sizeof(ANM_VM));
    int16_t count = 0;
    for (int32_t i = 0; i < g_Stage.data.object_count; i++) {
        STAGE_OBJECT *const object = &g_Stage.data.objects[i];
        object->animating = true;
        for (int32_t j = 0; j < object->quad_count; j++) {
            STAGE_QUAD *const quad = &object->quads[j];
            Anm_StartScriptAtOrigin(
                &g_Stage.data.sprites[count], quad->script + M_FIRST_SPRITE);
            quad->sprite = count++;
        }
    }
}

static RESULT M_Load(const char *const anm_path, const char *const data_path)
{
    MUST(Anm_Load(M_ANM_SLOT, anm_path, M_FIRST_SPRITE));
    READER reader;
    MUST(Files_Load(data_path, &reader), "%s", data_path);
    const RESULT result = M_Parse(&reader);
    Reader_Close(&reader);
    MUST(result, "%s", data_path);
    M_StartSprites();
    return OK;
}

static void M_Release(void)
{
    Anm_Release(M_ANM_SLOT);
    Mem_Free(g_Stage.data.name);
    for (int32_t i = 0; i < STAGE_SONG_COUNT; i++) {
        Mem_Free(g_Stage.data.song_names[i]);
        Mem_Free(g_Stage.data.song_paths[i]);
    }
    for (int32_t i = 0; i < g_Stage.data.object_count; i++) {
        Mem_Free(g_Stage.data.objects[i].quads);
    }
    Mem_Free(g_Stage.data.objects);
    Mem_Free(g_Stage.data.instances);
    Mem_Free(g_Stage.data.script);
    Mem_Free(g_Stage.data.sprites);
    g_Stage.data = (typeof(g_Stage.data)) {};
}

static void M_StepObjects(void)
{
    for (int32_t i = 0; i < g_Stage.data.object_count; i++) {
        STAGE_OBJECT *const object = &g_Stage.data.objects[i];
        if (!object->animating) {
            continue;
        }
        int32_t busy = 0;
        for (int32_t j = 0; j < object->quad_count; j++) {
            const STAGE_QUAD *const quad = &object->quads[j];
            ANM_VM *const vm = &g_Stage.data.sprites[quad->sprite];
            if (quad->type == 0 || quad->type == 1) {
                Anm_Step(vm);
            }
            if (vm->script != nullptr && vm->pc >= 0) {
                busy++;
            }
        }
        if (busy == 0) {
            object->animating = false;
        }
    }
}

// Runs the script instructions due at the current frame. Returns the one
// it stopped at, or the next keyframe after one it passed, as the
// original does: the camera and the clock look at that one.
static const STAGE_INSTR *M_RunScript(void)
{
    for (;;) {
        const STAGE_INSTR *const instr =
            &g_Stage.data.script[g_Stage.script.index];
        const bool due = g_Stage.script.time.current >= instr->frame;
        switch (instr->opcode) {
        case STAGE_OP_KEYFRAME:
            if (instr->frame == -1) {
                g_Stage.camera.key_pos = instr->args;
                g_Stage.camera.pos = g_Stage.camera.key_pos;
            } else if (due) {
                g_Stage.camera.pos = instr->args;
                g_Stage.camera.key_pos = instr->args;
                g_Stage.camera.key_frame = instr->frame;
                g_Stage.script.index++;
                const STAGE_INSTR *next = instr + 1;
                while (next->opcode != STAGE_OP_KEYFRAME) {
                    next++;
                }
                g_Stage.camera.next_key_frame = next->frame;
                g_Stage.camera.next_key_pos = next->args;
                return next;
            }
            return instr;
        case STAGE_OP_FOG:
            if (!due) {
                return instr;
            }
            memcpy(&g_Stage.fog.current.color, &instr->args.x, 4);
            g_Stage.fog.current.near = instr->args.y;
            g_Stage.fog.current.far = instr->args.z;
            g_Stage.script.index++;
            g_Stage.fog.to = g_Stage.fog.current;
            continue;
        case STAGE_OP_FOG_FADE:
            if (!due) {
                return instr;
            }
            g_Stage.fog.from = g_Stage.fog.current;
            memcpy(&g_Stage.fog.frames, &instr->args.x, 4);
            Timer_Set(&g_Stage.fog.time, 0);
            g_Stage.script.index++;
            continue;
        case STAGE_OP_CAMERA_OFFSET:
            if (!due) {
                return instr;
            }
            g_Stage.offset.from = g_Stage.offset.to;
            g_Stage.offset.to = instr->args;
            g_Stage.script.index++;
            continue;
        case STAGE_OP_OFFSET_FADE:
            if (!due) {
                return instr;
            }
            memcpy(&g_Stage.offset.frames, &instr->args.x, 4);
            Timer_Set(&g_Stage.offset.time, 0);
            g_Stage.script.index++;
            continue;
        case STAGE_OP_WAIT:
            if (!g_Stage.script.resume) {
                return instr;
            }
            g_Stage.script.index++;
            g_Stage.script.resume = false;
            continue;
        default:
            return instr;
        }
    }
}

static void M_FadeOffset(void)
{
    if (g_Stage.offset.time.current < g_Stage.offset.frames) {
        Timer_Tick(&g_Stage.offset.time);
    } else {
        Timer_Set(&g_Stage.offset.time, g_Stage.offset.frames);
    }
    const VEC3 delta = Vec3_Sub(g_Stage.offset.to, g_Stage.offset.from);
    const float t =
        (float)g_Stage.offset.time.current / (float)g_Stage.offset.frames;
    g_GameManager.camera.offset_x = delta.x * t + g_Stage.offset.from.x;
    g_GameManager.camera.offset_y = delta.y * t + g_Stage.offset.from.y;
    g_GameManager.camera.zoom = delta.z * t + g_Stage.offset.from.z;
}

static void M_FadeFog(void)
{
    Timer_Tick(&g_Stage.fog.time);
    float t = (float)g_Stage.fog.time.current / (float)g_Stage.fog.frames;
    if (t >= 1.0f) {
        t = 1.0f;
    }
    const STAGE_FOG *const from = &g_Stage.fog.from;
    const STAGE_FOG *const to = &g_Stage.fog.to;
    uint32_t color = 0;
    for (int32_t i = 0; i < 4; i++) {
        const float a = (float)((from->color >> (i * 8)) & 0xff);
        const float b = (float)((to->color >> (i * 8)) & 0xff);
        color |= (uint32_t)(uint8_t)X87_Ftol((b - a) * t + a) << (i * 8);
    }
    g_Stage.fog.current.color = color;
    g_Stage.fog.current.near = (to->near - from->near) * t + from->near;
    g_Stage.fog.current.far = (to->far - from->far) * t + from->far;
    if (g_Stage.fog.time.current >= g_Stage.fog.frames) {
        g_Stage.fog.frames = 0;
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    if (g_Stage.data.script == nullptr) {
        return CHAIN_CONTINUE;
    }
    if (g_GameManager.mode.frozen) {
        g_Stage.spell_card.background.color = 0x60803030u;
        return CHAIN_CONTINUE;
    }
    const STAGE_INSTR *const instr = M_RunScript();
    if (instr->frame != -1) {
        const float t = ((float)g_Stage.script.time.current
                         - (float)g_Stage.camera.key_frame)
            / (float)(g_Stage.camera.next_key_frame - g_Stage.camera.key_frame);
        const VEC3 next = g_Stage.camera.next_key_pos;
        const VEC3 key = g_Stage.camera.key_pos;
        g_Stage.camera.pos.x = (next.x - key.x) * t + key.x;
        g_Stage.camera.pos.y = (next.y - key.y) * t + key.y;
        g_Stage.camera.pos.z = (next.z - key.z) * t + key.z;
    }
    if (g_Stage.offset.frames != 0) {
        M_FadeOffset();
    }
    if (g_Stage.fog.frames != 0) {
        M_FadeFog();
    }
    if (instr->opcode != STAGE_OP_WAIT) {
        Timer_Tick(&g_Stage.script.time);
    }
    M_StepObjects();
    if (g_Stage.spell_card.state >= 1) {
        if (g_Stage.spell_card.frames == 60) {
            g_Stage.spell_card.state++;
        }
        g_Stage.spell_card.frames++;
        Anm_Step(&g_Stage.spell_card.background);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    return CHAIN_CONTINUE;
}

// Starts the spell card background on the spell card's first frame. The
// drawing comes with graphics.
static CHAIN_STATUS M_Draw2(void *const arg)
{
    if (g_Stage.spell_card.state >= 1 && g_Stage.spell_card.frames <= 0) {
        Anm_StartScript(&g_Stage.spell_card.background, M_SPELL_CARD_SCRIPT);
    }
    g_GameManager.camera.viewport.min_z = 0.0f;
    g_GameManager.camera.viewport.max_z = 0.5f;
    GameManager_SetUpFixedCamera(0.0f);
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    static const char *const paths[][2] = {
        { "dummy", "dummy" },
        { "data/stg1bg.anm", "data/stage1.std" },
        { "data/stg2bg.anm", "data/stage2.std" },
        { "data/stg3bg.anm", "data/stage3.std" },
        { "data/stg4bg.anm", "data/stage4.std" },
        { "data/stg5bg.anm", "data/stage5.std" },
        { "data/stg6bg.anm", "data/stage6.std" },
        { "data/stg7bg.anm", "data/stage7.std" },
    };
    const int32_t stage = g_GameManager.run.stage;
    FAIL_IF(stage < 0 || stage > 7, "no stage %d", stage);
    Timer_Set(&g_Stage.script.time, 0);
    g_Stage.script.index = 0;
    g_Stage.camera.pos = (VEC3) {};
    g_Stage.spell_card.state = 0;
    g_Stage.fog.frames = 0;
    const RESULT result = M_Load(paths[stage][0], paths[stage][1]);
    if (!IS_OK(result)) {
        M_Release();
        return result;
    }
    g_Stage.fog.current = (STAGE_FOG) {
        .near = 200.0f,
        .far = 500.0f,
        .color = 0xff000000u,
    };
    g_Stage.offset.to = (VEC3) { 0.0f, 0.0f, 1.0f };
    g_Stage.offset.from = (VEC3) { 0.0f, 0.0f, 1.0f };
    g_Stage.offset.frames = 1;
    Timer_Set(&g_Stage.offset.time, 0);
    g_Stage.script.resume = false;
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_Release();
}

RESULT Stage_Register(void)
{
    g_Stage = (STAGE) {};
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    MUST(Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY));
    Chain_InitElem(&m_Draw2Elem, M_Draw2, nullptr);
    return Chain_AddDraw(&g_Chain, &m_Draw2Elem, M_DRAW2_PRIORITY);
}

void Stage_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
    Chain_Remove(&m_Draw2Elem);
}
