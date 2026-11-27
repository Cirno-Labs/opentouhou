#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/gfx/private.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define M_LOG_SIZE 2048
// Indexes the samplers: bit 0 sets repeat, bit 1 the linear filter for
// textures drawn smaller than their size and bit 2 for larger.
#define M_SAMPLER_REPEAT 1
#define M_SAMPLER_LINEAR_MIN 2
#define M_SAMPLER_LINEAR_MAG 4
#define M_SAMPLER_COUNT 8

// Lists the OpenGL functions the backend loads, by their name after "gl",
// the field that holds them, their return type and their parameters.
#define M_GL_FUNCTIONS(X)                                                      \
    X(GetString, get_string, const GLubyte *, (GLenum name))                   \
    X(GetError, get_error, GLenum, (void))                                     \
    X(Viewport, viewport, void, (GLint x, GLint y, GLsizei w, GLsizei h))      \
    X(Scissor, scissor, void, (GLint x, GLint y, GLsizei w, GLsizei h))        \
    X(Enable, enable, void, (GLenum cap))                                      \
    X(Disable, disable, void, (GLenum cap))                                    \
    X(ClearColor, clear_color, void,                                           \
      (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                            \
    X(Clear, clear, void, (GLbitfield mask))                                   \
    X(DepthFunc, depth_func, void, (GLenum func))                              \
    X(DepthMask, depth_mask, void, (GLboolean flag))                           \
    X(ColorMask, color_mask, void,                                             \
      (GLboolean r, GLboolean g, GLboolean b, GLboolean a))                    \
    X(BlendFunc, blend_func, void, (GLenum source, GLenum destination))        \
    X(PixelStorei, pixel_storei, void, (GLenum name, GLint value))             \
    X(GenTextures, gen_textures, void, (GLsizei n, GLuint * textures))         \
    X(DeleteTextures, delete_textures, void,                                   \
      (GLsizei n, const GLuint *textures))                                     \
    X(BindTexture, bind_texture, void, (GLenum target, GLuint texture))        \
    X(ActiveTexture, active_texture, void, (GLenum unit))                      \
    X(TexImage2D, tex_image_2d, void,                                          \
      (GLenum target, GLint level, GLint format, GLsizei w, GLsizei h,         \
       GLint border, GLenum source_format, GLenum type, const void *pixels))   \
    X(TexSubImage2D, tex_sub_image_2d, void,                                   \
      (GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,     \
       GLenum format, GLenum type, const void *pixels))                        \
    X(TexParameteri, tex_parameteri, void,                                     \
      (GLenum target, GLenum name, GLint value))                               \
    X(CopyTexSubImage2D, copy_tex_sub_image_2d, void,                          \
      (GLenum target, GLint level, GLint x, GLint y, GLint source_x,           \
       GLint source_y, GLsizei w, GLsizei h))                                  \
    X(GenSamplers, gen_samplers, void, (GLsizei n, GLuint * samplers))         \
    X(DeleteSamplers, delete_samplers, void,                                   \
      (GLsizei n, const GLuint *samplers))                                     \
    X(SamplerParameteri, sampler_parameteri, void,                             \
      (GLuint sampler, GLenum name, GLint value))                              \
    X(BindSampler, bind_sampler, void, (GLuint unit, GLuint sampler))          \
    X(GenFramebuffers, gen_framebuffers, void, (GLsizei n, GLuint * ids))      \
    X(DeleteFramebuffers, delete_framebuffers, void,                           \
      (GLsizei n, const GLuint *ids))                                          \
    X(BindFramebuffer, bind_framebuffer, void, (GLenum target, GLuint id))     \
    X(FramebufferTexture2D, framebuffer_texture_2d, void,                      \
      (GLenum target, GLenum attachment, GLenum texture_target,                \
       GLuint texture, GLint level))                                           \
    X(FramebufferRenderbuffer, framebuffer_renderbuffer, void,                 \
      (GLenum target, GLenum attachment, GLenum renderbuffer_target,           \
       GLuint renderbuffer))                                                   \
    X(CheckFramebufferStatus, check_framebuffer_status, GLenum,                \
      (GLenum target))                                                         \
    X(BlitFramebuffer, blit_framebuffer, void,                                 \
      (GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0,       \
       GLint dx1, GLint dy1, GLbitfield mask, GLenum filter))                  \
    X(GenRenderbuffers, gen_renderbuffers, void, (GLsizei n, GLuint * ids))    \
    X(DeleteRenderbuffers, delete_renderbuffers, void,                         \
      (GLsizei n, const GLuint *ids))                                          \
    X(BindRenderbuffer, bind_renderbuffer, void, (GLenum target, GLuint id))   \
    X(RenderbufferStorage, renderbuffer_storage, void,                         \
      (GLenum target, GLenum format, GLsizei w, GLsizei h))                    \
    X(CreateShader, create_shader, GLuint, (GLenum type))                      \
    X(ShaderSource, shader_source, void,                                       \
      (GLuint shader, GLsizei count, const GLchar *const *strings,             \
       const GLint *lengths))                                                  \
    X(CompileShader, compile_shader, void, (GLuint shader))                    \
    X(GetShaderiv, get_shaderiv, void,                                         \
      (GLuint shader, GLenum name, GLint * value))                             \
    X(GetShaderInfoLog, get_shader_info_log, void,                             \
      (GLuint shader, GLsizei size, GLsizei * length, GLchar * log))           \
    X(DeleteShader, delete_shader, void, (GLuint shader))                      \
    X(CreateProgram, create_program, GLuint, (void))                           \
    X(AttachShader, attach_shader, void, (GLuint program, GLuint shader))      \
    X(LinkProgram, link_program, void, (GLuint program))                       \
    X(GetProgramiv, get_programiv, void,                                       \
      (GLuint program, GLenum name, GLint * value))                            \
    X(GetProgramInfoLog, get_program_info_log, void,                           \
      (GLuint program, GLsizei size, GLsizei * length, GLchar * log))          \
    X(DeleteProgram, delete_program, void, (GLuint program))                   \
    X(UseProgram, use_program, void, (GLuint program))                         \
    X(GetUniformLocation, get_uniform_location, GLint,                         \
      (GLuint program, const GLchar *name))                                    \
    X(Uniform1i, uniform_1i, void, (GLint location, GLint value))              \
    X(Uniform2f, uniform_2f, void, (GLint location, GLfloat x, GLfloat y))     \
    X(Uniform3f, uniform_3f, void,                                             \
      (GLint location, GLfloat x, GLfloat y, GLfloat z))                       \
    X(Uniform4f, uniform_4f, void,                                             \
      (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w))            \
    X(UniformMatrix4fv, uniform_matrix_4fv, void,                              \
      (GLint location, GLsizei count, GLboolean transpose,                     \
       const GLfloat *values))                                                 \
    X(GenVertexArrays, gen_vertex_arrays, void, (GLsizei n, GLuint * ids))     \
    X(DeleteVertexArrays, delete_vertex_arrays, void,                          \
      (GLsizei n, const GLuint *ids))                                          \
    X(BindVertexArray, bind_vertex_array, void, (GLuint id))                   \
    X(GenBuffers, gen_buffers, void, (GLsizei n, GLuint * ids))                \
    X(DeleteBuffers, delete_buffers, void, (GLsizei n, const GLuint *ids))     \
    X(BindBuffer, bind_buffer, void, (GLenum target, GLuint id))               \
    X(BufferData, buffer_data, void,                                           \
      (GLenum target, GLsizeiptr size, const void *data, GLenum usage))        \
    X(EnableVertexAttribArray, enable_vertex_attrib_array, void,               \
      (GLuint index))                                                          \
    X(VertexAttribPointer, vertex_attrib_pointer, void,                        \
      (GLuint index, GLint size, GLenum type, GLboolean normalized,            \
       GLsizei stride, const void *offset))                                    \
    X(DrawElements, draw_elements, void,                                       \
      (GLenum mode, GLsizei count, GLenum type, const void *offset))           \
    X(ReadPixels, read_pixels, void,                                           \
      (GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type,     \
       void *pixels))

typedef struct {
#define M_GL_FIELD(name_, field_, result_, params_)                            \
    result_(APIENTRY *field_) params_;
    M_GL_FUNCTIONS(M_GL_FIELD)
#undef M_GL_FIELD
    void(APIENTRY *clear_depth)(GLdouble depth);
    void(APIENTRY *clear_depthf)(GLfloat depth);
} M_GL;

typedef struct {
    GLuint framebuffer;
    GLuint color;
    GLuint depth;
    int32_t width;
    int32_t height;
    bool drawn;
} M_TARGET;

typedef struct {
    GLint view;
    GLint projection;
    GLint output;
    GLint texture;
    GLint palette;
    GLint indexed;
    GLint textured;
    GLint color_add;
    GLint alpha_test;
    GLint fog;
    GLint fog_range;
    GLint fog_color;
} M_LOCATIONS;

typedef struct {
    M_GL gl;
    bool es;
    GLuint program;
    GLuint vertex_array;
    GLuint vertex_buffer;
    GLuint index_buffer;
    GLuint rect_buffer;
    int32_t index_quads;
    int32_t rect_quads;
    GLuint samplers[M_SAMPLER_COUNT];
    M_LOCATIONS locations;
    M_TARGET screen;
    M_TARGET output;
    M_TARGET history;
    M_TARGET *target;
    GFX_PASS pass;
    GFX_PASS_OUTPUT pass_output;
    uint32_t *indices;
    // Holds each vertex's texel rect: min u, min v, max u, max v.
    float (*rects)[4];
} M_BACKEND;

typedef struct {
    GLuint pixels;
    GLuint palette;
} M_TEXTURE;

static M_BACKEND m_Backend;

#include "core/gfx_shaders.h"

static RESULT M_LoadFunctions(void)
{
#define M_GL_LOAD(name_, field_, result_, params_)                             \
    m_Backend.gl.field_ =                                                      \
        (typeof(m_Backend.gl.field_))SDL_GL_GetProcAddress("gl" #name_);       \
    FAIL_IF(                                                                   \
        m_Backend.gl.field_ == nullptr, "Cannot load gl" #name_ ": %s",        \
        SDL_GetError());
    M_GL_FUNCTIONS(M_GL_LOAD)
#undef M_GL_LOAD
    m_Backend.gl.clear_depth =
        (typeof(m_Backend.gl.clear_depth))SDL_GL_GetProcAddress("glClearDepth");
    m_Backend.gl.clear_depthf =
        (typeof(m_Backend.gl.clear_depthf))SDL_GL_GetProcAddress(
            "glClearDepthf");
    return OK;
}

static RESULT M_CompileShader(
    const GLenum type, const char *const body, GLuint *const out)
{
    const M_GL *const gl = &m_Backend.gl;
    const char *const sources[] = {
        m_Backend.es ? "#version 300 es\n" : "#version 330 core\n",
        body,
    };
    const GLuint shader = gl->create_shader(type);
    gl->shader_source(shader, 2, sources, nullptr);
    gl->compile_shader(shader);
    GLint compiled = GL_FALSE;
    gl->get_shaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char log[M_LOG_SIZE] = {};
        gl->get_shader_info_log(shader, sizeof(log), nullptr, log);
        gl->delete_shader(shader);
        return FAIL("Cannot compile a shader: %s", log);
    }
    *out = shader;
    return OK;
}

static RESULT M_CreateProgram(void)
{
    const M_GL *const gl = &m_Backend.gl;
    GLuint vertex;
    GLuint fragment;
    MUST(M_CompileShader(GL_VERTEX_SHADER, m_VertexShader, &vertex));
    const RESULT result =
        M_CompileShader(GL_FRAGMENT_SHADER, m_FragmentShader, &fragment);
    if (!IS_OK(result)) {
        gl->delete_shader(vertex);
        return result;
    }
    const GLuint program = gl->create_program();
    gl->attach_shader(program, vertex);
    gl->attach_shader(program, fragment);
    gl->link_program(program);
    gl->delete_shader(vertex);
    gl->delete_shader(fragment);
    GLint linked = GL_FALSE;
    gl->get_programiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[M_LOG_SIZE] = {};
        gl->get_program_info_log(program, sizeof(log), nullptr, log);
        gl->delete_program(program);
        return FAIL("Cannot link the shaders: %s", log);
    }
    m_Backend.program = program;
    M_LOCATIONS *const at = &m_Backend.locations;
    at->view = gl->get_uniform_location(program, "u_view");
    at->projection = gl->get_uniform_location(program, "u_projection");
    at->output = gl->get_uniform_location(program, "u_output");
    at->texture = gl->get_uniform_location(program, "u_texture");
    at->palette = gl->get_uniform_location(program, "u_palette");
    at->indexed = gl->get_uniform_location(program, "u_indexed");
    at->textured = gl->get_uniform_location(program, "u_textured");
    at->color_add = gl->get_uniform_location(program, "u_color_add");
    at->alpha_test = gl->get_uniform_location(program, "u_alpha_test");
    at->fog = gl->get_uniform_location(program, "u_fog");
    at->fog_range = gl->get_uniform_location(program, "u_fog_range");
    at->fog_color = gl->get_uniform_location(program, "u_fog_color");
    gl->use_program(program);
    gl->uniform_1i(at->texture, 0);
    gl->uniform_1i(at->palette, 1);
    return OK;
}

static void M_FreeTarget(M_TARGET *const target)
{
    const M_GL *const gl = &m_Backend.gl;
    if (target->framebuffer != 0) {
        gl->delete_framebuffers(1, &target->framebuffer);
        gl->delete_textures(1, &target->color);
        gl->delete_renderbuffers(1, &target->depth);
    }
    *target = (M_TARGET) {};
}

static RESULT M_CreateTarget(
    M_TARGET *const target, const int32_t width, const int32_t height)
{
    const M_GL *const gl = &m_Backend.gl;
    M_FreeTarget(target);
    gl->gen_textures(1, &target->color);
    gl->bind_texture(GL_TEXTURE_2D, target->color);
    gl->tex_image_2d(
        GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        nullptr);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->gen_renderbuffers(1, &target->depth);
    gl->bind_renderbuffer(GL_RENDERBUFFER, target->depth);
    gl->renderbuffer_storage(
        GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    gl->gen_framebuffers(1, &target->framebuffer);
    gl->bind_framebuffer(GL_FRAMEBUFFER, target->framebuffer);
    gl->framebuffer_texture_2d(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->color, 0);
    gl->framebuffer_renderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target->depth);
    const GLenum status = gl->check_framebuffer_status(GL_FRAMEBUFFER);
    gl->bind_framebuffer(GL_FRAMEBUFFER, 0);
    target->width = width;
    target->height = height;
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        M_FreeTarget(target);
        return FAIL("Cannot create a %dx%d render target", width, height);
    }
    return OK;
}

static void M_ClearDepth(void)
{
    if (m_Backend.es) {
        m_Backend.gl.clear_depthf(1.0f);
    } else {
        m_Backend.gl.clear_depth(1.0);
    }
}

static void M_ClearColor(const GFX_COLOR color)
{
    m_Backend.gl.clear_color(
        color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, 1.0f);
}

// Clears target to border, and rect of it to clear. Leaves the scissor
// test off.
static void M_ClearTarget(
    const M_TARGET *const target, const GFX_RECT rect, const GFX_COLOR border,
    const GFX_COLOR clear)
{
    const M_GL *const gl = &m_Backend.gl;
    gl->bind_framebuffer(GL_FRAMEBUFFER, target->framebuffer);
    gl->viewport(0, 0, target->width, target->height);
    gl->color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl->depth_mask(GL_TRUE);
    gl->disable(GL_SCISSOR_TEST);
    M_ClearColor(border);
    M_ClearDepth();
    gl->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->enable(GL_SCISSOR_TEST);
    gl->scissor(rect.x, rect.y, rect.width, rect.height);
    M_ClearColor(clear);
    gl->clear(GL_COLOR_BUFFER_BIT);
    gl->disable(GL_SCISSOR_TEST);
}

static RESULT M_EnsureOutput(const int32_t width, const int32_t height)
{
    if (m_Backend.output.framebuffer != 0 && m_Backend.output.width == width
        && m_Backend.output.height == height) {
        return OK;
    }
    return M_CreateTarget(&m_Backend.output, width, height);
}

// Copies the output target to the window. The targets are top-down and
// the window is bottom-up, so the copy flips it.
static void M_ShowOutput(void)
{
    const M_GL *const gl = &m_Backend.gl;
    const M_TARGET *const output = &m_Backend.output;
    gl->bind_framebuffer(GL_READ_FRAMEBUFFER, output->framebuffer);
    gl->bind_framebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl->blit_framebuffer(
        0, 0, output->width, output->height, 0, output->height, output->width,
        0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl->bind_framebuffer(GL_FRAMEBUFFER, 0);
}

static void M_EnsureIndices(const int32_t quads)
{
    if (quads <= m_Backend.index_quads) {
        return;
    }
    int32_t capacity = MAX(m_Backend.index_quads * 2, 1024);
    while (capacity < quads) {
        capacity *= 2;
    }
    m_Backend.indices = Mem_ReallocArr(
        m_Backend.indices, (size_t)capacity * 6, sizeof(uint32_t));
    for (int32_t i = 0; i < capacity; i++) {
        const uint32_t base = (uint32_t)i * 4;
        uint32_t *const quad = &m_Backend.indices[i * 6];
        quad[0] = base;
        quad[1] = base + 1;
        quad[2] = base + 3;
        quad[3] = base + 1;
        quad[4] = base + 2;
        quad[5] = base + 3;
    }
    m_Backend.gl.bind_buffer(GL_ELEMENT_ARRAY_BUFFER, m_Backend.index_buffer);
    m_Backend.gl.buffer_data(
        GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)capacity * 6 * sizeof(uint32_t),
        m_Backend.indices, GL_STATIC_DRAW);
    m_Backend.index_quads = capacity;
}

// Sets out to the span of four texture coordinates on one axis of a
// texture size texels long, half a texel in from its edges, and returns
// false when the span tiles.
static bool M_SpanTexels(
    const float coordinates[4], const int32_t size, float *const out_low,
    float *const out_high)
{
    float low = coordinates[0];
    float high = low;
    for (int32_t i = 1; i < 4; i++) {
        low = MIN(low, coordinates[i]);
        high = MAX(high, coordinates[i]);
    }
    // A quad tiles when its UVs cross from one copy of the texture into the
    // next.
    if (floorf(low) < ceilf(high) - 1.0f) {
        return false;
    }
    const float inset = 0.5f / (float)size;
    const float middle = (low + high) * 0.5f;
    *out_low = MIN(low + inset, middle);
    *out_high = MAX(high - inset, middle);
    return true;
}

// Sets each vertex's texel rect for a batch. The window pass magnifies, so
// it keeps each quad that does not tile inside its texels; elsewhere the
// rect is unbounded.
static void M_SetRects(
    const GFX_BATCH *const batch, const GFX_VERTEX *const vertices,
    float (*const rects)[4])
{
    const GFX_TEXTURE *const texture = batch->texture;
    const bool clamp = m_Backend.pass == GFX_PASS_WINDOW && texture != nullptr
        && texture->width > 0 && texture->height > 0;
    for (int32_t i = 0; i < batch->count; i += 4) {
        const GFX_VERTEX *const quad = &vertices[i];
        const float u[4] = { quad[0].uv.x, quad[1].uv.x, quad[2].uv.x,
                             quad[3].uv.x };
        const float v[4] = { quad[0].uv.y, quad[1].uv.y, quad[2].uv.y,
                             quad[3].uv.y };
        float rect[4];
        if (!clamp || !M_SpanTexels(u, texture->width, &rect[0], &rect[2])
            || !M_SpanTexels(v, texture->height, &rect[1], &rect[3])) {
            rect[0] = rect[1] = -FLT_MAX;
            rect[2] = rect[3] = FLT_MAX;
        }
        for (int32_t j = 0; j < 4; j++) {
            memcpy(rects[i + j], rect, sizeof(rect));
        }
    }
}

// Scissors to view's rect, mapped from logical-screen pixels to the
// output target.
static void M_SetScissor(const GFX_PASS_VIEW *const view)
{
    const M_GL *const gl = &m_Backend.gl;
    const GFX_PASS_OUTPUT *const output = &m_Backend.pass_output;
    const long x0 =
        lroundf(view->scissor_x * output->scale_x + output->offset_x);
    const long y0 =
        lroundf(view->scissor_y * output->scale_y + output->offset_y);
    const long x1 = lroundf(
        (view->scissor_x + view->scissor_width) * output->scale_x
        + output->offset_x);
    const long y1 = lroundf(
        (view->scissor_y + view->scissor_height) * output->scale_y
        + output->offset_y);
    gl->scissor(
        (GLint)x0, (GLint)y0, (GLsizei)MAX(x1 - x0, 0),
        (GLsizei)MAX(y1 - y0, 0));
}

static void M_SetView(const GFX_PASS_VIEW *const view)
{
    const M_GL *const gl = &m_Backend.gl;
    const M_LOCATIONS *const at = &m_Backend.locations;
    gl->uniform_matrix_4fv(at->view, 1, GL_FALSE, &view->view.m[0][0]);
    gl->uniform_matrix_4fv(
        at->projection, 1, GL_FALSE, &view->projection.m[0][0]);
    gl->uniform_1i(at->fog, view->fog.enabled ? 1 : 0);
    gl->uniform_2f(at->fog_range, view->fog.begin, view->fog.end);
    gl->uniform_3f(
        at->fog_color, view->fog.color.red / 255.0f,
        view->fog.color.green / 255.0f, view->fog.color.blue / 255.0f);
    M_SetScissor(view);
}

static void M_SetState(const GFX_STATE *const state, GFX_TEXTURE *const texture)
{
    const M_GL *const gl = &m_Backend.gl;
    const M_LOCATIONS *const at = &m_Backend.locations;
    switch (state->blend) {
    case GFX_BLEND_NONE:
        gl->disable(GL_BLEND);
        break;
    case GFX_BLEND_ALPHA:
        gl->enable(GL_BLEND);
        gl->blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case GFX_BLEND_ADD:
        gl->enable(GL_BLEND);
        gl->blend_func(GL_SRC_ALPHA, GL_ONE);
        break;
    }
    if (state->depth_test == GFX_DEPTH_OFF) {
        gl->disable(GL_DEPTH_TEST);
    } else {
        gl->enable(GL_DEPTH_TEST);
        gl->depth_func(
            state->depth_test == GFX_DEPTH_ALWAYS ? GL_ALWAYS : GL_LEQUAL);
    }
    gl->depth_mask(state->depth_write ? GL_TRUE : GL_FALSE);
    gl->uniform_1i(at->color_add, state->color_op == GFX_COLOR_ADD ? 1 : 0);
    gl->uniform_1i(at->alpha_test, state->alpha_test ? 1 : 0);
    const bool textured = texture != nullptr && texture->handle != nullptr;
    gl->uniform_1i(at->textured, textured ? 1 : 0);
    const M_TEXTURE *const handle = textured ? texture->handle : nullptr;
    const bool indexed = textured && texture->indexed;
    gl->uniform_1i(at->indexed, indexed ? 1 : 0);
    gl->active_texture(GL_TEXTURE1);
    gl->bind_texture(GL_TEXTURE_2D, indexed ? handle->palette : 0);
    gl->bind_sampler(1, m_Backend.samplers[0]);
    gl->active_texture(GL_TEXTURE0);
    gl->bind_texture(GL_TEXTURE_2D, textured ? handle->pixels : 0);
    // The output's filter samples what the window pass magnifies, as when it
    // scales the screen, so a sprite's edge never blends with the texels
    // past it. Indexed textures always sample their nearest index.
    const bool smooth = !indexed && m_Backend.pass == GFX_PASS_WINDOW
        && m_Backend.pass_output.filter == GFX_FILTER_LINEAR;
    const bool linear = !indexed && state->filter == GFX_FILTER_LINEAR;
    const bool linear_mag = m_Backend.pass == GFX_PASS_WINDOW ? smooth : linear;
    gl->bind_sampler(
        0,
        m_Backend.samplers
            [(state->repeat ? M_SAMPLER_REPEAT : 0)
             | (linear || smooth ? M_SAMPLER_LINEAR_MIN : 0)
             | (linear_mag ? M_SAMPLER_LINEAR_MAG : 0)]);
}

static RESULT M_Init(
    SDL_Window *const window, const int32_t width, const int32_t height)
{
    FAIL_IF(SDL_GL_GetCurrentContext() == nullptr, "No current OpenGL context");
    memset(&m_Backend, 0, sizeof(m_Backend));
    MUST(M_LoadFunctions());
    const M_GL *const gl = &m_Backend.gl;
    const char *const version = (const char *)gl->get_string(GL_VERSION);
    FAIL_IF(version == nullptr, "Cannot read the OpenGL version");
    m_Backend.es = strstr(version, "OpenGL ES") != nullptr;
    FAIL_IF(
        m_Backend.es ? gl->clear_depthf == nullptr : gl->clear_depth == nullptr,
        "Cannot load the OpenGL depth clear");
    LOG_INFO(
        "OpenGL %s, %s", version, (const char *)gl->get_string(GL_RENDERER));
    MUST(M_CreateProgram());
    gl->gen_vertex_arrays(1, &m_Backend.vertex_array);
    gl->bind_vertex_array(m_Backend.vertex_array);
    gl->gen_buffers(1, &m_Backend.vertex_buffer);
    gl->gen_buffers(1, &m_Backend.index_buffer);
    gl->bind_buffer(GL_ARRAY_BUFFER, m_Backend.vertex_buffer);
    gl->enable_vertex_attrib_array(0);
    gl->vertex_attrib_pointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(GFX_VERTEX),
        (const void *)offsetof(GFX_VERTEX, position));
    gl->enable_vertex_attrib_array(1);
    gl->vertex_attrib_pointer(
        1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GFX_VERTEX),
        (const void *)offsetof(GFX_VERTEX, color));
    gl->enable_vertex_attrib_array(2);
    gl->vertex_attrib_pointer(
        2, 2, GL_FLOAT, GL_FALSE, sizeof(GFX_VERTEX),
        (const void *)offsetof(GFX_VERTEX, uv));
    gl->gen_buffers(1, &m_Backend.rect_buffer);
    gl->bind_buffer(GL_ARRAY_BUFFER, m_Backend.rect_buffer);
    gl->enable_vertex_attrib_array(3);
    gl->vertex_attrib_pointer(
        3, 4, GL_FLOAT, GL_FALSE, sizeof(*m_Backend.rects), nullptr);
    gl->bind_buffer(GL_ELEMENT_ARRAY_BUFFER, m_Backend.index_buffer);
    M_EnsureIndices(1024);
    gl->gen_samplers(M_SAMPLER_COUNT, m_Backend.samplers);
    for (int32_t i = 0; i < M_SAMPLER_COUNT; i++) {
        const GLint wrap =
            (i & M_SAMPLER_REPEAT) != 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        gl->sampler_parameteri(
            m_Backend.samplers[i], GL_TEXTURE_MIN_FILTER,
            (i & M_SAMPLER_LINEAR_MIN) != 0 ? GL_LINEAR : GL_NEAREST);
        gl->sampler_parameteri(
            m_Backend.samplers[i], GL_TEXTURE_MAG_FILTER,
            (i & M_SAMPLER_LINEAR_MAG) != 0 ? GL_LINEAR : GL_NEAREST);
        gl->sampler_parameteri(m_Backend.samplers[i], GL_TEXTURE_WRAP_S, wrap);
        gl->sampler_parameteri(m_Backend.samplers[i], GL_TEXTURE_WRAP_T, wrap);
    }
    gl->pixel_storei(GL_UNPACK_ALIGNMENT, 1);
    gl->pixel_storei(GL_PACK_ALIGNMENT, 1);
    gl->active_texture(GL_TEXTURE0);
    return M_CreateTarget(&m_Backend.screen, width, height);
}

static void M_Shutdown(void)
{
    const M_GL *const gl = &m_Backend.gl;
    M_FreeTarget(&m_Backend.screen);
    M_FreeTarget(&m_Backend.output);
    M_FreeTarget(&m_Backend.history);
    gl->delete_samplers(M_SAMPLER_COUNT, m_Backend.samplers);
    gl->delete_buffers(1, &m_Backend.vertex_buffer);
    gl->delete_buffers(1, &m_Backend.index_buffer);
    gl->delete_buffers(1, &m_Backend.rect_buffer);
    gl->delete_vertex_arrays(1, &m_Backend.vertex_array);
    gl->delete_program(m_Backend.program);
    Mem_Free(m_Backend.indices);
    Mem_Free(m_Backend.rects);
    memset(&m_Backend, 0, sizeof(m_Backend));
}

static RESULT M_CreatePixels(
    const int32_t width, const int32_t height, const uint8_t *const pixels,
    const bool indexed, void **const out)
{
    const M_GL *const gl = &m_Backend.gl;
    GLuint texture = 0;
    gl->gen_textures(1, &texture);
    FAIL_IF(texture == 0, "Cannot create a texture");
    gl->bind_texture(GL_TEXTURE_2D, texture);
    gl->tex_image_2d(
        GL_TEXTURE_2D, 0, indexed ? GL_R8 : GL_RGBA8, width, height, 0,
        indexed ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    M_TEXTURE *const handle = Mem_AllocZ(sizeof(*handle));
    handle->pixels = texture;
    *out = handle;
    return OK;
}

static RESULT M_CreateTexture(
    const int32_t width, const int32_t height, const uint8_t *const pixels,
    void **const out)
{
    return M_CreatePixels(width, height, pixels, false, out);
}

static RESULT M_CreateIndexed(
    const int32_t width, const int32_t height, void **const out)
{
    uint8_t *const indices = Mem_AllocZ((size_t)width * (size_t)height);
    const RESULT result = M_CreatePixels(width, height, indices, true, out);
    Mem_Free(indices);
    if (!IS_OK(result)) {
        return result;
    }
    M_TEXTURE *const texture = *out;
    const M_GL *const gl = &m_Backend.gl;
    gl->gen_textures(1, &texture->palette);
    if (texture->palette == 0) {
        gl->delete_textures(1, &texture->pixels);
        Mem_Free(texture);
        *out = nullptr;
        return FAIL("Cannot create a palette texture");
    }
    const GFX_COLOR colors[256] = {};
    gl->bind_texture(GL_TEXTURE_2D, texture->palette);
    gl->tex_image_2d(
        GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        colors);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return OK;
}

static void M_UploadIndices(
    void *const handle, const GFX_RECT rect, const uint8_t *const indices)
{
    const M_GL *const gl = &m_Backend.gl;
    gl->bind_texture(GL_TEXTURE_2D, ((M_TEXTURE *)handle)->pixels);
    gl->tex_sub_image_2d(
        GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width, rect.height, GL_RED,
        GL_UNSIGNED_BYTE, indices);
}

static void M_UploadPalette(
    void *const handle, const int32_t first, const int32_t count,
    const GFX_COLOR *const colors)
{
    const M_GL *const gl = &m_Backend.gl;
    gl->bind_texture(GL_TEXTURE_2D, ((M_TEXTURE *)handle)->palette);
    gl->tex_sub_image_2d(
        GL_TEXTURE_2D, 0, first, 0, count, 1, GL_RGBA, GL_UNSIGNED_BYTE,
        colors);
}

static void M_UploadTexture(
    void *const handle, const GFX_RECT rect, const IMAGE *const image)
{
    const M_GL *const gl = &m_Backend.gl;
    gl->bind_texture(GL_TEXTURE_2D, ((M_TEXTURE *)handle)->pixels);
    gl->tex_sub_image_2d(
        GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width, rect.height, GL_RGBA,
        GL_UNSIGNED_BYTE, image->pixels);
}

static void M_CopyFromScreen(
    void *const handle, const GFX_RECT source, const int32_t x, const int32_t y)
{
    const M_GL *const gl = &m_Backend.gl;
    gl->bind_framebuffer(GL_READ_FRAMEBUFFER, m_Backend.screen.framebuffer);
    gl->bind_texture(GL_TEXTURE_2D, ((M_TEXTURE *)handle)->pixels);
    gl->copy_tex_sub_image_2d(
        GL_TEXTURE_2D, 0, x, y, source.x, source.y, source.width,
        source.height);
    if (m_Backend.target != nullptr) {
        gl->bind_framebuffer(GL_FRAMEBUFFER, m_Backend.target->framebuffer);
    }
}

static void M_FreeTexture(void *const handle)
{
    M_TEXTURE *const texture = handle;
    m_Backend.gl.delete_textures(1, &texture->pixels);
    m_Backend.gl.delete_textures(1, &texture->palette);
    Mem_Free(texture);
}

static void M_BeginPass(
    const GFX_PASS pass, const GFX_COLOR clear,
    const GFX_PASS_OUTPUT *const output, const bool preserve)
{
    const M_GL *const gl = &m_Backend.gl;
    if (pass == GFX_PASS_WINDOW
        && !Result_Log(
            M_EnsureOutput(output->width, output->height), LOG_LEVEL_ERROR)) {
        m_Backend.target = nullptr;
        return;
    }
    M_TARGET *const target =
        pass == GFX_PASS_SCREEN ? &m_Backend.screen : &m_Backend.output;
    m_Backend.target = target;
    m_Backend.pass = pass;
    m_Backend.pass_output = *output;
    if (pass == GFX_PASS_SCREEN && preserve && target->drawn) {
        M_TARGET *const history = &m_Backend.history;
        if (history->framebuffer == 0
            && !Result_Log(
                M_CreateTarget(history, target->width, target->height),
                LOG_LEVEL_ERROR)) {
            m_Backend.target = nullptr;
            return;
        }
        gl->disable(GL_SCISSOR_TEST);
        gl->color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl->bind_framebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
        gl->bind_framebuffer(GL_DRAW_FRAMEBUFFER, history->framebuffer);
        gl->blit_framebuffer(
            0, 0, target->width, target->height, 0, 0, target->width,
            target->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        history->drawn = true;
    }
    M_ClearTarget(target, output->rect, output->border, clear);
    if (preserve && m_Backend.history.drawn) {
        const M_TARGET *const history = &m_Backend.history;
        const GFX_RECT rect = output->rect;
        gl->bind_framebuffer(GL_READ_FRAMEBUFFER, history->framebuffer);
        gl->bind_framebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
        gl->blit_framebuffer(
            0, 0, history->width, history->height, rect.x, rect.y,
            rect.x + rect.width, rect.y + rect.height, GL_COLOR_BUFFER_BIT,
            output->filter == GFX_FILTER_LINEAR ? GL_LINEAR : GL_NEAREST);
        gl->bind_framebuffer(GL_FRAMEBUFFER, target->framebuffer);
    }
    gl->color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    gl->enable(GL_SCISSOR_TEST);
    gl->use_program(m_Backend.program);
    gl->bind_vertex_array(m_Backend.vertex_array);
    gl->uniform_4f(
        m_Backend.locations.output, 2.0f * output->scale_x / target->width,
        2.0f * output->scale_y / target->height,
        2.0f * output->offset_x / target->width - 1.0f,
        2.0f * output->offset_y / target->height - 1.0f);
}

static void M_Draw(
    const GFX_PASS_VIEW *const views, const GFX_BATCH *const batches,
    const int32_t batch_count, const GFX_VERTEX *const vertices)
{
    if (m_Backend.target == nullptr || batch_count <= 0) {
        return;
    }
    const M_GL *const gl = &m_Backend.gl;
    const int32_t first = batches[0].first;
    const GFX_BATCH *const last = &batches[batch_count - 1];
    const int32_t count = last->first + last->count - first;
    M_EnsureIndices(count / 4);
    gl->bind_buffer(GL_ARRAY_BUFFER, m_Backend.vertex_buffer);
    gl->buffer_data(
        GL_ARRAY_BUFFER, (GLsizeiptr)count * (GLsizeiptr)sizeof(GFX_VERTEX),
        vertices + first, GL_STREAM_DRAW);
    if (count > m_Backend.rect_quads * 4) {
        m_Backend.rect_quads = MAX(m_Backend.rect_quads * 2, count / 4);
        m_Backend.rects = Mem_ReallocArr(
            m_Backend.rects, (size_t)m_Backend.rect_quads * 4,
            sizeof(*m_Backend.rects));
    }
    for (int32_t i = 0; i < batch_count; i++) {
        M_SetRects(
            &batches[i], vertices + batches[i].first,
            m_Backend.rects + (batches[i].first - first));
    }
    gl->bind_buffer(GL_ARRAY_BUFFER, m_Backend.rect_buffer);
    gl->buffer_data(
        GL_ARRAY_BUFFER,
        (GLsizeiptr)count * (GLsizeiptr)sizeof(*m_Backend.rects),
        m_Backend.rects, GL_STREAM_DRAW);
    gl->bind_buffer(GL_ELEMENT_ARRAY_BUFFER, m_Backend.index_buffer);
    int32_t view = -1;
    for (int32_t i = 0; i < batch_count; i++) {
        const GFX_BATCH *const batch = &batches[i];
        if (batch->view != view) {
            view = batch->view;
            M_SetView(&views[view]);
        }
        M_SetState(&batch->state, batch->texture);
        gl->draw_elements(
            GL_TRIANGLES, batch->count / 4 * 6, GL_UNSIGNED_INT,
            (const void *)((uintptr_t)(batch->first - first) / 4 * 6
                           * sizeof(uint32_t)));
    }
}

static void M_Clear(
    const uint32_t flags, const GFX_COLOR color,
    const GFX_PASS_VIEW *const view)
{
    if (m_Backend.target == nullptr) {
        return;
    }
    const M_GL *const gl = &m_Backend.gl;
    M_SetScissor(view);
    GLbitfield bits = 0;
    if (flags & GFX_CLEAR_COLOR) {
        M_ClearColor(color);
        bits |= GL_COLOR_BUFFER_BIT;
    }
    if (flags & GFX_CLEAR_DEPTH) {
        gl->depth_mask(GL_TRUE);
        M_ClearDepth();
        bits |= GL_DEPTH_BUFFER_BIT;
    }
    if (bits != 0) {
        gl->clear(bits);
    }
}

static void M_EndPass(void)
{
    if (m_Backend.target == nullptr) {
        return;
    }
    m_Backend.target->drawn = true;
    const M_GL *const gl = &m_Backend.gl;
    gl->disable(GL_SCISSOR_TEST);
    gl->color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_Backend.pass == GFX_PASS_WINDOW) {
        M_ShowOutput();
    }
    gl->bind_framebuffer(GL_FRAMEBUFFER, 0);
    m_Backend.target = nullptr;
}

static void M_BlitScreen(const GFX_PASS_OUTPUT *const output)
{
    if (!Result_Log(
            M_EnsureOutput(output->width, output->height), LOG_LEVEL_ERROR)) {
        return;
    }
    const M_GL *const gl = &m_Backend.gl;
    M_ClearTarget(
        &m_Backend.output, output->rect, output->border, output->border);
    const GFX_RECT rect = output->rect;
    gl->bind_framebuffer(GL_READ_FRAMEBUFFER, m_Backend.screen.framebuffer);
    gl->bind_framebuffer(GL_DRAW_FRAMEBUFFER, m_Backend.output.framebuffer);
    gl->blit_framebuffer(
        0, 0, m_Backend.screen.width, m_Backend.screen.height, rect.x, rect.y,
        rect.x + rect.width, rect.y + rect.height, GL_COLOR_BUFFER_BIT,
        output->filter == GFX_FILTER_LINEAR ? GL_LINEAR : GL_NEAREST);
    m_Backend.output.drawn = true;
    M_ShowOutput();
}

static RESULT M_ReadPixels(
    const GFX_PASS pass, const ALLOCATOR *const allocator, IMAGE *const out)
{
    const M_TARGET *const target =
        pass == GFX_PASS_SCREEN ? &m_Backend.screen : &m_Backend.output;
    FAIL_IF(
        !target->drawn && m_Backend.target != target,
        "Nothing is drawn to read back");
    const M_GL *const gl = &m_Backend.gl;
    *out = Image_Create(target->width, target->height, allocator);
    gl->bind_framebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
    gl->read_pixels(
        0, 0, target->width, target->height, GL_RGBA, GL_UNSIGNED_BYTE,
        out->pixels);
    gl->bind_framebuffer(
        GL_FRAMEBUFFER,
        m_Backend.target != nullptr ? m_Backend.target->framebuffer : 0);
    const GLenum error = gl->get_error();
    if (error != GL_NO_ERROR) {
        Image_Free(out);
        return FAIL("Cannot read back pixels: OpenGL error 0x%x", error);
    }
    return OK;
}

static const GFX_BACKEND m_GlBackend = {
    .init = M_Init,
    .shutdown = M_Shutdown,
    .create_texture = M_CreateTexture,
    .create_indexed = M_CreateIndexed,
    .upload_indices = M_UploadIndices,
    .upload_palette = M_UploadPalette,
    .upload_texture = M_UploadTexture,
    .copy_from_screen = M_CopyFromScreen,
    .free_texture = M_FreeTexture,
    .begin_pass = M_BeginPass,
    .draw = M_Draw,
    .clear = M_Clear,
    .end_pass = M_EndPass,
    .blit_screen = M_BlitScreen,
    .read_pixels = M_ReadPixels,
};

RESULT Gfx_Init(
    SDL_Window *const window, const int32_t width, const int32_t height)
{
    return GfxFrontend_Start(&m_GlBackend, window, width, height);
}

void Gfx_Shutdown(void)
{
    GfxFrontend_Stop();
}
