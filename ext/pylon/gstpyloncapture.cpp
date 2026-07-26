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
#include "gstpylon-private.h"
#include "gstpylon.h"
#include "gstpylondisconnecthandler.h"
#include "gstpylonimagehandler.h"
#include "gstpyloninspect.h"
#include "gstpylonschema.h"
#include "gstpylonsysmembufferfactory.h"

#include <exception>
#include <map>
#include <set>
#include <vector>

static void gst_pylon_add_result_meta(
    GstPylon* self, GstBuffer* buf,
    Pylon::CBaslerUniversalGrabResultPtr& grab_result_ptr) {
  g_return_if_fail(self);
  g_return_if_fail(buf);

  gst_buffer_add_pylon_meta(buf, grab_result_ptr);
}

static void free_ptr_grab_result(gpointer data) {
  g_return_if_fail(data);

  auto wrapped_data = static_cast<GrabResultPair*>(data);

  Pylon::CBaslerUniversalGrabResultPtr* ptr_grab_result = wrapped_data->second;
  delete ptr_grab_result;

  delete wrapped_data;
}

gboolean GstPylon::Capture(GstBuffer** buf,
                           GstPylonCaptureErrorEnum capture_error,
                           GError** err) {
  g_return_val_if_fail(buf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  bool retry_grab = true;
  bool buffer_error = false;
  bool keep_failed_grab = false;
  gint retry_frame_counter = 0;
  static const gint max_frames_to_skip = 100;
  Pylon::CBaslerUniversalGrabResultPtr* grab_result_ptr = NULL;

  while (retry_grab) {
    Pylon::CBaslerUniversalGrabResultPtr* waited = NULL;
    GstPylonImageHandlerResult wait_result =
        image_handler.WaitForImage(&waited);
    grab_result_ptr = waited;

    if (wait_result == GstPylonImageHandlerResult::flushing) {
      return FALSE;
    }
    if (wait_result == GstPylonImageHandlerResult::disconnected) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Connection to camera was lost");
      return FALSE;
    }

    if ((*grab_result_ptr)->GrabSucceeded()) {
      break;
    }

    std::string error_message =
        std::string((*grab_result_ptr)->GetErrorDescription());
    switch (capture_error) {
      case ENUM_KEEP:
        /* Deliver the buffer into pipeline even if pylon reports an error */
        GST_ELEMENT_WARNING(gstpylonsrc, LIBRARY, FAILED,
                            ("Capture failed. Keeping buffer."),
                            ("%s", error_message.c_str()));
        keep_failed_grab = true;
        retry_grab = false;
        break;
      case ENUM_ABORT:
        /* Signal an error to abort pipeline */
        buffer_error = true;
        break;
      case ENUM_SKIP:
        /* Fail if max number of skipped frames is reached */
        if (retry_frame_counter == max_frames_to_skip) {
          error_message = "Max number of allowed buffer skips reached (" +
                          std::to_string(max_frames_to_skip) +
                          "): " + error_message;
          buffer_error = true;
        } else {
          /* Retry to capture next buffer and release current pylon buffer */
          GST_ELEMENT_WARNING(gstpylonsrc, LIBRARY, FAILED,
                              ("Capture failed. Skipping buffer."),
                              ("%s", error_message.c_str()));
          delete grab_result_ptr;
          grab_result_ptr = NULL;
          retry_grab = true;
          retry_frame_counter += 1;
        }
        break;
    };

    if (buffer_error) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                  error_message.c_str());
      delete grab_result_ptr;
      grab_result_ptr = NULL;
      return FALSE;
    }
  };

#ifdef NVMM_ENABLED
  if (MEM_NVMM == mem_type) {
    NvBufSurface* surf =
        reinterpret_cast<NvBufSurface*>((*grab_result_ptr)->GetBufferContext());

    size_t src_stride;
    (*grab_result_ptr)->GetStride(src_stride);

    /* calc src width in byte from pixel type info */
    const auto src_width_pix = (*grab_result_ptr)->GetWidth();
    const auto src_bit_per_pix =
        Pylon::BitPerPixel((*grab_result_ptr)->GetPixelType());

    g_assert(0 == (src_width_pix * src_bit_per_pix) % 8);
    const size_t src_width = (src_width_pix * src_bit_per_pix) >> 3;

    cudaError_t cuda_err = cudaMemcpy2D(
        surf->surfaceList[0].mappedAddr.addr[0], surf->surfaceList[0].pitch,
        (*grab_result_ptr)->GetBuffer(), src_stride, src_width,
        (*grab_result_ptr)->GetHeight(), cudaMemcpyDefault);
    if (cuda_err != cudaSuccess) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Error copying memory to device");
      delete grab_result_ptr;
      return FALSE;
    }

    auto buffer_ref = new GrabResultPair(buffer_factory, grab_result_ptr);
    *buf = gst_buffer_new_wrapped_full(
        GST_MEMORY_FLAG_READONLY, surf, sizeof(*surf), 0, sizeof(*surf),
        buffer_ref, static_cast<GDestroyNotify>(free_ptr_grab_result));
  } else {
#endif
    gsize buffer_size = (*grab_result_ptr)->GetImageSize();
    auto buffer_ref = new GrabResultPair(buffer_factory, grab_result_ptr);
    *buf = gst_buffer_new_wrapped_full(
        static_cast<GstMemoryFlags>(0), (*grab_result_ptr)->GetBuffer(),
        buffer_size, 0, buffer_size, buffer_ref,
        static_cast<GDestroyNotify>(free_ptr_grab_result));
#ifdef NVMM_ENABLED
  }
#endif

  if (keep_failed_grab) {
    GST_BUFFER_FLAG_SET(*buf, GST_BUFFER_FLAG_CORRUPTED);
  }

  try {
    gst_pylon_add_result_meta(this, *buf, *grab_result_ptr);
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Failed to attach Pylon meta: %s", e.GetDescription());
    gst_buffer_unref(*buf);
    *buf = NULL;
    return FALSE;
  }

  return TRUE;
}
