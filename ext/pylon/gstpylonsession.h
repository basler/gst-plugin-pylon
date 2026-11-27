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

#include "gst/pylon/gstpylonformatmapping.h"
#include "gst/pylon/gstpylonobject.h"
#include "gstpylonbufferfactory.h"
#include "gstpylondisconnecthandler.h"
#include "gstpylonimagehandler.h"
#include "gstpylonruntime.h"
#include "gstpylontypes.h"

#include <gst/gst.h>
#include <gst/pylon/gstpylonincludes.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class GstPylonSession {
 public:
  static std::unique_ptr<GstPylonSession> Create(
      GstElement* gstpylonsrc, const gchar* device_user_name,
      const gchar* device_serial_number, gint device_index,
      const gchar* user_set, const gchar* pfs_location,
      gboolean enable_correction, GError** err);

  explicit GstPylonSession(GstElement* element);
  ~GstPylonSession();

  GstPylonSession(const GstPylonSession&) = delete;
  GstPylonSession& operator=(const GstPylonSession&) = delete;

  void SetMaxPendingFrames(guint max_pending_frames);
  void SetGrabTimeout(guint timeout_ms);
  void SetAutoReconnect(gboolean enable);

  gboolean Start(GError** err);
  gboolean Stop(GError** err);
  void InterruptCapture();
  void ClearCaptureInterrupt();
  gboolean Capture(GstBuffer** buf, GstPylonCaptureErrorEnum capture_error,
                   GstClockTime* arrival_pts, GError** err);

  gboolean ApplyUserConfig(const gchar* user_set, GError** err);
  gboolean LoadPfsConfig(const gchar* pfs_location, GError** err);
  gboolean ApplySessionConfig(const gchar* user_set, const gchar* pfs_location,
                              GError** err);
  gboolean EnsureConfigured(const gchar* user_set, const gchar* pfs_location,
                            gboolean enable_correction, GError** err);

  GstCaps* QueryConfiguration(GError** err);
  gboolean SetConfiguration(const GstCaps* conf, GError** err);
  gboolean GetStartupGeometry(gint* start_width, gint* start_height);
  gboolean GetPreferredCapsValues(std::string* preferred_gst_format,
                                  gdouble* preferred_fps);
  gboolean ExecuteCommand(const gchar* command_name, GError** err);

  void SetEnableCorrection(gboolean enable_correction);
  GObject* RefCamera() const;
  GObject* RefStreamGrabber() const;
  gboolean MatchesRequestedDevice(gint device_index,
                                  const gchar* device_user_name,
                                  const gchar* device_serial_number) const;
  gboolean IsConfigApplied(const gchar* user_set, const gchar* pfs_location,
                           gboolean enable_correction) const;

#ifdef NVMM_ENABLED
  void SetNvsurfaceLayout(GstPylonNvsurfaceLayoutEnum layout) {
    nvsurface_layout = layout;
  }
  GstPylonNvsurfaceLayoutEnum GetNvsurfaceLayout() const {
    return nvsurface_layout;
  }
  void SetGpuId(guint id) { gpu_id = id; }
  guint GetGpuId() const { return gpu_id; }
#endif

#ifdef GST_PYLON_ENABLE_TEST_HOOKS
  void TestTriggerDisconnect();
#endif

 private:
  void Open(const gchar* device_user_name, const gchar* device_serial_number,
            gint device_index, const gchar* user_set, const gchar* pfs_location,
            gboolean enable_correction);
  void AttachAndOpen(const Pylon::CDeviceInfo& device_info, int retries);
  void ReleaseDevice();
  gboolean TryReconnect(GError** err);
  gboolean CaptureCompressed(Pylon::CBaslerUniversalGrabResultPtr& grab_result,
                             const Pylon::CompressionInfo_t& comp_info,
                             GstBuffer** buf, GError** err);
  void AddResultMeta(GstBuffer* buf,
                     Pylon::CBaslerUniversalGrabResultPtr& grab_result_ptr,
                     gsize override_stride);
  void InvalidateCapsCache();
  void RefreshDecompressor();

  /* Caps query helpers (called with camera_mutex held) */
  gboolean QueryFormat(GValue* outvalue,
                       PixelFormatMappingView pixel_format_mapping);
  void QueryInteger(GValue* outvalue, const std::string& name);
  void QueryWidth(GValue* outvalue);
  void QueryHeight(GValue* outvalue);
  void QueryFramerate(GValue* outvalue);
  gboolean QueryCaps(GstStructure* st,
                     PixelFormatMappingView pixel_format_mapping);

  /* Destroyed last so PylonTerminate() runs after camera, buffers and handlers
   * are released. */
  std::shared_ptr<GstPylonRuntime> runtime;
  std::shared_ptr<std::recursive_mutex> camera_mutex =
      std::make_shared<std::recursive_mutex>();

  GstElement* gstpylonsrc = nullptr;
  std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera =
      std::make_shared<Pylon::CBaslerUniversalInstantCamera>();
  GObject* gcamera = nullptr;
  GObject* gstream_grabber = nullptr;
  GstPylonImageHandler image_handler;
  GstPylonDisconnectHandler disconnect_handler;

  std::shared_ptr<GstPylonBufferFactory> buffer_factory;
  GstPylonMemoryTypeEnum mem_type = MEM_SYSMEM;

  std::string requested_device_user_name;
  std::string requested_device_serial_number;
  gint requested_device_index = -1;
  bool handlers_registered = false;
  std::string applied_user_set;
  std::string applied_pfs_location;
  gboolean applied_enable_correction = TRUE;
  gboolean has_applied_config = FALSE;

  std::atomic<guint> grab_timeout_ms{0};
  std::atomic<gboolean> auto_reconnect{FALSE};
  GstCaps* negotiated_caps = nullptr;
  GstCaps* cached_caps = nullptr;

  Pylon::CImageDecompressor decompressor;
  bool compression_active = false;
  std::vector<uint8_t> decomp_scratch_buf;

#ifdef NVMM_ENABLED
  GstPylonNvsurfaceLayoutEnum nvsurface_layout = PROP_NVSURFACE_LAYOUT_DEFAULT;
  guint gpu_id = PROP_GPU_ID_DEFAULT;
#endif
};

/* Member order is load-bearing: grab_result is destroyed first, then
 * buffer_factory, then runtime (PylonTerminate). */
struct GstPylonBufferRef {
  std::shared_ptr<GstPylonRuntime> runtime;
  std::shared_ptr<GstPylonBufferFactory> buffer_factory;
  Pylon::CBaslerUniversalGrabResultPtr grab_result;
};

inline constexpr std::array<GstStPixelFormats, 2> gst_structure_formats = {{
    {"video/x-raw", pixel_format_mapping_raw},
    {"video/x-bayer", pixel_format_mapping_bayer},
}};
