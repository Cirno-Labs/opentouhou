#include "th07/anm/texture.h"

#include "th07/anm/private.h"
#include "th07/files.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    GFX_TEXTURE *texture;
    IMAGE image;
    ANM_TEXTURE_FORMAT format;
} M_TEXTURE;

static M_TEXTURE m_Textures[ANM_TEXTURE_COUNT];
static GFX_TEXTURE *m_Surfaces[ANM_SURFACE_COUNT];

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

static int32_t M_PixelSize(const ANM_TEXTURE_FORMAT format)
{
    switch (format) {
    case ANM_TEXTURE_RGBA8:
        return 4;
    case ANM_TEXTURE_RGB8:
        return 3;
    default:
        return 2;
    }
}

// Leaves the channels' low bits for quantizing to fill.
static void M_ReadPixel(
    READER *const reader, const ANM_TEXTURE_FORMAT format, uint8_t *const pixel)
{
    uint32_t value;
    switch (format) {
    case ANM_TEXTURE_RGBA8:
    case ANM_TEXTURE_RGB8:
        pixel[2] = Reader_ReadU8(reader);
        pixel[1] = Reader_ReadU8(reader);
        pixel[0] = Reader_ReadU8(reader);
        pixel[3] = format == ANM_TEXTURE_RGBA8 ? Reader_ReadU8(reader) : 255;
        break;
    case ANM_TEXTURE_A1R5G5B5:
        value = Reader_ReadU16(reader);
        pixel[0] = (uint8_t)((value >> 10 & 0x1f) << 3);
        pixel[1] = (uint8_t)((value >> 5 & 0x1f) << 3);
        pixel[2] = (uint8_t)((value & 0x1f) << 3);
        pixel[3] = (uint8_t)((value >> 15) << 7);
        break;
    case ANM_TEXTURE_R5G6B5:
        value = Reader_ReadU16(reader);
        pixel[0] = (uint8_t)((value >> 11) << 3);
        pixel[1] = (uint8_t)((value >> 5 & 0x3f) << 2);
        pixel[2] = (uint8_t)((value & 0x1f) << 3);
        pixel[3] = 255;
        break;
    default:
        value = Reader_ReadU16(reader);
        pixel[0] = (uint8_t)((value >> 8 & 0xf) << 4);
        pixel[1] = (uint8_t)((value >> 4 & 0xf) << 4);
        pixel[2] = (uint8_t)((value & 0xf) << 4);
        pixel[3] = (uint8_t)((value >> 12) << 4);
        break;
    }
}

static RESULT M_ReadName(
    READER *const reader, const uint32_t offset, const char **const name)
{
    FAIL_IF(
        offset >= Reader_Size(reader), "ANM image name is outside the file");
    Reader_Seek(reader, offset);
    *name = Reader_Borrow(reader, Reader_Remaining(reader));
    FAIL_IF(
        *name == nullptr
            || memchr(*name, '\0', Reader_Size(reader) - offset) == nullptr,
        "Unterminated ANM image name");
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

RESULT Anm_CreateTextureFromImage(
    const int32_t slot, READER *const reader, const ANM_TEXTURE_FORMAT format)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    Reader_Skip(reader, 6);
    const ANM_TEXTURE_FORMAT source =
        (ANM_TEXTURE_FORMAT)Reader_ReadI16(reader);
    const int32_t width = Reader_ReadI16(reader);
    const int32_t height = Reader_ReadI16(reader);
    Reader_Skip(reader, 4);
    MUST(Reader_Result(reader));
    FAIL_IF(
        source < ANM_TEXTURE_RGBA8 || source > ANM_TEXTURE_A4R4G4B4,
        "ANM image format %d is not supported", source);
    FAIL_IF(width <= 0 || height <= 0, "Invalid ANM image dimensions");
    FAIL_IF(
        (uint64_t)width * (uint64_t)height * (uint64_t)M_PixelSize(source)
            > Reader_Remaining(reader),
        "ANM image pixels run past the file");
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int64_t i = 0; i < (int64_t)width * height; i++) {
        M_ReadPixel(reader, source, &image.pixels[i * 4]);
    }
    M_Quantize(&image, source);
    M_Quantize(&image, format);
    return M_Store(slot, &image, format);
}

void Anm_ReleaseTexture(const int32_t slot)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    M_TEXTURE *const texture = &m_Textures[slot];
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
    Reader_Skip(reader, 8);
    const uint32_t image_offset = Reader_ReadU32(reader);
    const bool has_image = Reader_ReadU8(reader) != 0;
    MUST(Reader_Result(reader));
    FAIL_IF(
        format < ANM_TEXTURE_DEFAULT || format > ANM_TEXTURE_A4R4G4B4,
        "ANM texture format %d is not supported", format);
    if (has_image) {
        FAIL_IF(
            image_offset >= Reader_Size(reader),
            "ANM image is outside the file");
        Reader_Seek(reader, image_offset);
        return Anm_CreateTextureFromImage(slot, reader, format);
    }
    FAIL_IF(name_offset == 0, "ANM names no image");
    const char *name;
    MUST(M_ReadName(reader, name_offset, &name));
    if (name[0] == '@') {
        MUST(Anm_CreateTexture(slot, width, height, format));
    } else {
        MUST(Anm_LoadTexture(slot, name, format, color_key));
    }
    if (alpha_offset != 0) {
        const char *alpha;
        MUST(M_ReadName(reader, alpha_offset, &alpha));
        // As thcrap, a patched image keeps its alpha unless the mask is too.
        if (!Files_IsPatched(name) || Files_IsPatched(alpha)) {
            MUST(Anm_LoadTextureAlpha(slot, alpha, format, color_key));
        }
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
