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

/* retry open camera limits in case of collision with other process */
constexpr int FAILED_OPEN_RETRY_COUNT = 30;
constexpr int FAILED_OPEN_RETRY_WAIT_TIME_MS = 1000;

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

  const bool open_by_filter_directly =
      (device_serial_number != nullptr || device_user_name != nullptr) &&
      device_index == -1;

  if (open_by_filter_directly) {
    GST_INFO_OBJECT(gstpylonsrc,
                    "Opening device directly by filter without enumeration");
  } else {
    factory.EnumerateDevices(device_list, filter);
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

  if (open_by_filter_directly) {
    try {
      camera->Attach(factory.CreateDevice(device_info));
      camera->Open();
    } catch (GenICam::GenericException& e) {
      std::string msg = "No devices found matching the specified criteria";
      msg += ": ";
      msg += e.GetDescription();
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } else {
    std::string last_error;
    bool attached = false;
    for (auto retry_idx = 0; retry_idx <= FAILED_OPEN_RETRY_COUNT;
         retry_idx++) {
      try {
        camera->Attach(factory.CreateDevice(device_info));
        camera->Open();
        attached = true;
        break;
      } catch (GenICam::GenericException& e) {
        last_error = e.GetDescription();
        GST_INFO_OBJECT(gstpylonsrc, "Failed to Open %s (%s)\n",
                        device_info.GetSerialNumber().c_str(),
                        e.GetDescription());
        try {
          if (camera->IsPylonDeviceAttached()) {
            camera->DestroyDevice();
          }
        } catch (const GenICam::GenericException&) {
        }
        g_usleep(FAILED_OPEN_RETRY_WAIT_TIME_MS * 1000);
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

  if (camera->UserSetSelector.IsWritable()) {
    std::string default_set = "Auto";
    gst_pylon_apply_set(this, default_set);
    GST_INFO_OBJECT(gstpylonsrc, "Applied default user set %s",
                    default_set.c_str());
  }

  GstPylonCache camera_feature_cache(
      gst_pylon_camera_schema_cache_key(*camera));
  GstPylonObjectSchema camera_schema =
      gst_pylon_make_camera_schema(*camera, camera_feature_cache);
  gcamera =
      gst_pylon_object_new_for_schema(camera, camera_schema, enable_correction);
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

gboolean GstPylon::Start(GError** err) {
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (camera->IsGrabbing()) {
    return TRUE;
  }

  try {
    camera->StartGrabbing(Pylon::GrabStrategy_LatestImageOnly,
                          Pylon::GrabLoop_ProvidedByInstantCamera);
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

  *start_height = camera->Height.GetValue();
  *start_width = camera->Width.GetValue();

  return TRUE;
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
