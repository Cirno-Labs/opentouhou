#pragma once

#include "th01/video.h"

// Writable view of g_Video for the module's own files, tests and for the
// raw pixel pokes that belong with the coming th01-draw split (writing
// through g_Video->pages[...] is fine even on the const view; this is
// for the few call sites that need g_Video's own fields writable).
extern VIDEO *const p_Video;
