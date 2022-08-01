/* Copyright (C) 2022 Basler AG
 *
 * Created by RidgeRun, LLC <support@ridgerun.com>
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

#include "gstpylon.h"

#include "gstchildinspector.h"
#include "gstpyloncamera.h"

#include <map>

#ifdef _MSC_VER  // MSVC
#pragma warning(push)
#pragma warning(disable : 4265)
#elif __GNUC__  // GCC, CLANG, MinGW
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/PylonIncludes.h>

#ifdef _MSC_VER  // MSVC
#pragma warning(pop)
#elif __GNUC__  // GCC, CLANG, MinWG
#pragma GCC diagnostic pop
#endif

/* prototypes */
static void free_ptr_grab_result(gpointer data);
static void gst_pylon_query_format(GstPylon *self, GValue *outvalue);
static void gst_pylon_query_integer(GstPylon *self, GValue *outvalue,
                                    const std::string &name);
static void gst_pylon_query_width(GstPylon *self, GValue *outvalue);
static void gst_pylon_query_height(GstPylon *self, GValue *outvalue);
static void gst_pylon_query_framerate(GstPylon *self, GValue *outvalue);
static std::vector<std::string> gst_pylon_gst_to_pfnc(
    const std::string &gst_format);
static std::vector<std::string> gst_pylon_pfnc_to_gst(
    const std::string &genapi_format);
static std::vector<std::string> gst_pylon_pfnc_list_to_gst(
    GenApi::StringList_t genapi_formats);
static std::string gst_pylon_query_default_set(
    const Pylon::CBaslerUniversalInstantCamera &camera);
static void gst_pylon_append_camera_properties(
    const Pylon::CBaslerUniversalInstantCamera &camera,
    gchar **camera_parameters, guint alignment);

static constexpr gint DEFAULT_ALIGNMENT = 35;

struct _GstPylon {
  std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera =
      std::make_shared<Pylon::CBaslerUniversalInstantCamera>();
  GObject *gcamera;
};

/* pixel format definitions */
typedef struct {
  std::string pfnc_name;
  std::string gst_name;
} PixelFormatMappingType;

static PixelFormatMappingType pixel_format_mapping[] = {
    {"Mono8", "GRAY8"},        {"RGB8Packed", "RGB"}, {"BGR8Packed", "BGR"},
    {"RGB8", "RGB"},           {"BGR8", "BGR"},       {"YCbCr422_8", "YUY2"},
    {"YUV422_8_UYVY", "UYVY"}, {"YUV422_8", "YUY2"}};

void gst_pylon_initialize() { Pylon::PylonInitialize(); }

GstPylon *gst_pylon_new(const gchar *device_user_name,
                        const gchar *device_serial_number, gint device_index,
                        GError **err) {
  GstPylon *self = new GstPylon;

  g_return_val_if_fail(self, NULL);
  g_return_val_if_fail(err && *err == NULL, NULL);

  try {
    Pylon::CTlFactory &factory = Pylon::CTlFactory::GetInstance();
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
          "\"device-index\" to select one from the following list:\n";

      for (gint i = 0; i < n_devices; i++) {
        msg += "[" + std::to_string(i) +
               "]: " + std::string(device_list.at(i).GetUserDefinedName()) +
               " " + std::string(device_list.at(i).GetSerialNumber()) + "\n";
      }
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    if (device_index >= n_devices) {
      std::string msg = "Device index " + std::to_string(device_index) +
                        " exceeds the " + std::to_string(n_devices) +
                        " devices found to match the given criteria";
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    // Only one device was found, we don't require the user specifying an index
    // and if they did, we already checked for out-of-range errors above
    if (1 == n_devices) {
      device_index = 0;
    }

    device_info = device_list.at(device_index);

    self->camera->Attach(factory.CreateDevice(device_info));
    self->camera->Open();

    self->gcamera = gst_pylon_camera_new(self->camera);

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    delete self;
    self = NULL;
  }

  return self;
}

static std::string gst_pylon_query_default_set(
    const Pylon::CBaslerUniversalInstantCamera &camera) {
  std::string set;

  /* Return default for cameras that don't support wake up default sets e.g
   * CamEmulator */
  if (!camera.UserSetDefault.IsReadable() &&
      !camera.UserSetDefaultSelector.IsReadable()) {
    set = "Default";
  } else if (camera.GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
    set = std::string(camera.UserSetDefault.ToString());
  } else {
    set = std::string(camera.UserSetDefaultSelector.ToString());
  }

  return set;
}

gboolean gst_pylon_set_user_config(GstPylon *self, const gchar *user_set,
                                   GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    if (!self->camera->UserSetSelector.IsWritable()) {
      return true;
    }

    std::string set;
    if (user_set) {
      set = std::string(user_set);
    }

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

      for (const auto &value : values) {
        msg += std::string(value) + "\n";
      }
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    self->camera->UserSetLoad.Execute();

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

void gst_pylon_free(GstPylon *self) {
  g_return_if_fail(self);

  self->camera->Close();
  g_object_unref(self->gcamera);

  delete self;
}

gboolean gst_pylon_start(GstPylon *self, GError **err) {
  gboolean ret = TRUE;

  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    self->camera->StartGrabbing();
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    ret = FALSE;
  }

  return ret;
}

gboolean gst_pylon_stop(GstPylon *self, GError **err) {
  gboolean ret = TRUE;

  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    self->camera->StopGrabbing();
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    ret = FALSE;
  }

  return ret;
}

static void free_ptr_grab_result(gpointer data) {
  g_return_if_fail(data);

  Pylon::CBaslerUniversalGrabResultPtr *ptr_grab_result =
      static_cast<Pylon::CBaslerUniversalGrabResultPtr *>(data);
  delete ptr_grab_result;
}

gboolean gst_pylon_capture(GstPylon *self, GstBuffer **buf, GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(buf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  Pylon::CBaslerUniversalGrabResultPtr ptr_grab_result;
  gint timeout_ms = 5000;
  /* Catch the timeout exception if any */
  try {
    self->camera->RetrieveResult(timeout_ms, ptr_grab_result,
                                 Pylon::TimeoutHandling_ThrowException);
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  if (ptr_grab_result->GrabSucceeded() == FALSE) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                ptr_grab_result->GetErrorDescription().c_str());
    return FALSE;
  }

  gsize buffer_size = ptr_grab_result->GetBufferSize();
  Pylon::CBaslerUniversalGrabResultPtr *persistent_ptr_grab_result =
      new Pylon::CBaslerUniversalGrabResultPtr(ptr_grab_result);
  *buf = gst_buffer_new_wrapped_full(
      static_cast<GstMemoryFlags>(0), ptr_grab_result->GetBuffer(), buffer_size,
      0, buffer_size, persistent_ptr_grab_result,
      static_cast<GDestroyNotify>(free_ptr_grab_result));

  return TRUE;
}

static std::vector<std::string> gst_pylon_gst_to_pfnc(
    const std::string &gst_format) {
  std::vector<std::string> out_formats;
  for (auto &entry : pixel_format_mapping) {
    if (entry.gst_name == gst_format) {
      out_formats.push_back(entry.pfnc_name);
    }
  }
  return out_formats;
}

static std::vector<std::string> gst_pylon_pfnc_to_gst(
    const std::string &genapi_format) {
  std::vector<std::string> out_formats;
  for (auto &entry : pixel_format_mapping) {
    if (entry.pfnc_name == genapi_format) {
      out_formats.push_back(entry.gst_name);
    }
  }
  return out_formats;
}

static std::vector<std::string> gst_pylon_pfnc_list_to_gst(
    GenApi::StringList_t genapi_formats) {
  std::vector<std::string> formats_list;

  for (const auto &genapi_fmt : genapi_formats) {
    std::vector<std::string> gst_fmts =
        gst_pylon_pfnc_to_gst(std::string(genapi_fmt));

    /* Insert every matching gst format */
    formats_list.insert(formats_list.end(), gst_fmts.begin(), gst_fmts.end());
  }

  return formats_list;
}

typedef void (*GstPylonQuery)(GstPylon *, GValue *);

static void gst_pylon_query_format(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
  Pylon::CEnumParameter pixelFormat(nodemap, "PixelFormat");

  GenApi::StringList_t genapi_formats;
  pixelFormat.GetSettableValues(genapi_formats);

  /* Convert GenApi formats to Gst formats */
  std::vector<std::string> gst_formats =
      gst_pylon_pfnc_list_to_gst(genapi_formats);

  /* Fill format field */
  g_value_init(outvalue, GST_TYPE_LIST);

  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_STRING);

  for (const auto &fmt : gst_formats) {
    g_value_set_string(&value, fmt.c_str());
    gst_value_list_append_value(outvalue, &value);
  }

  g_value_unset(&value);
}

static void gst_pylon_query_integer(GstPylon *self, GValue *outvalue,
                                    const std::string &name) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
  Pylon::CIntegerParameter param(nodemap, name.c_str());

  gint min = param.GetMin();
  gint max = param.GetMax();

  g_value_init(outvalue, GST_TYPE_INT_RANGE);
  gst_value_set_int_range(outvalue, min, max);
}

static void gst_pylon_query_width(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gst_pylon_query_integer(self, outvalue, "Width");
}

static void gst_pylon_query_height(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gst_pylon_query_integer(self, outvalue, "Height");
}

static void gst_pylon_query_framerate(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gdouble min_fps = 1;
  gdouble max_fps = 1;
  Pylon::CFloatParameter framerate;
  bool has_acquisition_framerate = false;

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();

  if (self->camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
    framerate.Attach(nodemap, "AcquisitionFrameRate");
  } else {
    framerate.Attach(nodemap, "AcquisitionFrameRateAbs");
  }

  if (framerate.IsReadable()) {
    min_fps = framerate.GetMin();
    max_fps = framerate.GetMax();
    has_acquisition_framerate = true;
  }

  if (has_acquisition_framerate) {
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
  }
}

GstCaps *gst_pylon_query_configuration(GstPylon *self, GError **err) {
  g_return_val_if_fail(self, NULL);
  g_return_val_if_fail(err && *err == NULL, NULL);

  /* Build gst caps */
  GstCaps *caps = gst_caps_new_empty();
  GstStructure *st = gst_structure_new_empty("video/x-raw");
  GValue value = G_VALUE_INIT;

  const std::vector<std::pair<GstPylonQuery, const std::string>> queries = {
      {gst_pylon_query_format, "format"},
      {gst_pylon_query_width, "width"},
      {gst_pylon_query_height, "height"},
      {gst_pylon_query_framerate, "framerate"}};

  try {
    /* Offsets are set to 0 to get the true image geometry */
    self->camera->OffsetX.TrySetToMinimum();
    self->camera->OffsetY.TrySetToMinimum();

    for (const auto &query : queries) {
      GstPylonQuery func = query.first;
      const gchar *name = query.second.c_str();

      func(self, &value);
      gst_structure_set_value(st, name, &value);
      g_value_unset(&value);
    }
  } catch (const Pylon::GenericException &e) {
    gst_structure_free(st);
    gst_caps_unref(caps);

    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return NULL;
  }

  gst_caps_append_structure(caps, st);

  return caps;
}

gboolean gst_pylon_set_configuration(GstPylon *self, const GstCaps *conf,
                                     GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(conf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  GstStructure *st = gst_caps_get_structure(conf, 0);

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
  Pylon::CEnumParameter pixelFormat(nodemap, "PixelFormat");

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

    const std::vector<std::string> pfnc_formats =
        gst_pylon_gst_to_pfnc(gst_format);

    /* In case of ambiguous format mapping choose first */
    bool fmt_valid = false;
    for (auto &fmt : pfnc_formats) {
      fmt_valid = pixelFormat.TrySetValue(fmt.c_str());
      if (fmt_valid) break;
    }

    if (!fmt_valid) {
      throw Pylon::GenericException(
          std::string("Unsupported GStreamer format: " + gst_format).c_str(),
          __FILE__, __LINE__);
    }

    Pylon::CIntegerParameter width(nodemap, "Width");
    width.SetValue(gst_width, Pylon::IntegerValueCorrection_None);

    Pylon::CIntegerParameter height(nodemap, "Height");
    height.SetValue(gst_height, Pylon::IntegerValueCorrection_None);

    Pylon::CBooleanParameter framerate_enable(nodemap,
                                              "AcquisitionFrameRateEnable");

    /* Basler dart gen1 models have no framerate_enable feature */
    framerate_enable.TrySetValue(true);

    gdouble div = 1.0 * gst_numerator / gst_denominator;

    if (self->camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
      Pylon::CFloatParameter framerate(nodemap, "AcquisitionFrameRate");
      framerate.TrySetValue(div, Pylon::FloatValueCorrection_None);
    } else {
      Pylon::CFloatParameter framerate(nodemap, "AcquisitionFrameRateAbs");
      framerate.TrySetValue(div, Pylon::FloatValueCorrection_None);
    }

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

static void gst_pylon_append_camera_properties(
    const Pylon::CBaslerUniversalInstantCamera &camera,
    gchar **camera_parameters, guint alignment) {
  g_return_if_fail(camera_parameters);

  GType camera_type = gst_pylon_camera_register(camera);
  GObject *camera_obj = G_OBJECT(g_object_new(camera_type, NULL));

  gchar *camera_name = g_strdup_printf(
      "%*s:\n", alignment, camera.GetDeviceInfo().GetFriendlyName().c_str());

  gchar *parameters = gst_child_inspector_properties_to_string(
      camera_obj, alignment, camera_name);

  if (NULL == *camera_parameters) {
    *camera_parameters = g_strdup(parameters);
  } else {
    *camera_parameters =
        g_strconcat(*camera_parameters, "\n", parameters, NULL);
  }

  g_free(camera_name);
  g_free(parameters);

  g_object_unref(camera_obj);
}

gchar *gst_pylon_get_string_properties(GError **err) {
  g_return_val_if_fail(err && *err == NULL, NULL);

  gchar *camera_parameters = NULL;

  try {
    Pylon::CTlFactory &factory = Pylon::CTlFactory::GetInstance();
    Pylon::DeviceInfoList_t device_list;

    factory.EnumerateDevices(device_list);

    for (const auto &device : device_list) {
      Pylon::CBaslerUniversalInstantCamera camera(factory.CreateDevice(device),
                                                  Pylon::Cleanup_Delete);
      camera.Open();
      gst_pylon_append_camera_properties(camera, &camera_parameters,
                                         DEFAULT_ALIGNMENT);
      camera.Close();
    }

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return NULL;
  }

  return camera_parameters;
}

GObject *gst_pylon_get_camera(GstPylon *self) {
  g_return_val_if_fail(self, NULL);

  return G_OBJECT(g_object_ref(self->gcamera));
}
