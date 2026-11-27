#include "core/text/font_registry.h"
#include "core/text/private.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <limits.h>

// Returns the URL of the first installed font whose attribute is name,
// retained, or nullptr. Matching on a mandatory attribute gives no
// substitutes.
static CFURLRef M_FindUrl(const CFStringRef attribute, const CFStringRef name)
{
    const void *keys[] = { attribute };
    const void *values[] = { name };
    CFDictionaryRef const attributes = CFDictionaryCreate(
        nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef const descriptor =
        CTFontDescriptorCreateWithAttributes(attributes);
    CFSetRef const mandatory =
        CFSetCreate(nullptr, keys, 1, &kCFTypeSetCallBacks);
    CFArrayRef const matches =
        CTFontDescriptorCreateMatchingFontDescriptors(descriptor, mandatory);
    CFURLRef url = nullptr;
    if (matches != nullptr && CFArrayGetCount(matches) > 0) {
        url = CTFontDescriptorCopyAttribute(
            CFArrayGetValueAtIndex(matches, 0), kCTFontURLAttribute);
    }
    if (matches != nullptr) {
        CFRelease(matches);
    }
    CFRelease(mandatory);
    CFRelease(descriptor);
    CFRelease(attributes);
    return url;
}

RESULT FontRegistry_AddSystemFont(const char *const family)
{
    FAIL_IF(family == nullptr, "Missing font family");
    CFStringRef const name =
        CFStringCreateWithCString(nullptr, family, kCFStringEncodingUTF8);
    FAIL_IF(name == nullptr, "Invalid font family %s", family);
    CFURLRef url = M_FindUrl(kCTFontFamilyNameAttribute, name);
    if (url == nullptr) {
        url = M_FindUrl(kCTFontNameAttribute, name);
    }
    if (url == nullptr) {
        url = M_FindUrl(kCTFontDisplayNameAttribute, name);
    }
    CFRelease(name);
    FAIL_IF(url == nullptr, "The system has no font %s", family);
    char path[PATH_MAX];
    const bool converted = CFURLGetFileSystemRepresentation(
        url, true, (UInt8 *)path, sizeof(path));
    CFRelease(url);
    FAIL_IF(!converted, "The system's font %s has no file path", family);
    return FontRegistry_AddSystemFile(path, family);
}
