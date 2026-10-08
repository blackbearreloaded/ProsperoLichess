// ProsperoLichess - JPEG decoding (the vendored stb_image, JPEG only).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PCH_THIRD_PARTY_STB_IMAGE_H
#define PCH_THIRD_PARTY_STB_IMAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // Decodes a JPEG held in memory to RGBA, top row first. Returns the pixels
    // (release them with pch_image_free) or NULL.
    unsigned char *pch_image_decode_jpeg(const unsigned char *data, int size, int *width,
                                         int *height);
    void pch_image_free(unsigned char *pixels);

#ifdef __cplusplus
}
#endif

#endif
