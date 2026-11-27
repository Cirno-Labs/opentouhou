#include "th07/anm/texture.h"

#include "th07/anm/private.h"
#include "th07/files.h"

#include "core/base/assert.h"
#include "core/base/utils.h"
#include "core/vfs.h"
#include "shell/game.h"

#include <math.h>
#include <string.h>

typedef struct {
    GFX_TEXTURE *texture;
    IMAGE image;
    ANM_TEXTURE_FORMAT format;
} M_TEXTURE;

typedef struct {
    int32_t slot; // -1 while no copy is pending
    GFX_RECT source;
    GFX_RECT dest;
} M_PENDING_COPY;

typedef struct {
    READER *anm;
    M_TEXTURE *texture;
} M_IMAGE_PATCH;

static M_TEXTURE m_Textures[ANM_TEXTURE_COUNT];
static GFX_TEXTURE *m_Surfaces[ANM_SURFACE_COUNT];
static M_PENDING_COPY m_PendingCopy = { .slot = -1 };

static IMAGE_DEPTH M_Depth(const ANM_TEXTURE_FORMAT format)
{
    switch (format) {
    case ANM_TEXTURE_A1R5G5B5:
        return IMAGE_DEPTH_A1R5G5B5;
    case ANM_TEXTURE_A4R4G4B4:
        return IMAGE_DEPTH_A4R4G4B4;
    case ANM_TEXTURE_R5G6B5:
        return IMAGE_DEPTH_R5G6B5;
    case ANM_TEXTURE_RGB8:
        return IMAGE_DEPTH_RGB8;
    default:
        return IMAGE_DEPTH_RGBA8;
    }
}

static void M_Quantize(IMAGE *const image, const ANM_TEXTURE_FORMAT format)
{
    Image_Quantize(image, M_Depth(format));
}

static void M_QuantizePatch(IMAGE *const image, const ANM_TEXTURE_FORMAT format)
{
    if (format == ANM_TEXTURE_A4R4G4B4) {
        for (int64_t i = 0; i < (int64_t)image->width * image->height * 4;
             i++) {
            image->pixels[i] = (uint8_t)((image->pixels[i] >> 4) * 17);
        }
    } else if (format == ANM_TEXTURE_R5G6B5) {
        for (int64_t i = 0; i < (int64_t)image->width * image->height; i++) {
            uint8_t *const pixel = image->pixels + i * 4;
            const uint8_t r = pixel[0] >> 3;
            const uint8_t g = pixel[1] >> 2;
            const uint8_t b = pixel[2] >> 3;
            pixel[0] = (uint8_t)((r << 3) | (r >> 2));
            pixel[1] = (uint8_t)((g << 2) | (g >> 4));
            pixel[2] = (uint8_t)((b << 3) | (b >> 2));
            pixel[3] = 255;
        }
    } else {
        M_Quantize(image, format);
    }
}

static RESULT M_Decode(
    const char *const path, const uint32_t color_key, IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader));
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

static uint8_t M_Expand(const uint32_t value, const int32_t bits)
{
    return (uint8_t)((value << (8 - bits)) | (value >> (bits * 2 - 8)));
}

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
        pixel[0] = M_Expand(value >> 10 & 0x1f, 5);
        pixel[1] = M_Expand(value >> 5 & 0x1f, 5);
        pixel[2] = M_Expand(value & 0x1f, 5);
        pixel[3] = (uint8_t)((value >> 15) * 255);
        break;
    case ANM_TEXTURE_R5G6B5:
        value = Reader_ReadU16(reader);
        pixel[0] = M_Expand(value >> 11, 5);
        pixel[1] = M_Expand(value >> 5 & 0x3f, 6);
        pixel[2] = M_Expand(value & 0x1f, 5);
        pixel[3] = 255;
        break;
    default:
        value = Reader_ReadU16(reader);
        pixel[0] = (uint8_t)((value >> 8 & 0xf) * 17);
        pixel[1] = (uint8_t)((value >> 4 & 0xf) * 17);
        pixel[2] = (uint8_t)((value & 0xf) * 17);
        pixel[3] = (uint8_t)((value >> 12) * 17);
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

static void M_PatchRegion(
    M_TEXTURE *const texture, const IMAGE *const patch, const int32_t x,
    const int32_t y, const int32_t width, const int32_t height)
{
    bool empty = true;
    bool opaque = true;
    IMAGE *const image = &texture->image;
    for (int32_t row = y; row < y + height; row++) {
        for (int32_t col = x; col < x + width; col++) {
            const size_t dst = ((size_t)row * image->width + col) * 4;
            const size_t src = ((size_t)row * patch->width + col) * 4;
            empty &= patch->pixels[src + 3] == 0;
            opaque &= image->pixels[dst + 3] == 255;
        }
    }
    if (empty) {
        return;
    }
    const bool blend = opaque
        && (texture->format == ANM_TEXTURE_RGBA8
            || texture->format == ANM_TEXTURE_A4R4G4B4);
    const int32_t shift = texture->format == ANM_TEXTURE_A4R4G4B4 ? 4 : 8;
    const int32_t quantum = shift == 4 ? 17 : 1;
    const int32_t maximum = shift == 4 ? 15 : 255;
    for (int32_t row = y; row < y + height; row++) {
        for (int32_t col = x; col < x + width; col++) {
            uint8_t *const dst =
                image->pixels + ((size_t)row * image->width + col) * 4;
            const uint8_t *const src =
                patch->pixels + ((size_t)row * patch->width + col) * 4;
            if (!blend) {
                memcpy(dst, src, 4);
                continue;
            }
            const int32_t alpha = src[3] / quantum;
            for (int32_t channel = 0; channel < 3; channel++) {
                dst[channel] =
                    (uint8_t)(((dst[channel] / quantum * (maximum - alpha)
                                + src[channel] / quantum * alpha)
                               >> shift)
                              * quantum);
            }
            dst[3] = (uint8_t)MIN((int32_t)dst[3] + src[3], 255);
        }
    }
}

static RESULT M_PatchSprites(
    const M_IMAGE_PATCH *const context, const IMAGE *const patch)
{
    READER *const reader = context->anm;
    M_TEXTURE *const texture = context->texture;
    const int32_t width = MIN(texture->image.width, patch->width);
    const int32_t height = MIN(texture->image.height, patch->height);
    Reader_Seek(reader, 0);
    const int32_t count = Reader_ReadI32(reader);
    if (count == 0) {
        M_PatchRegion(texture, patch, 0, 0, width, height);
    }
    for (int32_t i = 0; i < count; i++) {
        Reader_Seek(reader, 0x40 + (size_t)i * 4);
        const uint32_t offset = Reader_ReadU32(reader);
        Reader_Seek(reader, (size_t)offset + 4);
        const float x = Reader_ReadF32(reader);
        const float y = Reader_ReadF32(reader);
        const float w = Reader_ReadF32(reader);
        const float h = Reader_ReadF32(reader);
        MUST(Reader_Result(reader));
        FAIL_IF(
            !isfinite(x) || !isfinite(y) || !isfinite(w) || !isfinite(h),
            "Invalid patch sprite rectangle");
        const int32_t left = (int32_t)MIN(MAX(x, 0.0f), (float)width);
        const int32_t top = (int32_t)MIN(MAX(y, 0.0f), (float)height);
        const int32_t right = (int32_t)MIN(MAX(x + w, 0.0f), (float)width);
        const int32_t bottom = (int32_t)MIN(MAX(y + h, 0.0f), (float)height);
        if (right > left && bottom > top) {
            M_PatchRegion(
                texture, patch, left, top, right - left, bottom - top);
        }
    }
    return OK;
}

static RESULT M_PatchImage(void *const user, READER *const reader)
{
    M_IMAGE_PATCH *const context = user;
    IMAGE patch;
    MUST(Image_Decode(reader, &g_HeapAllocator, &patch));
    M_QuantizePatch(&patch, context->texture->format);
    const RESULT result = M_PatchSprites(context, &patch);
    Image_Free(&patch);
    return result;
}

static RESULT M_CreateTextureFromImage(
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
    return M_Store(
        slot, &image, format == ANM_TEXTURE_DEFAULT ? source : format);
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

IMAGE_DEPTH Anm_GetTextureDepth(const int32_t slot)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    return M_Depth(m_Textures[slot].format);
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
    IMAGE pixels;
    MUST(Image_Crop(
        image, 0, 0, image->width, image->height, &g_HeapAllocator, &pixels));
    M_Quantize(&pixels, texture->format);
    GfxTexture_Upload(texture->texture, rectangle, &pixels);
    const RESULT result =
        Image_Paste(&texture->image, rectangle.x, rectangle.y, &pixels);
    Image_Free(&pixels);
    return result;
}

RESULT Anm_CopyTextureRect(
    const int32_t dest_slot, const GFX_RECT dest, const int32_t source_slot,
    const GFX_RECT source)
{
    ASSERT(dest_slot >= 0 && dest_slot < ANM_TEXTURE_COUNT);
    ASSERT(source_slot >= 0 && source_slot < ANM_TEXTURE_COUNT);
    const M_TEXTURE *const from = &m_Textures[source_slot];
    if (m_Textures[dest_slot].texture == nullptr || from->texture == nullptr) {
        return OK;
    }
    FAIL_IF(
        source.x < 0 || source.y < 0 || source.width <= 0 || source.height <= 0
            || (int64_t)source.x + source.width > from->image.width
            || (int64_t)source.y + source.height > from->image.height
            || dest.width <= 0 || dest.height <= 0,
        "Invalid ANM texture copy rectangle");
    IMAGE pixels;
    MUST(Image_Crop(
        &from->image, source.x, source.y, source.width, source.height,
        &g_HeapAllocator, &pixels));
    if (source.width != dest.width || source.height != dest.height) {
        Image_Resize(&pixels, dest.width, dest.height);
    }
    const RESULT result = Anm_UpdateTexture(dest_slot, dest, &pixels);
    Image_Free(&pixels);
    return result;
}

bool Anm_SetPendingCopy(
    const int32_t slot, const GFX_RECT source, const GFX_RECT dest)
{
    ASSERT(slot >= 0 && slot < ANM_TEXTURE_COUNT);
    if (m_PendingCopy.slot >= 0) {
        return false;
    }
    m_PendingCopy =
        (M_PENDING_COPY) { .slot = slot, .source = source, .dest = dest };
    return true;
}

RESULT Anm_DoPendingCopy(void)
{
    const M_PENDING_COPY copy = m_PendingCopy;
    m_PendingCopy.slot = -1;
    if (copy.slot < 0 || m_Textures[copy.slot].texture == nullptr) {
        return OK;
    }
    FAIL_IF(
        copy.dest.width <= 0 || copy.dest.height <= 0,
        "Invalid screen copy rectangle");
    IMAGE pixels;
    MUST(Gfx_ReadScreenRect(copy.source, &g_HeapAllocator, &pixels));
    if (copy.source.width != copy.dest.width
        || copy.source.height != copy.dest.height) {
        Image_Resize(&pixels, copy.dest.width, copy.dest.height);
    }
    Image_QuantizeDithered(&pixels, Anm_GetTextureDepth(copy.slot));
    const RESULT result = Anm_UpdateTexture(copy.slot, copy.dest, &pixels);
    Image_Free(&pixels);
    return result;
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
    const char *name = nullptr;
    if (name_offset != 0) {
        MUST(M_ReadName(reader, name_offset, &name));
    }
    if (has_image) {
        FAIL_IF(
            image_offset >= Reader_Size(reader),
            "ANM image is outside the file");
        Reader_Seek(reader, image_offset);
        MUST(M_CreateTextureFromImage(slot, reader, format));
        if (name != nullptr) {
            M_IMAGE_PATCH context = { reader, &m_Textures[slot] };
            MUST(Files_VisitPatches(name, M_PatchImage, &context));
            const IMAGE *const image = &m_Textures[slot].image;
            GfxTexture_Upload(
                m_Textures[slot].texture,
                (GFX_RECT) { 0, 0, image->width, image->height }, image);
        }
        return OK;
    }
    FAIL_IF(name == nullptr, "ANM names no image");
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
