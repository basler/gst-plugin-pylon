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

#include "gstpylonnvmmcolor.h"

GstPylonNvmmColorKind gst_pylon_nvmm_color_kind(GstVideoFormat format,
                                                GstVideoColorRange range) {
  const bool full_range = (range == GST_VIDEO_COLOR_RANGE_0_255);

  switch (format) {
    case GST_VIDEO_FORMAT_GRAY8:
      return GstPylonNvmmColorKind::Gray8;
    case GST_VIDEO_FORMAT_RGB:
      return GstPylonNvmmColorKind::Rgb;
    case GST_VIDEO_FORMAT_BGR:
      return GstPylonNvmmColorKind::Bgr;
    case GST_VIDEO_FORMAT_YUY2:
      return full_range ? GstPylonNvmmColorKind::YuyvEr
                        : GstPylonNvmmColorKind::Yuyv;
    case GST_VIDEO_FORMAT_UYVY:
      return full_range ? GstPylonNvmmColorKind::UyvyEr
                        : GstPylonNvmmColorKind::Uyvy;
    default:
      return GstPylonNvmmColorKind::Unsupported;
  }
}

bool gst_pylon_nvmm_color_kind_is_extended(GstPylonNvmmColorKind kind) {
  return kind == GstPylonNvmmColorKind::YuyvEr ||
         kind == GstPylonNvmmColorKind::UyvyEr;
}

#ifdef NVMM_ENABLED
NvBufSurfaceColorFormat gst_pylon_nvmm_kind_to_nvbuf(
    GstPylonNvmmColorKind kind) {
  switch (kind) {
    case GstPylonNvmmColorKind::Gray8:
      return NVBUF_COLOR_FORMAT_GRAY8;
    case GstPylonNvmmColorKind::Rgb:
      return NVBUF_COLOR_FORMAT_RGB;
    case GstPylonNvmmColorKind::Bgr:
      return NVBUF_COLOR_FORMAT_BGR;
    case GstPylonNvmmColorKind::Yuyv:
      return NVBUF_COLOR_FORMAT_YUYV;
    case GstPylonNvmmColorKind::Uyvy:
      return NVBUF_COLOR_FORMAT_UYVY;
    case GstPylonNvmmColorKind::YuyvEr:
      return NVBUF_COLOR_FORMAT_YUYV_ER;
    case GstPylonNvmmColorKind::UyvyEr:
      return NVBUF_COLOR_FORMAT_UYVY_ER;
    case GstPylonNvmmColorKind::Unsupported:
    default:
      return NVBUF_COLOR_FORMAT_INVALID;
  }
}

NvBufSurfaceColorFormat gst_pylon_nvmm_color_format(GstVideoFormat format,
                                                    GstVideoColorRange range) {
  return gst_pylon_nvmm_kind_to_nvbuf(gst_pylon_nvmm_color_kind(format, range));
}
#endif
