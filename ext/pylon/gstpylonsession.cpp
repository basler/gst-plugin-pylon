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
#include "gst/pylon/gstpylontlenumerate.h"
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

/* retry open camera limits in case of collision with other process */
constexpr int FAILED_OPEN_RETRY_COUNT = 30;
constexpr int FAILED_OPEN_RETRY_WAIT_TIME_MS = 350;

namespace {

int gst_pylon_get_open_retry_count() {
  const gchar* env = g_getenv("PYLONSRC_OPEN_RETRIES");
  if (env && env[0] != '\0') {
    char* endptr = nullptr;
    const long parsed = std::strtol(env, &endptr, 10);
    if (endptr != env && parsed >= 0 && parsed <= 600) {
      return static_cast<int>(parsed);
    }
  }
  return FAILED_OPEN_RETRY_COUNT;
}

gulong gst_pylon_jittered_backoff_us(int retry_idx) {
  const int base_ms =
      std::min(FAILED_OPEN_RETRY_WAIT_TIME_MS, 100 + retry_idx * 50);
  const int jitter_ms = g_random_int_range(0, 125);
  return static_cast<gulong>(base_ms + jitter_ms) * 1000UL;
}

}  // namespace

const std::vector<GstStPixelFormats> gst_structure_formats = {
    {"video/x-raw", pixel_format_mapping_raw},
    {"video/x-bayer", pixel_format_mapping_bayer}};

static std::string gst_pylon_query_default_set(
    const Pylon::CBaslerUniversalInstantCamera& camera) {
  std::string set;

  /* Return default for cameras that don't support wake up default sets e.g
   * CamEmulator */
  if (!camera.UserSetDefault.IsReadable() &&
      !camera.UserSetDefaultSelector.IsReadable()) {
    set = "Default";
  } else if (camera.UserSetDefault.IsReadable()) {
    set = std::string(camera.UserSetDefault.ToString());
  } else {
    set = std::string(camera.UserSetDefaultSelector.ToString());
  }

  return set;
}

static void gst_pylon_apply_set(GstPylon* self, std::string& set) {
  g_return_if_fail(self);

  /* If auto or nothing is set, return default config */
  if ("Auto" == set || set.empty()) {
    set = gst_pylon_query_default_set(*self->camera);
  }

  if (self->camera->UserSetSelector.CanSetValue(set.c_str())) {
    self->camera->UserSetSelector.SetValue(set.c_str());
  } else {
    GenApi::StringList_t values;
    self->camera->UserSetSelector.GetSettableValues(values);
    std::string msg = "Invalid user set, has to be one of the following:\n";
    msg += "Auto\n";

    for (const auto& value : values) {
      msg += std::string(value) + "\n";
    }
    throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }

  self->camera->UserSetLoad.Execute();
}

void GstPylon::Open(const gchar* device_user_name,
                    const gchar* device_serial_number, gint device_index,
                    const gchar* user_set, const gchar* pfs_location,
                    gboolean enable_correction) {
  requested_device_index = device_index;
  requested_device_user_name = device_user_name ? device_user_name : "";
  requested_device_serial_number =
      device_serial_number ? device_serial_number : "";
  has_applied_config = FALSE;
  applied_user_set = "";
  applied_pfs_location = "";
  applied_enable_correction = enable_correction;

  Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();
  Pylon::DeviceInfoList_t filter(1);
  Pylon::DeviceInfoList_t device_list;
  Pylon::CDeviceInfo device_info;

  if (device_user_name) {
    filter[0].SetUserDefinedName(device_user_name);
    device_info.SetUserDefinedName(device_user_name);
  }

  if (device_serial_number) {
    filter[0].SetSerialNumber(device_serial_number);
    device_info.SetSerialNumber(device_serial_number);
  }

  const bool skip_tl_active = []() {
    const gchar* env = g_getenv("PYLONSRC_SKIP_TRANSPORT_LAYERS");
    return env && env[0] != '\0';
  }();

  const bool open_by_filter_directly =
      !skip_tl_active &&
      (device_serial_number != nullptr || device_user_name != nullptr) &&
      device_index == -1;

  if (open_by_filter_directly) {
    GST_INFO_OBJECT(gstpylonsrc,
                    "Opening device directly by filter without enumeration");
  } else {
    gst_pylon_enumerate_devices(device_list, &filter);
    GST_INFO_OBJECT(gstpylonsrc, "Enumerated %d matching devices",
                    static_cast<gint>(device_list.size()));

    gint n_devices = device_list.size();
    if (0 == n_devices) {
      throw Pylon::GenericException(
          "No devices found matching the specified criteria", __FILE__,
          __LINE__);
    }

    if (n_devices > 1 && -1 == device_index) {
      std::string msg =
          "At least " + std::to_string(n_devices) +
          " devices match the specified criteria, use "
          "\"device-index\", \"device-serial-number\" or \"device-user-name\""
          " to select one from the following list:\n";

      for (gint i = 0; i < n_devices; i++) {
        msg += "[" + std::to_string(i) +
               "]: " + std::string(device_list.at(i).GetSerialNumber()) + "\t" +
               std::string(device_list.at(i).GetModelName()) + "\t" +
               std::string(device_list.at(i).GetUserDefinedName()) + "\n";
      }
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    if (device_index >= n_devices) {
      std::string msg = "Device index " + std::to_string(device_index) +
                        " exceeds the " + std::to_string(n_devices) +
                        " devices found to match the given criteria";
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    if (1 == n_devices) {
      device_index = 0;
    }

    device_info = device_list.at(device_index);
  }

  GST_INFO_OBJECT(gstpylonsrc, "Selected device %s",
                  device_info.GetSerialNumber().c_str());

  const int max_open_retries = gst_pylon_get_open_retry_count();

  if (open_by_filter_directly) {
    bool attached = false;
    std::string last_error;
    try {
      camera->Attach(factory.CreateDevice(device_info));
      camera->Open();
      attached = true;
    } catch (const GenICam::GenericException& e) {
      last_error = e.GetDescription();
      try {
        if (camera->IsPylonDeviceAttached()) {
          camera->DestroyDevice();
        }
      } catch (const GenICam::GenericException& cleanup_err) {
        GST_DEBUG_OBJECT(gstpylonsrc,
                         "Ignoring device cleanup error after open failure: %s",
                         cleanup_err.GetDescription());
      }
    }

    if (!attached) {
      /* Distinguish a non-existent device (fail immediately) from a physical
       * device that is present on the bus but temporarily locked or contended
       * during multi-process startup/handoff. */
      Pylon::DeviceInfoList_t matching_devices;
      gst_pylon_enumerate_devices(matching_devices, &filter);

      if (matching_devices.empty()) {
        std::string msg =
            "No devices found matching the specified criteria: " + last_error;
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
      }

      if (matching_devices.size() > 1) {
        std::string msg = "At least " +
                          std::to_string(matching_devices.size()) +
                          " devices match the specified criteria, use "
                          "\"device-index\", \"device-serial-number\" or "
                          "\"device-user-name\" to select one unambiguously";
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
      }

      device_info = matching_devices.at(0);
      for (int retry_idx = 0; retry_idx < max_open_retries; ++retry_idx) {
        g_usleep(gst_pylon_jittered_backoff_us(retry_idx));
        try {
          camera->Attach(factory.CreateDevice(device_info));
          camera->Open();
          attached = true;
          break;
        } catch (const GenICam::GenericException& e) {
          last_error = e.GetDescription();
          GST_INFO_OBJECT(gstpylonsrc, "Retrying open of %s (%d/%d): %s",
                          device_info.GetSerialNumber().c_str(), retry_idx + 1,
                          max_open_retries, e.GetDescription());
          try {
            if (camera->IsPylonDeviceAttached()) {
              camera->DestroyDevice();
            }
          } catch (const GenICam::GenericException& cleanup_err) {
            GST_DEBUG_OBJECT(
                gstpylonsrc,
                "Ignoring device cleanup error during open retry: %s",
                cleanup_err.GetDescription());
          }
        }
      }

      if (!attached) {
        throw Pylon::GenericException(last_error.c_str(), __FILE__, __LINE__);
      }
    }
  } else {
    std::string last_error;
    bool attached = false;
    for (int retry_idx = 0; retry_idx <= max_open_retries; retry_idx++) {
      try {
        camera->Attach(factory.CreateDevice(device_info));
        camera->Open();
        attached = true;
        break;
      } catch (const GenICam::GenericException& e) {
        last_error = e.GetDescription();
        GST_INFO_OBJECT(gstpylonsrc, "Failed to Open %s (%s)\n",
                        device_info.GetSerialNumber().c_str(),
                        e.GetDescription());
        try {
          if (camera->IsPylonDeviceAttached()) {
            camera->DestroyDevice();
          }
        } catch (const GenICam::GenericException& cleanup_err) {
          GST_DEBUG_OBJECT(
              gstpylonsrc,
              "Ignoring device cleanup error during open retry: %s",
              cleanup_err.GetDescription());
        }
        if (retry_idx < max_open_retries) {
          g_usleep(gst_pylon_jittered_backoff_us(retry_idx));
        }
      }
    }

    if (!attached) {
      throw Pylon::GenericException(last_error.c_str(), __FILE__, __LINE__);
    }
  }

  GST_INFO_OBJECT(gstpylonsrc, "Camera opened");

  camera->DeviceFeaturePersistenceEnd.TryExecute();
  camera->DeviceRegistersStreamingEnd.TryExecute();
  GST_INFO_OBJECT(gstpylonsrc, "Camera reset to sane state");

  /* Apply initial user-set and/or PFS once at open time instead of loading
   * "Auto" here and immediately reloading in ApplySessionConfig(). */
  if (camera->UserSetSelector.IsWritable()) {
    std::string initial_set = user_set ? user_set : "Auto";
    gst_pylon_apply_set(this, initial_set);
    GST_INFO_OBJECT(gstpylonsrc, "Applied initial user set %s",
                    initial_set.c_str());
  }
  applied_user_set = user_set ? user_set : "";
  applied_pfs_location = "";
  has_applied_config = TRUE;

  bool pfs_loaded = false;
  if (pfs_location) {
    static const bool check_nodemap_sanity = false;
    try {
      Pylon::CFeaturePersistence::Load(pfs_location, &camera->GetNodeMap(),
                                       check_nodemap_sanity);
      applied_pfs_location = pfs_location;
      pfs_loaded = true;
      GST_INFO_OBJECT(gstpylonsrc, "Loaded initial PFS file %s", pfs_location);
    } catch (const Pylon::GenericException& e) {
      std::string msg = "PFS file error: ";
      msg += e.GetDescription();
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  }

  GstPylonCache camera_feature_cache(
      gst_pylon_camera_schema_cache_key(*camera));
  GstPylonObjectSchema camera_schema =
      gst_pylon_make_camera_schema(*camera, camera_feature_cache);
  gcamera =
      gst_pylon_object_new_for_schema(camera, camera_schema, enable_correction);
  if (pfs_loaded) {
    gst_pylon_object_mark_framerate_configured(gcamera);
  }
  gst_object_set_parent(GST_OBJECT(gcamera), GST_OBJECT(gstpylonsrc));
  g_object_ref(gcamera);
  GST_INFO_OBJECT(gstpylonsrc, "Created camera child object");

  GstPylonCache stream_feature_cache(
      gst_pylon_stream_schema_cache_key(*camera));
  GstPylonObjectSchema stream_schema =
      gst_pylon_make_stream_schema(*camera, stream_feature_cache);
  gstream_grabber =
      gst_pylon_object_new_for_schema(camera, stream_schema, enable_correction);
  gst_object_set_parent(GST_OBJECT(gstream_grabber), GST_OBJECT(gstpylonsrc));
  g_object_ref(gstream_grabber);
  GST_INFO_OBJECT(gstpylonsrc, "Created stream grabber child object");

  camera->RegisterImageEventHandler(
      &image_handler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);
  disconnect_handler.SetData(gstpylonsrc, &image_handler);
  camera->RegisterConfiguration(
      &disconnect_handler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);
  handlers_registered = true;
  GST_INFO_OBJECT(gstpylonsrc, "Registered camera event handlers");
}

void GstPylon::SetMaxPendingFrames(guint max_pending_frames) {
  image_handler.SetMaxPendingFrames(max_pending_frames);
}

void GstPylon::SetGrabTimeout(guint timeout_ms) {
  grab_timeout_ms.store(timeout_ms, std::memory_order_relaxed);
}

void GstPylon::SetAutoReconnect(gboolean enable) {
  auto_reconnect.store(enable, std::memory_order_relaxed);
}

void GstPylon::RefreshDecompressor() {
  compression_active = false;
  try {
    GenApi::INodeMap& nodemap = camera->GetNodeMap();
    const Pylon::ECompressionMode mode =
        Pylon::CImageDecompressor::GetCompressionMode(nodemap);
    if (mode != Pylon::CompressionMode_Off) {
      decompressor.SetCompressionDescriptor(nodemap);
      compression_active = true;
      GST_INFO_OBJECT(gstpylonsrc,
                      "Initialized Pylon CImageDecompressor (mode=%d)",
                      static_cast<int>(mode));
    } else {
      decompressor.ResetCompressionDescriptor();
    }
  } catch (const GenICam::GenericException& e) {
    /* Cameras without Compression Beyond support or disabled descriptors */
    GST_DEBUG_OBJECT(gstpylonsrc, "Compression Beyond not active: %s",
                     e.GetDescription());
    decompressor.ResetCompressionDescriptor();
    compression_active = false;
  }
}

gboolean GstPylon::TryReconnect(GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  constexpr int RECONNECT_MAX_ATTEMPTS = 15;
  constexpr int RECONNECT_DELAY_MS = 1000;

  GST_WARNING_OBJECT(gstpylonsrc,
                     "Camera disconnected; attempting auto-reconnect...");

  if (gstpylonsrc) {
    GstStructure* s = gst_structure_new("pylonsrc-reconnect", "status",
                                        G_TYPE_STRING, "reconnecting", NULL);
    gst_element_post_message(
        gstpylonsrc, gst_message_new_element(GST_OBJECT(gstpylonsrc), s));
  }

  try {
    if (camera->IsGrabbing()) {
      camera->StopGrabbing();
    }
    if (camera->IsOpen()) {
      camera->Close();
    }
    if (camera->IsPylonDeviceAttached()) {
      camera->DestroyDevice();
    }
  } catch (const GenICam::GenericException& cleanup_err) {
    GST_DEBUG_OBJECT(gstpylonsrc,
                     "Ignoring teardown error before reconnect: %s",
                     cleanup_err.GetDescription());
  }

  Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();
  Pylon::CDeviceInfo filter_info;
  if (!requested_device_user_name.empty()) {
    filter_info.SetUserDefinedName(requested_device_user_name.c_str());
  }
  if (!requested_device_serial_number.empty()) {
    filter_info.SetSerialNumber(requested_device_serial_number.c_str());
  }

  std::string last_error = "Camera did not reappear within reconnect window";
  for (int attempt = 1; attempt <= RECONNECT_MAX_ATTEMPTS; ++attempt) {
    try {
      Pylon::DeviceInfoList_t filter(1);
      filter[0] = filter_info;
      Pylon::DeviceInfoList_t list;
      gst_pylon_enumerate_devices(list, &filter);
      if (!list.empty()) {
        const size_t idx =
            (requested_device_index >= 0 &&
             static_cast<size_t>(requested_device_index) < list.size())
                ? static_cast<size_t>(requested_device_index)
                : 0;
        camera->Attach(factory.CreateDevice(list.at(idx)));
        camera->Open();

        camera->DeviceFeaturePersistenceEnd.TryExecute();
        camera->DeviceRegistersStreamingEnd.TryExecute();

        if (!applied_user_set.empty() || camera->UserSetSelector.IsWritable()) {
          std::string set_to_apply =
              applied_user_set.empty() ? "Auto" : applied_user_set;
          if (camera->UserSetSelector.IsWritable()) {
            gst_pylon_apply_set(this, set_to_apply);
          }
        }
        if (!applied_pfs_location.empty()) {
          Pylon::CFeaturePersistence::Load(applied_pfs_location.c_str(),
                                           &camera->GetNodeMap(), false);
        }

        /* Rebind child GObject nodemap pointers to the reopened device */
        if (gcamera) {
          auto* cam_priv = static_cast<GstPylonObjectPrivate*>(
              gst_pylon_object_get_instance_private(GST_PYLON_OBJECT(gcamera)));
          cam_priv->nodemap = &camera->GetNodeMap();
        }
        if (gstream_grabber) {
          auto* stream_priv = static_cast<GstPylonObjectPrivate*>(
              gst_pylon_object_get_instance_private(
                  GST_PYLON_OBJECT(gstream_grabber)));
          stream_priv->nodemap = &camera->GetStreamGrabberNodeMap();
        }

        image_handler.ResetForReconnect();

        if (negotiated_caps) {
          GError* cfg_err = NULL;
          if (!SetConfiguration(negotiated_caps, &cfg_err)) {
            if (cfg_err) {
              last_error = cfg_err->message;
              g_error_free(cfg_err);
            }
            throw Pylon::GenericException(last_error.c_str(), __FILE__,
                                          __LINE__);
          }
        }

        GError* start_err = NULL;
        if (!Start(&start_err)) {
          if (start_err) {
            last_error = start_err->message;
            g_error_free(start_err);
          }
          throw Pylon::GenericException(last_error.c_str(), __FILE__, __LINE__);
        }

        if (gstpylonsrc) {
          GstStructure* s = gst_structure_new(
              "pylonsrc-reconnect", "status", G_TYPE_STRING, "reconnected",
              "attempt", G_TYPE_INT, attempt, NULL);
          gst_element_post_message(
              gstpylonsrc, gst_message_new_element(GST_OBJECT(gstpylonsrc), s));
        }
        GST_INFO_OBJECT(gstpylonsrc,
                        "Successfully reconnected camera on attempt %d",
                        attempt);
        return TRUE;
      }
    } catch (const GenICam::GenericException& e) {
      last_error = e.GetDescription();
      try {
        if (camera->IsPylonDeviceAttached()) {
          camera->DestroyDevice();
        }
      } catch (const GenICam::GenericException& cleanup_err) {
        GST_DEBUG_OBJECT(
            gstpylonsrc,
            "Ignoring device cleanup error after reconnect attempt: %s",
            cleanup_err.GetDescription());
      }
    }

    g_usleep(RECONNECT_DELAY_MS * 1000);
  }

  g_set_error(err, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
              "Connection to camera was lost and auto-reconnect failed: %s",
              last_error.c_str());
  return FALSE;
}

gboolean GstPylon::Start(GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (camera->IsGrabbing()) {
    return TRUE;
  }

  try {
    RefreshDecompressor();

    /* A frame queue needs every frame from pylon, in order */
    const size_t max_pending_frames = image_handler.GetMaxPendingFrames();
    const Pylon::EGrabStrategy strategy =
        max_pending_frames > 1 ? Pylon::GrabStrategy_OneByOne
                               : Pylon::GrabStrategy_LatestImageOnly;

    /* Pending frames hold pylon grab buffers */
    if (max_pending_frames > 1 && camera->MaxNumBuffer.IsReadable()) {
      const gint64 max_num_buffer = camera->MaxNumBuffer.GetValue();
      if (static_cast<gint64>(max_pending_frames) >= max_num_buffer) {
        GST_WARNING_OBJECT(
            gstpylonsrc,
            "max-pending-frames (%" G_GSIZE_FORMAT
            ") should be well below stream::MaxNumBuffer (%" G_GINT64_FORMAT
            ")",
            static_cast<gsize>(max_pending_frames), max_num_buffer);
      }
    }

    camera->StartGrabbing(strategy, Pylon::GrabLoop_ProvidedByInstantCamera);
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean GstPylon::Stop(GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (!camera->IsGrabbing()) {
    return TRUE;
  }

  try {
    camera->StopGrabbing();
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean GstPylon::ApplyUserConfig(const gchar* user_set, GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    if (!camera->UserSetSelector.IsWritable()) {
      GST_INFO(
          "UserSet feature not available"
          " camera will start in internal default state");
      applied_user_set = user_set ? user_set : "";
      applied_pfs_location = "";
      has_applied_config = TRUE;

      return TRUE;
    }

    std::string set;
    if (user_set) {
      set = std::string(user_set);
    }

    gst_pylon_apply_set(this, set);
    applied_user_set = user_set ? user_set : "";
    applied_pfs_location = "";
    has_applied_config = TRUE;
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean GstPylon::LoadPfsConfig(const gchar* pfs_location, GError** err) {
  g_return_val_if_fail(pfs_location, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  static const bool check_nodemap_sanity = false;

  try {
    Pylon::CFeaturePersistence::Load(pfs_location, &camera->GetNodeMap(),
                                     check_nodemap_sanity);
    gst_pylon_object_mark_framerate_configured(gcamera);
    applied_pfs_location = pfs_location;
    has_applied_config = TRUE;
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "PFS file error: %s", e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean GstPylon::GetStartupGeometry(gint* start_width, gint* start_height) {
  g_return_val_if_fail(start_height, FALSE);
  g_return_val_if_fail(start_width, FALSE);

  /* Honor cached Width/Height set via cam::Width / cam::Height before start */
  if (gcamera) {
    auto* cam_priv = static_cast<GstPylonObjectPrivate*>(
        gst_pylon_object_get_instance_private(GST_PYLON_OBJECT(gcamera)));
    *start_width = cam_priv->dimension_cache.width >= 0
                       ? cam_priv->dimension_cache.width
                       : static_cast<gint>(camera->Width.GetValue());
    *start_height = cam_priv->dimension_cache.height >= 0
                        ? cam_priv->dimension_cache.height
                        : static_cast<gint>(camera->Height.GetValue());
    return TRUE;
  }

  *start_height = camera->Height.GetValue();
  *start_width = camera->Width.GetValue();

  return TRUE;
}

gboolean GstPylon::GetPreferredCapsValues(std::string* preferred_gst_format,
                                          gdouble* preferred_fps) {
  if (!camera || !camera->IsOpen()) {
    return FALSE;
  }

  try {
    GenApi::INodeMap& nodemap = camera->GetNodeMap();
    /* Prefer PFS/UserSet configured PixelFormat when explicitly configured */
    if (preferred_gst_format &&
        (!applied_pfs_location.empty() || !applied_user_set.empty())) {
      Pylon::CEnumParameter pf(nodemap, "PixelFormat");
      if (pf.IsReadable()) {
        const std::string cur_pfnc = pf.GetValue().c_str();
        for (const auto& st_fmt : gst_structure_formats) {
          for (const auto& entry : st_fmt.format_map) {
            if (entry.pfnc_name == cur_pfnc) {
              *preferred_gst_format = entry.gst_name;
              break;
            }
          }
          if (!preferred_gst_format->empty()) {
            break;
          }
        }
      }
    }

    if (preferred_fps && gcamera &&
        gst_pylon_object_is_framerate_configured(gcamera)) {
      Pylon::CFloatParameter fps;
      if (camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
        fps.Attach(nodemap, "AcquisitionFrameRate");
      } else {
        fps.Attach(nodemap, "AcquisitionFrameRateAbs");
      }
      if (fps.IsReadable()) {
        *preferred_fps = fps.GetValue();
      }
    }
  } catch (const GenICam::GenericException&) {
    return FALSE;
  }

  return TRUE;
}

gboolean GstPylon::ExecuteCommand(const gchar* command_name, GError** err) {
  if (!gcamera) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Camera child object is not initialized");
    return FALSE;
  }
  return gst_pylon_object_execute_command(gcamera, command_name, err);
}

void GstPylon::InterruptCapture() { image_handler.InterruptWaitForImage(); }

void GstPylon::ClearCaptureInterrupt() { image_handler.ClearInterrupt(); }

void GstPylon::SetEnableCorrection(gboolean enable_correction) {
  applied_enable_correction = enable_correction;
  gst_pylon_object_set_enable_correction(gcamera, enable_correction);
  gst_pylon_object_set_enable_correction(gstream_grabber, enable_correction);
}

GObject* GstPylon::RefCamera() const { return G_OBJECT(g_object_ref(gcamera)); }

GObject* GstPylon::RefStreamGrabber() const {
  return G_OBJECT(g_object_ref(gstream_grabber));
}

gboolean GstPylon::MatchesRequestedDevice(
    gint device_index, const gchar* device_user_name,
    const gchar* device_serial_number) const {
  std::string user_name = device_user_name ? device_user_name : "";
  std::string serial_number = device_serial_number ? device_serial_number : "";

  return requested_device_index == device_index &&
         requested_device_user_name == user_name &&
         requested_device_serial_number == serial_number;
}

gboolean GstPylon::IsConfigApplied(const gchar* user_set,
                                   const gchar* pfs_location,
                                   gboolean enable_correction) const {
  if (!has_applied_config) {
    return FALSE;
  }

  const std::string wanted_user_set = user_set ? user_set : "";
  const std::string wanted_pfs_location = pfs_location ? pfs_location : "";

  return applied_user_set == wanted_user_set &&
         applied_pfs_location == wanted_pfs_location &&
         applied_enable_correction == enable_correction;
}

gboolean GstPylon::ApplySessionConfig(const gchar* user_set,
                                      const gchar* pfs_location, GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (!ApplyUserConfig(user_set, err)) {
    return FALSE;
  }

  if (!pfs_location) {
    return TRUE;
  }

  return LoadPfsConfig(pfs_location, err);
}

gboolean GstPylon::EnsureConfigured(const gchar* user_set,
                                    const gchar* pfs_location,
                                    gboolean enable_correction, GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (IsConfigApplied(user_set, pfs_location, enable_correction)) {
    return TRUE;
  }

  SetEnableCorrection(enable_correction);
  return ApplySessionConfig(user_set, pfs_location, err);
}
