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
#include "gstchildinspector.h"
#include "gstpylon.h"
#include "gstpylondisconnecthandler.h"
#include "gstpylonimagehandler.h"
#include "gstpylonsysmembufferfactory.h"

#include <exception>
#include <map>
#include <set>
#include <vector>

/* retry open camera limits in case of collision with other
 * process
 */
constexpr int FAILED_OPEN_RETRY_COUNT = 30;
constexpr int FAILED_OPEN_RETRY_WAIT_TIME_MS = 1000;

/* Mapping of GstStructure with its corresponding formats */
typedef struct {
  const std::string st_name;
  std::vector<PixelFormatMappingType> format_map;
} GstStPixelFormats;

typedef enum {
  MEM_SYSMEM,
  MEM_NVMM,
} GstPylonMemoryTypeEnum;

/* prototypes */
static std::string gst_pylon_query_default_set(
    const Pylon::CBaslerUniversalInstantCamera& camera);
static void gst_pylon_apply_set(GstPylon* self, std::string& set);
static std::string gst_pylon_get_camera_fullname(
    Pylon::CBaslerUniversalInstantCamera& camera);
static std::string gst_pylon_get_sgrabber_name(
    Pylon::CBaslerUniversalInstantCamera& camera);
static GstPylonObjectSchema gst_pylon_get_camera_schema(
    Pylon::CBaslerUniversalInstantCamera& camera, GstPylonCache& feature_cache);
static GstPylonObjectSchema gst_pylon_get_stream_schema(
    Pylon::CBaslerUniversalInstantCamera& camera, GstPylonCache& feature_cache);
static void free_ptr_grab_result(gpointer data);
static void gst_pylon_query_format(
    GstPylon* self, GValue* outvalue,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping);
static void gst_pylon_query_integer(GstPylon* self, GValue* outvalue,
                                    const std::string& name);
static void gst_pylon_query_width(GstPylon* self, GValue* outvalue);
static void gst_pylon_query_height(GstPylon* self, GValue* outvalue);
static void gst_pylon_query_framerate(GstPylon* self, GValue* outvalue);
static void gst_pylon_query_caps(
    GstPylon* self, GstStructure* st,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping);
static void gst_pylon_add_result_meta(
    GstPylon* self, GstBuffer* buf,
    Pylon::CBaslerUniversalGrabResultPtr& grab_result_ptr);
static std::vector<std::string> gst_pylon_gst_to_pfnc(
    const std::string& gst_format,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping);
static std::vector<std::string> gst_pylon_pfnc_to_gst(
    const std::string& genapi_format,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping);
static std::vector<std::string> gst_pylon_pfnc_list_to_gst(
    const GenApi::StringList_t& genapi_formats,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping);
static void gst_pylon_append_properties(
    Pylon::CBaslerUniversalInstantCamera* camera,
    const GstPylonObjectSchema& schema, const std::string& device_type_str,
    gchar** device_properties, guint alignment);
static constexpr gint DEFAULT_ALIGNMENT = 35;

/* Combined introspection: one device pass, cache, one output per schema */
static void gst_pylon_get_introspection_strings_impl(gchar** cam_out,
                                                     gchar** stream_out);

struct _GstPylon {
  explicit _GstPylon(GstElement* element)
      : gstpylonsrc(element),
        gcamera(nullptr),
        gstream_grabber(nullptr),
        mem_type(MEM_SYSMEM),
        requested_device_index(-1),
        handlers_registered(false) {
#ifdef NVMM_ENABLED
    nvsurface_layout = PROP_NVSURFACE_LAYOUT_DEFAULT;
    gpu_id = PROP_GPU_ID_DEFAULT;
#endif
  }

  ~_GstPylon() {
    if (handlers_registered) {
      camera->DeregisterImageEventHandler(&image_handler);
      camera->DeregisterConfiguration(&disconnect_handler);
    }

    if (camera->IsOpen()) {
      camera->Close();
    }

    g_clear_object(&gcamera);
    g_clear_object(&gstream_grabber);
  }

  void Open(const gchar* device_user_name, const gchar* device_serial_number,
            gint device_index, gboolean enable_correction);
  gboolean Start(GError** err);
  gboolean Stop(GError** err);
  gboolean ApplyUserConfig(const gchar* user_set, GError** err);
  gboolean LoadPfsConfig(const gchar* pfs_location, GError** err);
  gboolean GetStartupGeometry(gint* start_width, gint* start_height);
  gboolean Capture(GstBuffer** buf, GstPylonCaptureErrorEnum capture_error,
                   GError** err);
  GstCaps* QueryConfiguration(GError** err);
  gboolean SetConfiguration(const GstCaps* conf, GError** err);
  void InterruptCapture();
  GObject* RefCamera() const;
  GObject* RefStreamGrabber() const;
  gboolean MatchesRequestedDevice(gint device_index,
                                  const gchar* device_user_name,
                                  const gchar* device_serial_number) const;

  GstElement* gstpylonsrc;
  std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera =
      std::make_shared<Pylon::CBaslerUniversalInstantCamera>();
  GObject* gcamera;
  GObject* gstream_grabber;
  GstPylonImageHandler image_handler;
  GstPylonDisconnectHandler disconnect_handler;

  std::shared_ptr<GstPylonBufferFactory> buffer_factory;
  GstPylonMemoryTypeEnum mem_type;

  std::string requested_device_user_name;
  std::string requested_device_serial_number;
  gint requested_device_index;
  bool handlers_registered;

#ifdef NVMM_ENABLED
  GstPylonNvsurfaceLayoutEnum nvsurface_layout;
  guint gpu_id;
#endif
};

using GrabResultPair = std::pair<std::shared_ptr<GstPylonBufferFactory>,
                                 Pylon::CBaslerUniversalGrabResultPtr*>;

static const std::vector<GstStPixelFormats> gst_structure_formats = {
    {"video/x-raw", pixel_format_mapping_raw},
    {"video/x-bayer", pixel_format_mapping_bayer}};

static std::string gst_pylon_get_camera_fullname(
    Pylon::CBaslerUniversalInstantCamera& camera) {
  return std::string(camera.GetDeviceInfo().GetFullName());
}

static std::string gst_pylon_get_sgrabber_name(
    Pylon::CBaslerUniversalInstantCamera& camera) {
  return gst_pylon_get_camera_fullname(camera) + " StreamGrabber";
}

static std::string gst_pylon_get_camera_schema_cache_key(
    Pylon::CBaslerUniversalInstantCamera& camera) {
  return std::string(camera.DeviceModelName.GetValue() + "_" +
                     camera.DeviceFirmwareVersion.GetValue() + "_" + VERSION);
}

static std::string gst_pylon_get_stream_schema_cache_key(
    Pylon::CBaslerUniversalInstantCamera& camera) {
  return std::string(camera.GetDeviceInfo().GetModelName() + "_" +
                     Pylon::GetPylonVersionString() + "_" + VERSION);
}

static GstPylonObjectSchema gst_pylon_get_camera_schema(
    Pylon::CBaslerUniversalInstantCamera& camera,
    GstPylonCache& feature_cache) {
  return {gst_pylon_get_camera_fullname(camera),
          gst_pylon_get_camera_schema_cache_key(camera), feature_cache,
          camera.GetNodeMap()};
}

static GstPylonObjectSchema gst_pylon_get_stream_schema(
    Pylon::CBaslerUniversalInstantCamera& camera,
    GstPylonCache& feature_cache) {
  return {gst_pylon_get_sgrabber_name(camera),
          gst_pylon_get_stream_schema_cache_key(camera), feature_cache,
          camera.GetStreamGrabberNodeMap()};
}

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

  Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();
  Pylon::DeviceInfoList_t filter(1);
  Pylon::DeviceInfoList_t device_list;
  Pylon::CDeviceInfo device_info;

  if (device_user_name) {
    filter[0].SetUserDefinedName(device_user_name);
  }

  if (device_serial_number) {
    filter[0].SetSerialNumber(device_serial_number);
  }

  factory.EnumerateDevices(device_list, filter);
  GST_INFO_OBJECT(gstpylonsrc, "Enumerated %d matching devices",
                  static_cast<gint>(device_list.size()));

  gint n_devices = device_list.size();
  if (0 == n_devices) {
    throw Pylon::GenericException(
        "No devices found matching the specified criteria", __FILE__, __LINE__);
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
  GST_INFO_OBJECT(gstpylonsrc, "Selected device %s",
                  device_info.GetSerialNumber().c_str());

  for (auto retry_idx = 0; retry_idx <= FAILED_OPEN_RETRY_COUNT; retry_idx++) {
    try {
      camera->Attach(factory.CreateDevice(device_info));
      break;
    } catch (GenICam::GenericException& e) {
      GST_INFO_OBJECT(gstpylonsrc, "Failed to Open %s (%s)\n",
                      device_info.GetSerialNumber().c_str(),
                      e.GetDescription());
      g_usleep(FAILED_OPEN_RETRY_WAIT_TIME_MS * 1000);
    }
  }
  camera->Open();
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
      gst_pylon_get_camera_schema_cache_key(*camera));
  GstPylonObjectSchema camera_schema =
      gst_pylon_get_camera_schema(*camera, camera_feature_cache);
  gcamera = gst_pylon_object_new_for_schema(
      camera, camera_schema, &camera_schema.nodemap, enable_correction);
  GST_INFO_OBJECT(gstpylonsrc, "Created camera child object");

  GstPylonCache stream_feature_cache(
      gst_pylon_get_stream_schema_cache_key(*camera));
  GstPylonObjectSchema stream_schema =
      gst_pylon_get_stream_schema(*camera, stream_feature_cache);
  gstream_grabber = gst_pylon_object_new_for_schema(
      camera, stream_schema, &stream_schema.nodemap, enable_correction);
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

      return TRUE;
    }

    std::string set;
    if (user_set) {
      set = std::string(user_set);
    }

    gst_pylon_apply_set(this, set);
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

  static const bool check_nodemap_sanity = true;

  try {
    Pylon::CFeaturePersistence::Load(pfs_location, &camera->GetNodeMap(),
                                     check_nodemap_sanity);
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

GstPylon* gst_pylon_new(GstElement* gstpylonsrc, const gchar* device_user_name,
                        const gchar* device_serial_number, gint device_index,
                        gboolean enable_correction, GError** err) {
  GstPylon* self = new GstPylon(gstpylonsrc);

  g_return_val_if_fail(self, NULL);
  g_return_val_if_fail(err && *err == NULL, NULL);

  try {
    self->Open(device_user_name, device_serial_number, device_index,
               enable_correction);
  } catch (const GenICam::GenericException& e) {
    GST_ERROR_OBJECT(gstpylonsrc, "gst_pylon_new failed opening camera: %s",
                     e.GetDescription());
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    delete self;
    self = NULL;
  } catch (const std::exception& e) {
    GST_ERROR_OBJECT(gstpylonsrc, "gst_pylon_new caught std::exception: %s",
                     e.what());
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.what());
    delete self;
    self = NULL;
  }

  return self;
}

gboolean gst_pylon_set_user_config(GstPylon* self, const gchar* user_set,
                                   GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->ApplyUserConfig(user_set, err);
}

gboolean gst_pylon_get_startup_geometry(GstPylon* self, gint* start_width,
                                        gint* start_height) {
  g_return_val_if_fail(self, FALSE);
  return self->GetStartupGeometry(start_width, start_height);
}

gboolean gst_pylon_set_pfs_config(GstPylon* self, const gchar* pfs_location,
                                  GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->LoadPfsConfig(pfs_location, err);
}

void gst_pylon_free(GstPylon* self) {
  g_return_if_fail(self);
  delete self;
}

gboolean gst_pylon_start(GstPylon* self, GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->Start(err);
}

gboolean gst_pylon_stop(GstPylon* self, GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->Stop(err);
}

void gst_pylon_interrupt_capture(GstPylon* self) {
  g_return_if_fail(self);
  self->InterruptCapture();
}

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
  gint retry_frame_counter = 0;
  static const gint max_frames_to_skip = 100;
  Pylon::CBaslerUniversalGrabResultPtr* grab_result_ptr = NULL;

  while (retry_grab) {
    grab_result_ptr = image_handler.WaitForImage();

    /* Return if user requests to interrupt the grabbing thread */
    if (!grab_result_ptr) {
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

  gst_pylon_add_result_meta(this, *buf, *grab_result_ptr);

  return TRUE;
}

gboolean gst_pylon_capture(GstPylon* self, GstBuffer** buf,
                           GstPylonCaptureErrorEnum capture_error,
                           GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->Capture(buf, capture_error, err);
}

static std::vector<std::string> gst_pylon_gst_to_pfnc(
    const std::string& gst_format,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping) {
  std::vector<std::string> out_formats;
  for (auto& entry : pixel_format_mapping) {
    if (entry.gst_name == gst_format) {
      out_formats.push_back(entry.pfnc_name);
    }
  }
  return out_formats;
}

static std::vector<std::string> gst_pylon_pfnc_to_gst(
    const std::string& genapi_format,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping) {
  std::vector<std::string> out_formats;
  for (auto& entry : pixel_format_mapping) {
    if (entry.pfnc_name == genapi_format) {
      out_formats.push_back(entry.gst_name);
    }
  }
  return out_formats;
}

static std::vector<std::string> gst_pylon_pfnc_list_to_gst(
    const GenApi::StringList_t& genapi_formats,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping) {
  std::vector<std::string> formats_list;

  for (const auto& genapi_fmt : genapi_formats) {
    std::vector<std::string> gst_fmts =
        gst_pylon_pfnc_to_gst(std::string(genapi_fmt), pixel_format_mapping);

    /* Insert every matching gst format */
    formats_list.insert(formats_list.end(), gst_fmts.begin(), gst_fmts.end());
  }

  return formats_list;
}

typedef void (*GstPylonQuery)(GstPylon*, GValue*);

static void gst_pylon_query_format(
    GstPylon* self, GValue* outvalue,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  GenApi::INodeMap& nodemap = self->camera->GetNodeMap();
  Pylon::CEnumParameter pixelformat(nodemap, "PixelFormat");

  GenApi::StringList_t genapi_formats;
  pixelformat.GetSettableValues(genapi_formats);

  /* Convert GenApi formats to Gst formats */
  std::vector<std::string> gst_formats =
      gst_pylon_pfnc_list_to_gst(genapi_formats, pixel_format_mapping);

  /* Fill format field */
  g_value_init(outvalue, GST_TYPE_LIST);

  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_STRING);

  for (const auto& fmt : gst_formats) {
    g_value_set_string(&value, fmt.c_str());
    gst_value_list_append_value(outvalue, &value);
  }

  g_value_unset(&value);
}

static void gst_pylon_query_integer(GstPylon* self, GValue* outvalue,
                                    const std::string& name) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  GenApi::INodeMap& nodemap = self->camera->GetNodeMap();
  Pylon::CIntegerParameter param(nodemap, name.c_str());

  gint min = param.GetMin();
  gint max = param.GetMax();

  g_value_init(outvalue, GST_TYPE_INT_RANGE);
  gst_value_set_int_range(outvalue, min, max);
}

static void gst_pylon_query_width(GstPylon* self, GValue* outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gst_pylon_query_integer(self, outvalue, "Width");
}

static void gst_pylon_query_height(GstPylon* self, GValue* outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gst_pylon_query_integer(self, outvalue, "Height");
}

static void gst_pylon_query_framerate(GstPylon* self, GValue* outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gdouble min_fps = 1;
  gdouble max_fps = 1;
  Pylon::CFloatParameter framerate;

  GenApi::INodeMap& nodemap = self->camera->GetNodeMap();

  if (self->camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
    framerate.Attach(nodemap, "AcquisitionFrameRate");
  } else {
    framerate.Attach(nodemap, "AcquisitionFrameRateAbs");
  }

  if (framerate.IsReadable()) {
    min_fps = framerate.GetMin();
    max_fps = framerate.GetMax();

    gint min_fps_num = 0;
    gint min_fps_den = 0;
    gst_util_double_to_fraction(min_fps, &min_fps_num, &min_fps_den);

    gint max_fps_num = 0;
    gint max_fps_den = 0;
    gst_util_double_to_fraction(max_fps, &max_fps_num, &max_fps_den);

    g_value_init(outvalue, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range_full(outvalue, min_fps_num, min_fps_den,
                                      max_fps_num, max_fps_den);
  } else {
    /* Fallback framerate 0, if camera does not supply any value */
    g_value_init(outvalue, GST_TYPE_FRACTION);
    gst_value_set_fraction(outvalue, 0, 1);
    GST_INFO(
        "AcquisitionFramerate feature not available"
        " camera will report 0/1 as supported framerate");
  }
}

static void gst_pylon_query_caps(
    GstPylon* self, GstStructure* st,
    const std::vector<PixelFormatMappingType>& pixel_format_mapping) {
  g_return_if_fail(self);
  g_return_if_fail(st);

  GValue value = G_VALUE_INIT;

  /* Save offset to later reset values after querying */
  gint64 orig_offset_x = self->camera->OffsetX.GetValue();
  gint64 orig_offset_y = self->camera->OffsetY.GetValue();

  const std::vector<std::pair<GstPylonQuery, const std::string>> queries = {
      {gst_pylon_query_width, "width"},
      {gst_pylon_query_height, "height"},
      {gst_pylon_query_framerate, "framerate"}};

  /* Offsets are set to 0 to get the true image geometry */
  self->camera->OffsetX.TrySetToMinimum();
  self->camera->OffsetY.TrySetToMinimum();

  /* Pixel format is queried separately to support querying different pixel
   * format mappings */
  gst_pylon_query_format(self, &value, pixel_format_mapping);
  gst_structure_set_value(st, "format", &value);
  g_value_unset(&value);

  for (const auto& query : queries) {
    GstPylonQuery func = query.first;
    const gchar* name = query.second.c_str();

    func(self, &value);
    gst_structure_set_value(st, name, &value);
    g_value_unset(&value);
  }

  /* Reset offset after querying */
  self->camera->OffsetX.TrySetValue(orig_offset_x);
  self->camera->OffsetY.TrySetValue(orig_offset_y);
}

GstCaps* GstPylon::QueryConfiguration(GError** err) {
  g_return_val_if_fail(err && *err == NULL, NULL);

  /* Build gst caps */
  GstCaps* caps = gst_caps_new_empty();

  for (const auto& gst_structure_format : gst_structure_formats) {
    GstStructure* st =
        gst_structure_new_empty(gst_structure_format.st_name.c_str());
    try {
      gst_pylon_query_caps(this, st, gst_structure_format.format_map);
      gst_caps_append_structure(caps, st);

#ifdef NVMM_ENABLED
      /* We need the copy since the append has taken ownership of the "old" st
       */
      gst_caps_append_structure_full(
          caps, gst_structure_copy(st),
          gst_caps_features_new("memory:NVMM", NULL));
#endif

    } catch (const Pylon::GenericException& e) {
      gst_structure_free(st);
      gst_caps_unref(caps);

      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                  e.GetDescription());
      return NULL;
    }
  }

  return caps;
}

GstCaps* gst_pylon_query_configuration(GstPylon* self, GError** err) {
  g_return_val_if_fail(self, NULL);
  return self->QueryConfiguration(err);
}

gboolean GstPylon::SetConfiguration(const GstCaps* conf, GError** err) {
  g_return_val_if_fail(conf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  GstStructure* st = gst_caps_get_structure(conf, 0);

  GenApi::INodeMap& nodemap = camera->GetNodeMap();
  Pylon::CEnumParameter pixelformat(nodemap, "PixelFormat");

  try {
    const std::string gst_format = gst_structure_get_string(st, "format");
    if (gst_format.empty()) {
      throw Pylon::GenericException(
          "Unable to find the format in the configuration", __FILE__, __LINE__);
    }

    gint gst_width = 0;
    if (!gst_structure_get_int(st, "width", &gst_width)) {
      throw Pylon::GenericException(
          "Unable to find the width in the configuration", __FILE__, __LINE__);
    }

    gint gst_height = 0;
    if (!gst_structure_get_int(st, "height", &gst_height)) {
      throw Pylon::GenericException(
          "Unable to find the height in the configuration", __FILE__, __LINE__);
    }

    gint gst_numerator = 0;
    gint gst_denominator = 0;
    if (!gst_structure_get_fraction(st, "framerate", &gst_numerator,
                                    &gst_denominator)) {
      throw Pylon::GenericException(
          "Unable to find the framerate in the configuration", __FILE__,
          __LINE__);
    }

    bool fmt_valid = false;
    for (const auto& gst_structure_format : gst_structure_formats) {
      const std::vector<std::string> pfnc_formats =
          gst_pylon_gst_to_pfnc(gst_format, gst_structure_format.format_map);

      /* In case of ambiguous format mapping choose first */
      for (auto& fmt : pfnc_formats) {
        fmt_valid = pixelformat.TrySetValue(fmt.c_str());
        if (fmt_valid) break;
      }
    }

    if (!fmt_valid) {
      throw Pylon::GenericException(
          std::string("Unsupported GStreamer format: " + gst_format).c_str(),
          __FILE__, __LINE__);
    }

    Pylon::CIntegerParameter width(nodemap, "Width");
    width.SetValue(gst_width, Pylon::IntegerValueCorrection_None);
    GST_INFO("Set Feature Width: %d", gst_width);

    Pylon::CIntegerParameter height(nodemap, "Height");
    height.SetValue(gst_height, Pylon::IntegerValueCorrection_None);
    GST_INFO("Set Feature Height: %d", gst_height);

    /* set the cached offsetx/y values
     * respect the rounding value adjustment rules
     * -> offset will be adjusted to keep dimensions
     */

    GstPylonObjectPrivate* cam_properties =
        (GstPylonObjectPrivate*)gst_pylon_object_get_instance_private(
            reinterpret_cast<GstPylonObject*>(gcamera));

    auto& offsetx_cache = cam_properties->dimension_cache.offsetx;
    auto& offsety_cache = cam_properties->dimension_cache.offsety;
    auto enable_correction = cam_properties->enable_correction;

    bool value_corrected = false;
    if (offsetx_cache >= 0) {
      Pylon::CIntegerParameter offsetx(nodemap, "OffsetX");
      if (enable_correction) {
        try {
          offsetx.SetValue(
              offsetx_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_None);
        } catch (GenICam::OutOfRangeException&) {
          offsetx.SetValue(
              offsetx_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_Nearest);
          value_corrected = true;
        }
      } else {
        offsetx.SetValue(offsetx_cache);
      }
      GST_INFO("Set Feature OffsetX: %d %s",
               static_cast<gint>(offsetx.GetValue()),
               value_corrected ? " [corrected]" : "");
      offsetx_cache = -1;
    }

    value_corrected = false;
    if (offsety_cache >= 0) {
      Pylon::CIntegerParameter offsety(nodemap, "OffsetY");
      if (enable_correction) {
        try {
          offsety.SetValue(
              offsety_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_None);
        } catch (GenICam::OutOfRangeException&) {
          offsety.SetValue(
              offsety_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_Nearest);
          value_corrected = true;
        }
      } else {
        offsety.SetValue(offsety_cache);
      }
      GST_INFO("Set Feature Offsety: %d %s",
               static_cast<gint>(offsety.GetValue()),
               value_corrected ? " [corrected]" : "");
      offsety_cache = -1;
    }

    Pylon::CBooleanParameter framerate_enable(nodemap,
                                              "AcquisitionFrameRateEnable");

    /* Basler dart gen1 models have no framerate_enable feature */
    framerate_enable.TrySetValue(true);

    gdouble div = 1.0 * gst_numerator / gst_denominator;
    if (camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
      Pylon::CFloatParameter framerate(nodemap, "AcquisitionFrameRate");
      framerate.TrySetValue(div, Pylon::FloatValueCorrection_None);
      GST_INFO("Set Feature AcquisitionFrameRate: %f", div);
    } else {
      Pylon::CFloatParameter framerate(nodemap, "AcquisitionFrameRateAbs");
      framerate.TrySetValue(div, Pylon::FloatValueCorrection_None);
      GST_INFO("Set Feature AcquisitionFrameRateAbs: %f", div);
    }

    guint64 maxnumbuffers = 0;
    g_object_get(gstream_grabber, "MaxNumBuffer", &maxnumbuffers, nullptr);
    camera->MaxNumBuffer.TrySetValue(maxnumbuffers);

#ifdef NVMM_ENABLED
    GstCapsFeatures* features = gst_caps_get_features(conf, 0);
    if (gst_caps_features_contains(features, "memory:NVMM")) {
      buffer_factory = std::make_shared<GstPylonDsNvmmBufferFactory>(
          nvsurface_layout, gpu_id);

      buffer_factory->SetConfig(conf);
      mem_type = MEM_NVMM;
    } else {
#endif
      buffer_factory = std::make_shared<GstPylonSysMemBufferFactory>();
      mem_type = MEM_SYSMEM;
#ifdef NVMM_ENABLED
    }
#endif

    camera->SetBufferFactory(buffer_factory.get(), Pylon::Cleanup_None);

    return TRUE;
  } catch (const Pylon::GenericException& e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }
}

gboolean gst_pylon_set_configuration(GstPylon* self, const GstCaps* conf,
                                     GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->SetConfiguration(conf, err);
}

static void gst_pylon_append_properties(
    Pylon::CBaslerUniversalInstantCamera* camera,
    const GstPylonObjectSchema& schema, const std::string& device_type_str,
    gchar** device_properties, guint alignment) {
  g_return_if_fail(camera);
  g_return_if_fail(device_properties);

  GType device_type = gst_pylon_object_register_schema(schema);
  GObject* device_obj = G_OBJECT(g_object_new(device_type, NULL));

  gchar* device_name = g_strdup_printf(
      "%*s %s:\n", alignment, camera->GetDeviceInfo().GetFriendlyName().c_str(),
      device_type_str.c_str());

  gchar* properties = gst_child_inspector_properties_to_string(
      device_obj, alignment, device_name);

  if (NULL == *device_properties) {
    *device_properties = g_strdup(properties);
  } else {
    *device_properties =
        g_strconcat(*device_properties, "\n", properties, NULL);
  }

  g_free(device_name);
  g_free(properties);

  g_object_unref(device_obj);
}

static gchar* gst_pylon_get_camera_properties_block(
    Pylon::CBaslerUniversalInstantCamera* camera, guint alignment) {
  g_return_val_if_fail(camera, NULL);

  GstPylonCache feature_cache(gst_pylon_get_camera_schema_cache_key(*camera));
  GstPylonObjectSchema schema =
      gst_pylon_get_camera_schema(*camera, feature_cache);
  gchar* block = NULL;
  gst_pylon_append_properties(camera, schema, "Camera", &block, alignment);
  return block;
}

static gchar* gst_pylon_get_stream_properties_block(
    Pylon::CBaslerUniversalInstantCamera* camera, guint alignment) {
  g_return_val_if_fail(camera, NULL);

  GstPylonCache feature_cache(gst_pylon_get_stream_schema_cache_key(*camera));
  GstPylonObjectSchema schema =
      gst_pylon_get_stream_schema(*camera, feature_cache);
  gchar* block = NULL;
  gst_pylon_append_properties(camera, schema, "Stream Grabber", &block,
                              alignment);
  return block;
}

static void gst_pylon_get_introspection_strings_impl(gchar** cam_out,
                                                     gchar** stream_out) {
  gchar* camera_properties = NULL;
  gchar* stream_properties = NULL;

  Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();
  Pylon::DeviceInfoList_t device_list;
  factory.EnumerateDevices(device_list);

  /* One block per device (old behavior). Blocks have device-specific content
   * (e.g. GType enum names), so we build each; introspection cache only helps
   * when a single device is present. */
  for (const auto& device : device_list) {
    try {
      Pylon::CBaslerUniversalInstantCamera camera(factory.CreateDevice(device),
                                                  Pylon::Cleanup_Delete);
      camera.Open();

      /* Set the camera to a valid state */
      camera.DeviceFeaturePersistenceEnd.TryExecute();
      camera.DeviceRegistersStreamingEnd.TryExecute();

      /* Load factory default set (gst-inspect always uses Default) */
      if (camera.UserSetSelector.IsWritable()) {
        camera.UserSetSelector.SetValue("Default");
        camera.UserSetLoad.Execute();
      }

      const std::string camera_key =
          gst_pylon_get_camera_schema_cache_key(camera);
      const std::string stream_key =
          gst_pylon_get_stream_schema_cache_key(camera);

      /* Camera: build block (use cache only when single device for this schema)
       */
      gchar* cam_block = NULL;
      if (device_list.size() == 1) {
        gchar* cached = GstPylonCache::GetIntrospection(camera_key);
        if (cached) {
          cam_block = cached;
        }
      }
      if (!cam_block) {
        cam_block =
            gst_pylon_get_camera_properties_block(&camera, DEFAULT_ALIGNMENT);
        if (device_list.size() == 1) {
          GstPylonCache::SetIntrospection(
              camera_key, std::string(cam_block ? cam_block : ""));
        }
      }
      if (cam_block) {
        if (camera_properties) {
          gchar* tmp = g_strconcat(camera_properties, "\n", cam_block, NULL);
          g_free(camera_properties);
          g_free(cam_block);
          camera_properties = tmp;
        } else {
          camera_properties = cam_block;
        }
      }

      /* Stream: same */
      gchar* stream_block = NULL;
      if (device_list.size() == 1) {
        gchar* cached = GstPylonCache::GetIntrospection(stream_key);
        if (cached) {
          stream_block = cached;
        }
      }
      if (!stream_block) {
        stream_block =
            gst_pylon_get_stream_properties_block(&camera, DEFAULT_ALIGNMENT);
        if (device_list.size() == 1) {
          GstPylonCache::SetIntrospection(
              stream_key, std::string(stream_block ? stream_block : ""));
        }
      }
      if (stream_block) {
        if (stream_properties) {
          gchar* tmp = g_strconcat(stream_properties, "\n", stream_block, NULL);
          g_free(stream_properties);
          g_free(stream_block);
          stream_properties = tmp;
        } else {
          stream_properties = stream_block;
        }
      }

      camera.Close();
    } catch (const Pylon::GenericException&) {
      continue;
    }
  }

  *cam_out = camera_properties ? camera_properties : g_strdup("");
  *stream_out = stream_properties ? stream_properties : g_strdup("");
}

static void gst_pylon_get_introspection_strings(gchar** cam_out,
                                                gchar** stream_out) {
  static gchar* cached_cam = NULL;
  static gchar* cached_stream = NULL;

  if (cached_cam == NULL) {
    gst_pylon_get_introspection_strings_impl(&cached_cam, &cached_stream);
  }
  *cam_out = g_strdup(cached_cam ? cached_cam : "");
  *stream_out = g_strdup(cached_stream ? cached_stream : "");
}

gchar* gst_pylon_camera_get_string_properties() {
  gchar* cam = NULL;
  gchar* stream = NULL;
  gst_pylon_get_introspection_strings(&cam, &stream);
  g_free(stream);
  return cam;
}

gchar* gst_pylon_stream_grabber_get_string_properties() {
  gchar* cam = NULL;
  gchar* stream = NULL;
  gst_pylon_get_introspection_strings(&cam, &stream);
  g_free(cam);
  return stream;
}

GObject* gst_pylon_get_camera(GstPylon* self) {
  g_return_val_if_fail(self, NULL);
  return self->RefCamera();
}

GObject* gst_pylon_get_stream_grabber(GstPylon* self) {
  g_return_val_if_fail(self, NULL);
  return self->RefStreamGrabber();
}

gboolean gst_pylon_is_same_device(GstPylon* self, const gint device_index,
                                  const gchar* device_user_name,
                                  const gchar* device_serial_number) {
  g_return_val_if_fail(self, FALSE);
  return self->MatchesRequestedDevice(device_index, device_user_name,
                                      device_serial_number);
}

#ifdef NVMM_ENABLED
void gst_pylon_set_nvsurface_layout(
    GstPylon* self, const GstPylonNvsurfaceLayoutEnum nvsurface_layout) {
  g_return_if_fail(self);

  self->nvsurface_layout = nvsurface_layout;
}

GstPylonNvsurfaceLayoutEnum gst_pylon_get_nvsurface_layout(GstPylon* self) {
  g_return_val_if_fail(self, PROP_NVSURFACE_LAYOUT_DEFAULT);

  return self->nvsurface_layout;
}

void gst_pylon_set_gpu_id(GstPylon* self, const gint gpu_id) {
  g_return_if_fail(self);

  self->gpu_id = gpu_id;
}

guint gst_pylon_get_gpu_id(GstPylon* self) {
  g_return_val_if_fail(self, PROP_GPU_ID_DEFAULT);

  return self->gpu_id;
}
#endif
