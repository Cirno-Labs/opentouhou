#include "th06/anm/texture.h"

#include "th06/anm/private.h"
#include "th06/files.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    GFX_TEXTURE *texture;
    IMAGE image;
    ANM_TEXTURE_FORMAT format;
} M_TEXTURE;

static M_TEXTURE m_Textures[ANM_TEXTURE_COUNT];
static GFX_TEXTURE *m_Surfaces[ANM_SURFACE_COUNT];
static int32_t m_PendingTexture = -1;
static GFX_RECT m_PendingRectangle;

static void M_Quantize(IMAGE *const image, const ANM_TEXTURE_FORMAT format)
{
    switch (format) {
    case ANM_TEXTURE_A1R5G5B5:
        Image_Quantize(image, IMAGE_DEPTH_A1R5G5B5);
        break;
    case ANM_TEXTURE_R5G6B5:
        Image_Quantize(image, IMAGE_DEPTH_R5G6B5);
        break;
    case ANM_TEXTURE_A4R4G4B4:
        Image_Quantize(image, IMAGE_DEPTH_A4R4G4B4);
        break;
    case ANM_TEXTURE_RGB8:
        for (int64_t i = 0; i < (int64_t)image->width * image->height; i++) {
            image->pixels[i * 4 + 3] = 255;
        }
        break;
    default:
        break;
    }
}

static RESULT M_Decode(
    const char *const path, const uint32_t color_key, IMAGE *const out)
{
    READER reader;
    MUST(Files_Load(path, &reader));
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, out);
    Reader_Close(&reader);
    MUST(result);
    if (color_key != 0) {
        Image_ColorKey(out, color_key & 0xffffff);
    }
    return OK;
}

static RESULT M_Store(
    const int32_t slot, IMAGE *const image, const ANM_TEXTURE_FORMAT format)
{
    GFX_TEXTURE *texture = nullptr;
    const RESULT result = GfxTexture_Create(image, &texture);
    if (!IS_OK(result)) {
        Image_Free(image);
        return result;
    }
    Anm_ReleaseTexture(slot);
    m_Textures[slot] =
        (M_TEXTURE) { .texture = texture, .image = *image, .format = format };
    return OK;
}

RESULT Anm_LoadTexture(
    const int32_t slot, const char *const path, const ANM_TEXTURE_FORMAT format,
    const uint32_t color_key)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    IMAGE image;
    MUST(M_Decode(path, color_key, &image));
    M_Quantize(&image, format);
    return M_Store(slot, &image, format);
}

RESULT Anm_LoadTextureAlpha(
    const int32_t slot, const char *const path, const ANM_TEXTURE_FORMAT format,
    const uint32_t color_key)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    M_TEXTURE *const texture = &m_Textures[slot];
    FAIL_IF(texture->texture == nullptr, "ANM texture %d is not loaded", slot);
    FAIL_IF(
        format == ANM_TEXTURE_R5G6B5 || format == ANM_TEXTURE_RGB8,
        "ANM texture has no alpha channel");
    IMAGE alpha;
    MUST(M_Decode(path, color_key, &alpha));
    M_Quantize(&alpha, format);
    RESULT result = Image_AlphaFrom(&texture->image, &alpha);
    Image_Free(&alpha);
    if (IS_OK(result)) {
        M_Quantize(&texture->image, format);
        GfxTexture_Upload(
            texture->texture,
            (GFX_RECT) { 0, 0, texture->image.width, texture->image.height },
            &texture->image);
    }
    return result;
}

RESULT Anm_CreateTexture(
    const int32_t slot, const int32_t width, const int32_t height,
    const ANM_TEXTURE_FORMAT format)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    FAIL_IF(
        width <= 0 || height <= 0 || width > 16384 || height > 16384,
        "Invalid ANM texture dimensions");
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    M_Quantize(&image, format);
    return M_Store(slot, &image, format);
}

void Anm_ReleaseTexture(const int32_t slot)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    M_TEXTURE *const texture = &m_Textures[slot];
    if (m_PendingTexture == slot) {
        m_PendingTexture = -1;
    }
    GfxTexture_Free(texture->texture);
    if (texture->image.pixels != nullptr) {
        Image_Free(&texture->image);
    }
    *texture = (M_TEXTURE) {};
}

GFX_TEXTURE *Anm_GetTexture(const int32_t slot)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    return m_Textures[slot].texture;
}

RESULT Anm_UpdateTexture(
    const int32_t slot, const GFX_RECT rectangle, const IMAGE *const image)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    FAIL_IF(m_Textures[slot].texture == nullptr, "ANM texture is not loaded");
    M_TEXTURE *const texture = &m_Textures[slot];
    FAIL_IF(
        image == nullptr || image->pixels == nullptr
            || image->width != rectangle.width
            || image->height != rectangle.height || rectangle.x < 0
            || rectangle.y < 0 || rectangle.width <= 0 || rectangle.height <= 0
            || (int64_t)rectangle.x + rectangle.width > texture->image.width
            || (int64_t)rectangle.y + rectangle.height > texture->image.height,
        "Invalid ANM texture update rectangle");
    IMAGE pixels = Image_Create(image->width, image->height, &g_HeapAllocator);
    memcpy(
        pixels.pixels, image->pixels,
        (size_t)image->width * (size_t)image->height * 4);
    M_Quantize(&pixels, texture->format);
    GfxTexture_Upload(texture->texture, rectangle, &pixels);
    for (int32_t row = 0; row < image->height; row++) {
        memcpy(
            texture->image.pixels
                + ((size_t)(rectangle.y + row) * (size_t)texture->image.width
                   + (size_t)rectangle.x)
                    * 4,
            pixels.pixels + (size_t)row * (size_t)image->width * 4,
            (size_t)image->width * 4);
    }
    Image_Free(&pixels);
    return OK;
}

const IMAGE *AnmTexture_GetImage(const int32_t slot)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    return &m_Textures[slot].image;
}

RESULT AnmTexture_LoadSlot(READER *const reader, const int32_t slot)
{
    Reader_Seek(reader, 12);
    const int32_t width = Reader_ReadI32(reader);
    const int32_t height = Reader_ReadI32(reader);
    const ANM_TEXTURE_FORMAT format =
        (ANM_TEXTURE_FORMAT)Reader_ReadI32(reader);
    const uint32_t color_key = Reader_ReadU32(reader);
    const uint32_t name_offset = Reader_ReadU32(reader);
    Reader_Skip(reader, 4);
    const uint32_t alpha_offset = Reader_ReadU32(reader);
    MUST(Reader_Result(reader));
    if (name_offset == 0) {
        return OK;
    }
    FAIL_IF(
        name_offset >= Reader_Size(reader),
        "ANM image name is outside the file");
    Reader_Seek(reader, name_offset);
    const char *const name = Reader_Borrow(reader, Reader_Remaining(reader));
    FAIL_IF(
        memchr(name, '\0', Reader_Size(reader) - name_offset) == nullptr,
        "Unterminated ANM image name");
    if (name[0] == '@') {
        MUST(Anm_CreateTexture(slot, width, height, format));
    } else {
        MUST(Anm_LoadTexture(slot, name, format, color_key));
    }
    if (alpha_offset != 0) {
        FAIL_IF(
            alpha_offset >= Reader_Size(reader),
            "ANM alpha image name is outside the file");
        Reader_Seek(reader, alpha_offset);
        const char *const alpha =
            Reader_Borrow(reader, Reader_Remaining(reader));
        FAIL_IF(
            memchr(alpha, '\0', Reader_Size(reader) - alpha_offset) == nullptr,
            "Unterminated ANM alpha image name");
        MUST(Anm_LoadTextureAlpha(slot, alpha, format, color_key));
    }
    return OK;
}

RESULT Anm_LoadSurface(const int32_t surface, const char *const path)
{
    ASSERT(surface >= 0 && surface < ANM_SURFACE_COUNT);
    IMAGE image;
    MUST(M_Decode(path, 0, &image));
    GFX_TEXTURE *texture = nullptr;
    const RESULT result = GfxTexture_Create(&image, &texture);
    Image_Free(&image);
    MUST(result);
    Anm_ReleaseSurface(surface);
    m_Surfaces[surface] = texture;
    return OK;
}

void Anm_ReleaseSurface(const int32_t surface)
{
    ASSERT(surface >= 0 && surface < ANM_SURFACE_COUNT);
    GfxTexture_Free(m_Surfaces[surface]);
    m_Surfaces[surface] = nullptr;
}

void Anm_CopySurfaceRectToBackBuffer(
    const int32_t surface, const int32_t x, const int32_t y,
    const GFX_RECT rectangle)
{
    ASSERT(surface >= 0 && surface < ANM_SURFACE_COUNT);
    GFX_TEXTURE *const texture = m_Surfaces[surface];
    if (texture == nullptr) {
        return;
    }
    const float width = (float)GfxTexture_GetWidth(texture);
    const float height = (float)GfxTexture_GetHeight(texture);
    const float u0 = (float)rectangle.x / width;
    const float v0 = (float)rectangle.y / height;
    const float u1 = (float)(rectangle.x + rectangle.width) / width;
    const float v1 = (float)(rectangle.y + rectangle.height) / height;
    const float x0 = (float)x;
    const float y0 = (float)y;
    const float x1 = (float)(x + rectangle.width);
    const float y1 = (float)(y + rectangle.height);
    const GFX_COLOR white = { 255, 255, 255, 255 };
    const GFX_VERTEX vertices[4] = {
        { { x0, y0, 0.0f }, white, { u0, v0 } },
        { { x1, y0, 0.0f }, white, { u1, v0 } },
        { { x1, y1, 0.0f }, white, { u1, v1 } },
        { { x0, y1, 0.0f }, white, { u0, v1 } },
    };
    const GFX_VIEW view = GfxView_Screen();
    Gfx_SetView(&view);
    Gfx_DrawQuad(&(GFX_STATE) {}, texture, vertices, 0);
}

void Anm_CopySurfaceToBackBuffer(
    const int32_t surface, const int32_t left, const int32_t top,
    const int32_t x, const int32_t y)
{
    ASSERT(surface >= 0 && surface < ANM_SURFACE_COUNT);
    if (m_Surfaces[surface] != nullptr) {
        Anm_CopySurfaceRectToBackBuffer(
            surface, x, y,
            (GFX_RECT) { left, top,
                         GfxTexture_GetWidth(m_Surfaces[surface]) - left,
                         GfxTexture_GetHeight(m_Surfaces[surface]) - top });
    }
}

RESULT Anm_CopyBackBufferToTexture(const int32_t slot, const GFX_RECT rectangle)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    if (m_Textures[slot].texture == nullptr) {
        return OK;
    }
    GFX_TEXTURE *copy = nullptr;
    MUST(GfxTexture_CreateEmpty(rectangle.width, rectangle.height, &copy));
    GfxTexture_CopyFromScreen(copy, rectangle, 0, 0);
    GfxTexture_Free(m_Textures[slot].texture);
    m_Textures[slot].texture = copy;
    return OK;
}

void Anm_SetPendingCopy(const int32_t slot, const GFX_RECT rectangle)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    m_PendingTexture = slot;
    m_PendingRectangle = rectangle;
}

RESULT Anm_DoPendingCopy(void)
{
    if (m_PendingTexture < 0) {
        return OK;
    }
    const int32_t slot = m_PendingTexture;
    m_PendingTexture = -1;
    return Anm_CopyBackBufferToTexture(slot, m_PendingRectangle);
}
