#include "core/text/font_registry.h"
#include "core/text/private.h"

#include <SDL3/SDL.h>

// Loads fontconfig when the system has it, so that the game runs without
// it and the build needs none of its headers. The declarations follow
// fontconfig's stable ABI.

#define M_FC_FAMILY "family"
#define M_FC_FULLNAME "fullname"
#define M_FC_FILE "file"
#define M_FC_STYLE "style"
#define M_FC_RESULT_MATCH 0

typedef struct M_FC_PATTERN M_FC_PATTERN;
typedef struct M_FC_OBJECT_SET M_FC_OBJECT_SET;

typedef struct {
    int nfont;
    int sfont;
    M_FC_PATTERN **fonts;
} M_FC_FONT_SET;

static struct {
    bool loaded;
    SDL_SharedObject *library;
    int (*Init)(void);
    M_FC_PATTERN *(*PatternCreate)(void);
    void (*PatternDestroy)(M_FC_PATTERN *pattern);
    int (*PatternAddString)(
        M_FC_PATTERN *pattern, const char *object, const char *text);
    int (*PatternGetString)(
        const M_FC_PATTERN *pattern, const char *object, int index,
        char **text);
    M_FC_OBJECT_SET *(*ObjectSetBuild)(const char *first, ...);
    void (*ObjectSetDestroy)(M_FC_OBJECT_SET *set);
    M_FC_FONT_SET *(*FontList)(
        void *config, M_FC_PATTERN *pattern, M_FC_OBJECT_SET *set);
    void (*FontSetDestroy)(M_FC_FONT_SET *set);
} m_Fc;

static RESULT M_Load(void)
{
    if (m_Fc.loaded) {
        return OK;
    }
    SDL_SharedObject *const library = SDL_LoadObject("libfontconfig.so.1");
    FAIL_IF(library == nullptr, "Cannot load fontconfig: %s", SDL_GetError());
    const struct {
        const char *name;
        SDL_FunctionPointer *function;
    } functions[] = {
        { "FcInit", (SDL_FunctionPointer *)&m_Fc.Init },
        { "FcPatternCreate", (SDL_FunctionPointer *)&m_Fc.PatternCreate },
        { "FcPatternDestroy", (SDL_FunctionPointer *)&m_Fc.PatternDestroy },
        { "FcPatternAddString", (SDL_FunctionPointer *)&m_Fc.PatternAddString },
        { "FcPatternGetString", (SDL_FunctionPointer *)&m_Fc.PatternGetString },
        { "FcObjectSetBuild", (SDL_FunctionPointer *)&m_Fc.ObjectSetBuild },
        { "FcObjectSetDestroy", (SDL_FunctionPointer *)&m_Fc.ObjectSetDestroy },
        { "FcFontList", (SDL_FunctionPointer *)&m_Fc.FontList },
        { "FcFontSetDestroy", (SDL_FunctionPointer *)&m_Fc.FontSetDestroy },
    };
    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++) {
        *functions[i].function = SDL_LoadFunction(library, functions[i].name);
        if (*functions[i].function == nullptr) {
            SDL_UnloadObject(library);
            return FAIL("Cannot load fontconfig's %s", functions[i].name);
        }
    }
    if (m_Fc.Init() == 0) {
        SDL_UnloadObject(library);
        return FAIL("Cannot initialize fontconfig");
    }
    m_Fc.library = library;
    m_Fc.loaded = true;
    return OK;
}

// Returns the file of the first installed font whose object is text, and
// whose style is style when it is not nullptr, as a new string that
// SDL_free releases, or nullptr. fontconfig's list matches names without
// substitutes.
static char *M_FindFile(
    const char *const object, const char *const text, const char *const style)
{
    M_FC_PATTERN *const pattern = m_Fc.PatternCreate();
    M_FC_OBJECT_SET *const set =
        m_Fc.ObjectSetBuild(M_FC_FILE, (const char *)nullptr);
    char *path = nullptr;
    if (pattern != nullptr && set != nullptr
        && m_Fc.PatternAddString(pattern, object, text) != 0
        && (style == nullptr
            || m_Fc.PatternAddString(pattern, M_FC_STYLE, style) != 0)) {
        M_FC_FONT_SET *const fonts = m_Fc.FontList(nullptr, pattern, set);
        char *file;
        if (fonts != nullptr && fonts->nfont > 0
            && m_Fc.PatternGetString(fonts->fonts[0], M_FC_FILE, 0, &file)
                == M_FC_RESULT_MATCH) {
            path = SDL_strdup(file);
        }
        if (fonts != nullptr) {
            m_Fc.FontSetDestroy(fonts);
        }
    }
    if (set != nullptr) {
        m_Fc.ObjectSetDestroy(set);
    }
    if (pattern != nullptr) {
        m_Fc.PatternDestroy(pattern);
    }
    return path;
}

RESULT FontRegistry_AddSystemFont(const char *const family)
{
    FAIL_IF(family == nullptr, "Missing font family");
    MUST(M_Load());
    // Prefers the Regular face, whose file has the family name.
    char *path = M_FindFile(M_FC_FAMILY, family, "Regular");
    if (path == nullptr) {
        path = M_FindFile(M_FC_FAMILY, family, nullptr);
    }
    if (path == nullptr) {
        path = M_FindFile(M_FC_FULLNAME, family, nullptr);
    }
    FAIL_IF(path == nullptr, "The system has no font %s", family);
    const RESULT result = FontRegistry_AddSystemFile(path, family);
    SDL_free(path);
    return result;
}
