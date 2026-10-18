/* Copyright (C) 2026 Basler AG
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GST_PYLON_NVMM_COLOR_H
#define GST_PYLON_NVMM_COLOR_H

#include <gst/video/video.h>

/**
 * Platform-independent NVMM surface color selection for pylonsrc.
 *
 * The kind enum stays free of nvbufsurface.h so policy unit tests run without
 * DeepStream / Jetson Multimedia API headers. With NVMM_ENABLED, helpers map
 * kinds to NvBufSurfaceColorFormat.
 */
enum class GstPylonNvmmColorKind {
  Unsupported = 0,
  Gray8,
  Rgb,
  Bgr,
  Yuyv,
  Uyvy,
  YuyvEr,
  UyvyEr,
};

/**
 * Choose the NVMM surface kind for a GStreamer video format and color range.
 *
 * Full-range (`GST_VIDEO_COLOR_RANGE_0_255`) YUY2/UYVY request the extended
 * (*Er) kinds. Caps without colorimetry default to limited range for YUV in
 * GStreamer, so they keep the plain Yuyv/Uyvy kinds.
 */
GstPylonNvmmColorKind gst_pylon_nvmm_color_kind(GstVideoFormat format,
                                                GstVideoColorRange range);

/** True when kind refers to an extended-range YUV surface. */
bool gst_pylon_nvmm_color_kind_is_extended(GstPylonNvmmColorKind kind);

#ifdef NVMM_ENABLED
#  include <nvbufsurface.h>

NvBufSurfaceColorFormat gst_pylon_nvmm_kind_to_nvbuf(
    GstPylonNvmmColorKind kind);

/** Kind selection + NvBufSurfaceColorFormat mapping used by the NVMM factory.
 */
NvBufSurfaceColorFormat gst_pylon_nvmm_color_format(GstVideoFormat format,
                                                    GstVideoColorRange range);
#endif

#endif
