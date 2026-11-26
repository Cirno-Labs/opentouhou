#pragma once

// Reads archives through one interface: core/archive/zip.h, stg/pbg3.h and
// each other format provide an ARCHIVE_FORMAT, and every archive behaves the
// same whatever its format.

#include "core/archive/archive.h"
#include "core/archive/format.h"
#include "core/archive/hdi.h"
#include "core/archive/zip.h"
