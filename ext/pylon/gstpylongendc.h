/* Copyright (C) 2022 Basler AG
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

#pragma once

#include <gst/gst.h>
#include <gst/pylon/gstpylonincludes.h>

#include <cstddef>
#include <cstdint>

struct GstPylonResolvedPayload {
  void* data = nullptr;
  size_t size = 0;
  size_t stride = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  Pylon::EPixelType pixel_type = Pylon::PixelType_Undefined;
  bool is_gendc = false;
};

/* Resolve image pixels and geometry from either a standard image/chunk payload
 * or a GenDC container (GeV 3.0 GenDC-only and USB3 optional GenDC).
 *
 * Lifetime invariant: any temporary CPylonDataContainer / CPylonDataComponent
 * is destroyed before this function returns so the caller's grab result buffer
 * does not accumulate extra container references; the returned `data` pointer
 * remains valid for as long as the underlying grab result buffer is alive. */
[[nodiscard]] inline bool gst_pylon_resolve_gendc_component(
    const Pylon::CPylonDataContainer& container, GstPylonResolvedPayload& out,
    GError** err) {
  Pylon::CPylonDataComponent comp = container.GetFirstImageDataComponent(false);
  if (!comp.IsValid()) {
    /* Fall back to scanning components for any 2D pixel-bearing component */
    const size_t count = container.GetDataComponentCount();
    for (size_t i = 0; i < count; ++i) {
      Pylon::CPylonDataComponent candidate = container.GetDataComponent(i);
      if (candidate.IsValid() &&
          candidate.GetPixelType() != Pylon::PixelType_Undefined &&
          candidate.GetWidth() > 0 && candidate.GetHeight() > 0) {
        comp = candidate;
        break;
      }
    }
  }

  if (!comp.IsValid() || comp.GetData() == nullptr || comp.GetDataSize() == 0) {
    g_set_error(err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE,
                "GenDC payload contains no valid image data component");
    return false;
  }

  out.data = comp.GetData();
  out.size = comp.GetDataSize();
  out.width = comp.GetWidth();
  out.height = comp.GetHeight();
  out.pixel_type = comp.GetPixelType();
  out.is_gendc = true;
  if (!comp.GetStride(out.stride)) {
    Pylon::ComputeStride(out.stride, out.pixel_type, out.width,
                         comp.GetPaddingX());
  }
  return true;
}

[[nodiscard]] inline bool gst_pylon_resolve_grab_payload(
    const Pylon::CBaslerUniversalGrabResultPtr& grab_result_ptr,
    bool allow_corrupted, GstPylonResolvedPayload& out, GError** err) {
  if (grab_result_ptr->GrabSucceeded() &&
      grab_result_ptr->GetPayloadType() == Pylon::PayloadType_GenDC) {
    try {
      Pylon::CPylonDataContainer container =
          grab_result_ptr->GetDataContainer();
      return gst_pylon_resolve_gendc_component(container, out, err);
    } catch (const GenICam::GenericException& e) {
      g_set_error(err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE,
                  "Failed to unpack GenDC container: %s", e.GetDescription());
      return false;
    }
  }

  try {
    out.data = grab_result_ptr->GetBuffer();
    out.is_gendc = false;
    if (grab_result_ptr->GrabSucceeded()) {
      out.size = grab_result_ptr->GetImageSize();
      out.width = grab_result_ptr->GetWidth();
      out.height = grab_result_ptr->GetHeight();
      out.pixel_type = grab_result_ptr->GetPixelType();
      if (!grab_result_ptr->GetStride(out.stride)) {
        out.stride = 0;
      }
    } else if (allow_corrupted) {
      out.size = grab_result_ptr->GetPayloadSize();
      if (out.size == 0 || out.size == static_cast<size_t>(-1)) {
        out.size = grab_result_ptr->GetBufferSize();
      }
      out.pixel_type = grab_result_ptr->GetPixelType();
      out.width = grab_result_ptr->GetWidth();
      out.height = grab_result_ptr->GetHeight();
      if (!grab_result_ptr->GetStride(out.stride)) {
        out.stride = 0;
      }
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                  grab_result_ptr->GetErrorDescription().c_str());
      return false;
    }
    return true;
  } catch (const GenICam::GenericException& e) {
    g_set_error(err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE,
                "Failed to inspect grab payload: %s", e.GetDescription());
    return false;
  }
}
