// ProsperoLichess - The single compilation unit for the vendored stb_image.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pictures are read into memory and decoded from there, so stdio is left out;
// JPEG is the only format the app ships.

#include "image.h"

#define STB_IMAGE_IMPLEMENTATION 1
#define STB_IMAGE_STATIC 1
#define STBI_ONLY_JPEG 1
#define STBI_NO_STDIO 1
#define STBI_NO_LINEAR 1
#define STBI_NO_HDR 1
#define STBI_NO_THREAD_LOCALS 1
#define STBI_NO_FAILURE_STRINGS 1
#include "stb_image.inc"

unsigned char *pch_image_decode_jpeg(const unsigned char *data, int size, int *width, int *height)
{
    int channels = 0;
    return stbi_load_from_memory(data, size, width, height, &channels, 4);
}

void pch_image_free(unsigned char *pixels)
{
    stbi_image_free(pixels);
}
