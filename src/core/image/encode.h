// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/types.h"
#include "core/image/pixels.h"
#include "core/io/writer.h"

// Encode image as PNG, or as JPEG at quality 1 to 100.
void Image_EncodePng(const IMAGE *image, WRITER *writer);
void Image_EncodeJpeg(const IMAGE *image, int32_t quality, WRITER *writer);
