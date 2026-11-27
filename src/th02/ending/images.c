#include "th02/ending/images.h"

#include "core/base/assert.h"

#include <string.h>

static const struct {
    const char *path;
    int32_t width;
    int32_t height;
} m_Images[ENDING_IMAGE_COUNT] = {
    [ENDING_IMAGE_STORY_1] = { "ed01.pi", 640, 400 },
    [ENDING_IMAGE_STORY_2] = { "ed02.pi", 640, 400 },
    [ENDING_IMAGE_STORY_3] = { "ed03.pi", 640, 400 },
    [ENDING_IMAGE_STORY_4] = { "ed04.pi", 640, 400 },
    [ENDING_IMAGE_STORY_5] = { "ed05.pi", 640, 400 },
    [ENDING_IMAGE_CREDITS_1] = { "ed06.pi", 640, 400 },
    [ENDING_IMAGE_CREDITS_2] = { "ed07.pi", 640, 400 },
    [ENDING_IMAGE_CREDITS_3] = { "ed08.pi", 640, 400 },
    [ENDING_IMAGE_VERDICT] = { "ed09.pi", 640, 400 },
    [ENDING_IMAGE_ALL_CLEAR] = { "all.pi", 640, 400 },
    [ENDING_IMAGE_EXTRA_UNLOCK] = { "but.pi", 288, 160 },
    [ENDING_IMAGE_EXTRA_CLEAR] = { "extra.pi", 320, 160 },
};

static const char *const m_Colors[ENDING_COLOR_COUNT] = {
    [ENDING_COLOR_CREDITS_1_B] = "ed06b.rgb",
    [ENDING_COLOR_CREDITS_1_C] = "ed06c.rgb",
    [ENDING_COLOR_CREDITS_2_A] = "ed07a.rgb",
    [ENDING_COLOR_CREDITS_2_B] = "ed07b.rgb",
    [ENDING_COLOR_CREDITS_3_A] = "ed08a.rgb",
    [ENDING_COLOR_CREDITS_3_B] = "ed08b.rgb",
    [ENDING_COLOR_CREDITS_3_C] = "ed08c.rgb",
    [ENDING_COLOR_STORY_3] = "ed03a.rgb",
};

static RESULT M_Load(VFS *const files, ENDING_IMAGES *const out)
{
    for (size_t i = 0; i < ENDING_IMAGE_COUNT; i++) {
        READER reader;
        MUST(Vfs_Open(files, m_Images[i].path, VFS_IN_MEMORY, &reader));
        const RESULT result =
            Pi_Load(&reader, &out->allocator, &out->images[i]);
        Reader_Close(&reader);
        MUST(result, "%s", m_Images[i].path);
        FAIL_IF(
            out->images[i].width != m_Images[i].width
                || out->images[i].height != m_Images[i].height,
            "Invalid ending image dimensions: %s", m_Images[i].path);
    }
    for (size_t i = 0; i < ENDING_COLOR_COUNT; i++) {
        READER reader;
        MUST(Vfs_Open(files, m_Colors[i], VFS_IN_MEMORY, &reader));
        ENDING_COLOR_DATA *const color = &out->colors[i];
        const size_t available = Reader_Remaining(&reader);
        color->size = (uint8_t)(available < 48 ? available : 48);
        Reader_ReadBytes(&reader, color->bytes, color->size);
        const RESULT result = Reader_Result(&reader);
        Reader_Close(&reader);
        MUST(result, "%s", m_Colors[i]);
    }
    return OK;
}

RESULT EndingImages_Load(
    VFS *const files, const ALLOCATOR *const allocator,
    ENDING_IMAGES *const out)
{
    memset(out, 0, sizeof(*out));
    out->allocator = *allocator;
    const RESULT result = M_Load(files, out);
    if (!IS_OK(result)) {
        EndingImages_Free(out);
    }
    return result;
}

void EndingImages_Draw(
    const ENDING_IMAGES *const images, const ENDING_IMAGE image)
{
    ASSERT((uint32_t)image < ENDING_IMAGE_COUNT && g_Grcg->mode == 0);
    const PI_IMAGE *const source = &images->images[image];
    ASSERT(source->pixels != nullptr);
    Video_PutImage8(
        g_Clip, source->pixels, (int16_t)source->width, (int16_t)source->height,
        0, 0);
}

void EndingImages_ApplyPalette(
    const ENDING_IMAGES *const images, const ENDING_IMAGE image)
{
    ASSERT((uint32_t)image < ENDING_IMAGE_COUNT);
    Palette_Import(images->images[image].palette);
    Palette_Show();
}

void EndingImages_Recolor(
    const ENDING_IMAGES *const images, const ENDING_COLOR color)
{
    ASSERT((uint32_t)color < ENDING_COLOR_COUNT);
    READER reader;
    Reader_OpenBuffer(
        &reader, images->colors[color].bytes, images->colors[color].size,
        nullptr);
    const RESULT result = Palette_Recolor(&reader);
    Reader_Close(&reader);
    ASSERT(Result_Ignore(result));
    Palette_Show();
}

void EndingImages_Free(ENDING_IMAGES *const images)
{
    for (size_t i = 0; i < ENDING_IMAGE_COUNT; i++) {
        PI_IMAGE *const image = &images->images[i];
        if (image->pixels != nullptr) {
            Allocator_Free(
                &images->allocator, image->pixels,
                (size_t)image->width * image->height);
        }
    }
    memset(images, 0, sizeof(*images));
}
