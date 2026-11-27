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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef NVMM_ENABLED
#  include "cuda_runtime.h"
#  include "gstpylondsnvmmbufferfactory.h"
#endif

#include "gst/pylon/gstpyloncache.h"
#include "gst/pylon/gstpylondebug.h"
#include "gst/pylon/gstpylonformatmapping.h"
#include "gst/pylon/gstpylonincludes.h"
#include "gst/pylon/gstpylonmetaprivate.h"
#include "gst/pylon/gstpylonobject.h"
#include "gstpylondisconnecthandler.h"
#include "gstpylongendc.h"
#include "gstpylonimagehandler.h"
#include "gstpyloninspect.h"
#include "gstpylonschema.h"
#include "gstpylonsession.h"
#include "gstpylonsysmembufferfactory.h"

#include <exception>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace {

using ScopedGstBuffer = std::unique_ptr<GstBuffer, decltype(&gst_buffer_unref)>;

class ScopedGstBufferMap {
 public:
  ScopedGstBufferMap(GstBuffer* buf, GstMapFlags flags)
      : buf_(buf),
        mapped_(buf != nullptr && gst_buffer_map(buf, &info_, flags)) {}

  ~ScopedGstBufferMap() { Unmap(); }

  ScopedGstBufferMap(const ScopedGstBufferMap&) = delete;
  ScopedGstBufferMap& operator=(const ScopedGstBufferMap&) = delete;

  bool IsMapped() const { return mapped_; }
  GstMapInfo& Info() { return info_; }

  void Unmap() {
    if (mapped_) {
      gst_buffer_unmap(buf_, &info_);
      mapped_ = false;
    }
  }

 private:
  GstBuffer* buf_ = nullptr;
  GstMapInfo info_{};
  bool mapped_ = false;
};

}  // namespace

void GstPylonSession::AddResultMeta(
    GstBuffer* buf, Pylon::CBaslerUniversalGrabResultPtr& grab_result_ptr,
    gsize override_stride) {
  g_return_if_fail(buf);

  gst_buffer_add_pylon_meta_with_stride(buf, grab_result_ptr, override_stride);
}

static void free_ptr_grab_result(gpointer data) {
  g_return_if_fail(data);

  auto* ref = static_cast<GstPylonBufferRef*>(data);
  delete ref;
}

static void gst_pylon_post_capture_error_message(
    GstElement* elem, const gchar* action, gint skipped_count, uint32_t code,
    const std::string& description) {
  if (!elem) {
    return;
  }
  GstStructure* s = gst_structure_new(
      "pylonsrc-capture-error", "action", G_TYPE_STRING, action,
      "skipped-frames", G_TYPE_INT, skipped_count, "error-code", G_TYPE_UINT,
      static_cast<guint>(code), "description", G_TYPE_STRING,
      description.c_str(), NULL);
  gst_element_post_message(elem, gst_message_new_element(GST_OBJECT(elem), s));
}

/* Scale 10-bit or 12-bit unpacked little-endian monochrome samples into full
 * 16-bit GRAY16_LE range [0..65535] so GStreamer videoconvert/displays render
 * expected brightness instead of near-black. */
static void gst_pylon_scale_mono_sub16_to_gray16(const uint16_t* src,
                                                 size_t src_stride_bytes,
                                                 uint16_t* dst, uint32_t width,
                                                 uint32_t height,
                                                 unsigned int bit_depth) {
  const unsigned int shift_up = 16U - bit_depth;
  const unsigned int shift_down = 2U * bit_depth - 16U;
  const uint16_t mask = static_cast<uint16_t>((1U << bit_depth) - 1U);

  for (uint32_t y = 0; y < height; ++y) {
    const auto* src_row = reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(src) + y * src_stride_bytes);
    uint16_t* dst_row = dst + static_cast<size_t>(y) * width;
    for (uint32_t x = 0; x < width; ++x) {
      const uint16_t v = src_row[x] & mask;
      dst_row[x] = static_cast<uint16_t>((v << shift_up) | (v >> shift_down));
    }
  }
}

gboolean GstPylonSession::CaptureCompressed(
    Pylon::CBaslerUniversalGrabResultPtr& grab_result,
    const Pylon::CompressionInfo_t& comp_info, GstBuffer** buf, GError** err) {
  if (comp_info.compressionStatus != Pylon::CompressionStatus_Ok) {
    g_set_error(err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE,
                "Camera failed to compress frame (CompressionStatus=%d); "
                "increase BslImageCompressionRatio or use lossless mode",
                static_cast<int>(comp_info.compressionStatus));
    return FALSE;
  }

  gsize effective_stride = 0;

  try {
    if (!decompressor.HasCompressionDescriptor()) {
      decompressor.SetCompressionDescriptor(camera->GetNodeMap());
      compression_active = true;
    }

    const Pylon::EPixelType px_type = comp_info.pixelType;
    const bool is_sub16_mono = (px_type == Pylon::PixelType_Mono10 ||
                                px_type == Pylon::PixelType_Mono12);

#ifdef NVMM_ENABLED
    if (MEM_NVMM == mem_type) {
      NvBufSurface* surf =
          reinterpret_cast<NvBufSurface*>(grab_result->GetBufferContext());

      decomp_scratch_buf.resize(comp_info.decompressedImageSize);
      size_t out_sz = decomp_scratch_buf.size();
      decompressor.DecompressImage(decomp_scratch_buf.data(), &out_sz,
                                   grab_result);

      size_t decomp_stride = 0;
      if (!Pylon::ComputeStride(decomp_stride, px_type,
                                static_cast<uint32_t>(comp_info.width),
                                comp_info.paddingX)) {
        decomp_stride = (static_cast<size_t>(comp_info.width) *
                         Pylon::BitPerPixel(px_type)) >>
                        3;
      }
      const size_t src_width = (static_cast<size_t>(comp_info.width) *
                                Pylon::BitPerPixel(px_type)) >>
                               3;

      cudaError_t cuda_err = cudaMemcpy2D(
          surf->surfaceList[0].mappedAddr.addr[0], surf->surfaceList[0].pitch,
          decomp_scratch_buf.data(), decomp_stride, src_width, comp_info.height,
          cudaMemcpyDefault);
      if (cuda_err != cudaSuccess) {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "Error copying decompressed memory to device");
        return FALSE;
      }
      NvBufSurfaceSyncForDevice(surf, 0, 0);

      effective_stride = decomp_stride;
      auto* buffer_ref = new GstPylonBufferRef{runtime, buffer_factory,
                                               std::move(grab_result)};
      *buf = gst_buffer_new_wrapped_full(
          GST_MEMORY_FLAG_READONLY, surf, sizeof(*surf), 0, sizeof(*surf),
          buffer_ref, static_cast<GDestroyNotify>(free_ptr_grab_result));

      try {
        AddResultMeta(*buf, buffer_ref->grab_result, effective_stride);
      } catch (const Pylon::GenericException& e) {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "Failed to attach Pylon meta: %s", e.GetDescription());
        gst_buffer_unref(*buf);
        *buf = NULL;
        return FALSE;
      }
      return TRUE;
    }
#endif

    if (is_sub16_mono) {
      decomp_scratch_buf.resize(comp_info.decompressedImageSize);
      size_t out_sz = decomp_scratch_buf.size();
      decompressor.DecompressImage(decomp_scratch_buf.data(), &out_sz,
                                   grab_result);

      size_t decomp_stride = 0;
      Pylon::ComputeStride(decomp_stride, px_type,
                           static_cast<uint32_t>(comp_info.width),
                           comp_info.paddingX);

      const gsize dst_size =
          static_cast<gsize>(comp_info.width) * comp_info.height * 2;
      ScopedGstBuffer out_buf(gst_buffer_new_allocate(NULL, dst_size, NULL),
                              gst_buffer_unref);
      ScopedGstBufferMap map(out_buf.get(), GST_MAP_WRITE);
      if (!map.IsMapped()) {
        g_set_error(err, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NO_SPACE_LEFT,
                    "Failed to allocate or map output buffer for compressed "
                    "sub-16-bit frame");
        return FALSE;
      }
      gst_pylon_scale_mono_sub16_to_gray16(
          reinterpret_cast<const uint16_t*>(decomp_scratch_buf.data()),
          decomp_stride, reinterpret_cast<uint16_t*>(map.Info().data),
          static_cast<uint32_t>(comp_info.width),
          static_cast<uint32_t>(comp_info.height),
          px_type == Pylon::PixelType_Mono10 ? 10U : 12U);
      map.Unmap();

      effective_stride = static_cast<gsize>(comp_info.width) * 2;
      *buf = out_buf.release();
    } else {
      const gsize dst_size = comp_info.decompressedImageSize;
      ScopedGstBuffer out_buf(gst_buffer_new_allocate(NULL, dst_size, NULL),
                              gst_buffer_unref);
      ScopedGstBufferMap map(out_buf.get(), GST_MAP_WRITE);
      if (!map.IsMapped()) {
        g_set_error(err, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NO_SPACE_LEFT,
                    "Failed to allocate or map output buffer for decompressed "
                    "frame");
        return FALSE;
      }
      size_t out_sz = map.Info().size;
      decompressor.DecompressImage(map.Info().data, &out_sz, grab_result);
      map.Unmap();

      size_t decomp_stride = 0;
      if (Pylon::ComputeStride(decomp_stride, px_type,
                               static_cast<uint32_t>(comp_info.width),
                               comp_info.paddingX)) {
        effective_stride = decomp_stride;
      }
      *buf = out_buf.release();
    }
  } catch (const GenICam::GenericException& e) {
    g_set_error(err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE,
                "Compression Beyond decompression failed: %s",
                e.GetDescription());
    return FALSE;
  }

  try {
    AddResultMeta(*buf, grab_result, effective_stride);
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Failed to attach Pylon meta: %s", e.GetDescription());
    gst_buffer_unref(*buf);
    *buf = NULL;
    return FALSE;
  }

  /* Decompressed data lives in the newly allocated GstBuffer; release Pylon
   * grab buffer back to the stream grabber immediately. */
  return TRUE;
}

gboolean GstPylonSession::Capture(GstBuffer** buf,
                                  GstPylonCaptureErrorEnum capture_error,
                                  GstClockTime* arrival_pts, GError** err) {
  g_return_val_if_fail(buf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  bool retry_grab = true;
  bool buffer_error = false;
  bool keep_failed_grab = false;
  gint retry_frame_counter = 0;
  static const gint max_frames_to_skip = 100;
  GstPylonPendingFrame pending_frame;

  const uint32_t timeout_ms = grab_timeout_ms.load(std::memory_order_relaxed);

  while (retry_grab) {
    GstPylonImageHandlerResult wait_result =
        image_handler.WaitForImage(pending_frame, timeout_ms);

    if (wait_result == GstPylonImageHandlerResult::flushing) {
      return FALSE;
    }
    if (wait_result == GstPylonImageHandlerResult::timeout) {
      g_set_error(err, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
                  "Timed out waiting %u ms for camera frame", timeout_ms);
      return FALSE;
    }
    if (wait_result == GstPylonImageHandlerResult::disconnected) {
      if (auto_reconnect.load(std::memory_order_relaxed)) {
        if (TryReconnect(err)) {
          continue;
        }
        return FALSE;
      }
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Connection to camera was lost");
      return FALSE;
    }

    if (pending_frame.grab->GrabSucceeded()) {
      break;
    }

    std::string error_message =
        std::string(pending_frame.grab->GetErrorDescription());
    const uint32_t error_code = pending_frame.grab->GetErrorCode();
    switch (capture_error) {
      case ENUM_KEEP:
        /* Deliver the buffer into pipeline even if pylon reports an error */
        GST_ELEMENT_WARNING(gstpylonsrc, LIBRARY, FAILED,
                            ("Capture failed. Keeping buffer."),
                            ("%s", error_message.c_str()));
        gst_pylon_post_capture_error_message(gstpylonsrc, "kept",
                                             retry_frame_counter, error_code,
                                             error_message);
        keep_failed_grab = true;
        retry_grab = false;
        break;
      case ENUM_ABORT:
        /* Signal an error to abort pipeline */
        gst_pylon_post_capture_error_message(gstpylonsrc, "aborted",
                                             retry_frame_counter, error_code,
                                             error_message);
        buffer_error = true;
        break;
      case ENUM_SKIP:
        /* Fail if max number of skipped frames is reached */
        if (retry_frame_counter == max_frames_to_skip) {
          error_message = "Max number of allowed buffer skips reached (" +
                          std::to_string(max_frames_to_skip) +
                          "): " + error_message;
          gst_pylon_post_capture_error_message(gstpylonsrc, "aborted",
                                               retry_frame_counter, error_code,
                                               error_message);
          buffer_error = true;
        } else {
          /* Retry to capture next buffer and release current pylon buffer */
          retry_frame_counter += 1;
          GST_ELEMENT_WARNING(gstpylonsrc, LIBRARY, FAILED,
                              ("Capture failed. Skipping buffer."),
                              ("%s", error_message.c_str()));
          gst_pylon_post_capture_error_message(gstpylonsrc, "skipped",
                                               retry_frame_counter, error_code,
                                               error_message);
          pending_frame.grab.Release();
          retry_grab = true;
        }
        break;
      default:
        error_message = "Unknown capture-error policy (" +
                        std::to_string(static_cast<int>(capture_error)) +
                        "): " + error_message;
        buffer_error = true;
        break;
    }

    if (buffer_error) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                  error_message.c_str());
      return FALSE;
    }
  }

  if (arrival_pts) {
    *arrival_pts = pending_frame.arrival_time;
  }

  /* Check for Basler Compression Beyond compressed payloads before accessing
   * GetImageSize() / GetStride(), which throw on compressed grab results. */
  Pylon::CompressionInfo_t comp_info;
  bool is_compressed = false;
  if (pending_frame.grab->GrabSucceeded()) {
    try {
      if (compression_active ||
          pending_frame.grab->GetPayloadType() != Pylon::PayloadType_Image) {
        if (Pylon::CImageDecompressor::GetCompressionInfo(comp_info,
                                                          pending_frame.grab) &&
            comp_info.hasCompressedImage) {
          is_compressed = true;
        }
      }
    } catch (const GenICam::GenericException& e) {
      g_set_error(err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE,
                  "Failed to inspect compressed payload: %s",
                  e.GetDescription());
      return FALSE;
    }
  }

  if (is_compressed) {
    return CaptureCompressed(pending_frame.grab, comp_info, buf, err);
  }

  gsize effective_stride = 0;

  /* Resolve image payload from either standard Image/Chunk payloads or GenDC
   * containers (GeV 3.0 GenDC-only and USB3 optional GenDC). */
  GstPylonResolvedPayload resolved;
  if (!gst_pylon_resolve_grab_payload(pending_frame.grab, keep_failed_grab,
                                      resolved, err)) {
    return FALSE;
  }

  GstPylonBufferRef* wrapped_ref = nullptr;

#ifdef NVMM_ENABLED
  if (MEM_NVMM == mem_type) {
    NvBufSurface* surf =
        reinterpret_cast<NvBufSurface*>(pending_frame.grab->GetBufferContext());

    /* calc src width in byte from pixel type info */
    const auto src_width_pix = resolved.width;
    const auto src_bit_per_pix = Pylon::BitPerPixel(resolved.pixel_type);

    g_assert(0 == (src_width_pix * src_bit_per_pix) % 8);
    const size_t src_width = (src_width_pix * src_bit_per_pix) >> 3;
    const size_t src_stride = resolved.stride > 0 ? resolved.stride : src_width;

    cudaError_t cuda_err =
        cudaMemcpy2D(surf->surfaceList[0].mappedAddr.addr[0],
                     surf->surfaceList[0].pitch, resolved.data, src_stride,
                     src_width, resolved.height, cudaMemcpyDefault);
    if (cuda_err != cudaSuccess) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Error copying memory to device");
      return FALSE;
    }
    NvBufSurfaceSyncForDevice(surf, 0, 0);

    effective_stride = src_stride;
    wrapped_ref = new GstPylonBufferRef{runtime, buffer_factory,
                                        std::move(pending_frame.grab)};
    *buf = gst_buffer_new_wrapped_full(
        GST_MEMORY_FLAG_READONLY, surf, sizeof(*surf), 0, sizeof(*surf),
        wrapped_ref, static_cast<GDestroyNotify>(free_ptr_grab_result));
  } else {
#endif
    const Pylon::EPixelType px_type = resolved.pixel_type;
    if (!keep_failed_grab && (px_type == Pylon::PixelType_Mono10 ||
                              px_type == Pylon::PixelType_Mono12)) {
      const uint32_t width = resolved.width;
      const uint32_t height = resolved.height;
      const size_t src_stride = resolved.stride > 0
                                    ? resolved.stride
                                    : static_cast<size_t>(width) * 2;

      const gsize dst_size = static_cast<gsize>(width) * height * 2;
      ScopedGstBuffer out_buf(gst_buffer_new_allocate(NULL, dst_size, NULL),
                              gst_buffer_unref);
      ScopedGstBufferMap map(out_buf.get(), GST_MAP_WRITE);
      if (!map.IsMapped()) {
        g_set_error(err, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NO_SPACE_LEFT,
                    "Failed to allocate or map output buffer for sub-16-bit "
                    "frame");
        return FALSE;
      }
      gst_pylon_scale_mono_sub16_to_gray16(
          static_cast<const uint16_t*>(resolved.data), src_stride,
          reinterpret_cast<uint16_t*>(map.Info().data), width, height,
          px_type == Pylon::PixelType_Mono10 ? 10U : 12U);
      map.Unmap();

      effective_stride = static_cast<gsize>(width) * 2;
      *buf = out_buf.release();

      try {
        AddResultMeta(*buf, pending_frame.grab, effective_stride);
      } catch (const Pylon::GenericException& e) {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "Failed to attach Pylon meta: %s", e.GetDescription());
        gst_buffer_unref(*buf);
        *buf = NULL;
        return FALSE;
      }

      return TRUE;
    }

    if (resolved.stride > 0) {
      effective_stride = resolved.stride;
    }
    gsize buffer_size = resolved.size;
    wrapped_ref = new GstPylonBufferRef{runtime, buffer_factory,
                                        std::move(pending_frame.grab)};
    *buf = gst_buffer_new_wrapped_full(
        static_cast<GstMemoryFlags>(0), resolved.data, buffer_size, 0,
        buffer_size, wrapped_ref,
        static_cast<GDestroyNotify>(free_ptr_grab_result));
#ifdef NVMM_ENABLED
  }
#endif

  if (keep_failed_grab) {
    GST_BUFFER_FLAG_SET(*buf, GST_BUFFER_FLAG_CORRUPTED);
  }

  try {
    AddResultMeta(*buf, wrapped_ref->grab_result, effective_stride);
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Failed to attach Pylon meta: %s", e.GetDescription());
    gst_buffer_unref(*buf);
    *buf = NULL;
    return FALSE;
  }

  return TRUE;
}
