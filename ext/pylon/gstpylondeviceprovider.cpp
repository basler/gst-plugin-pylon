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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gst/pylon/gstpylondebug.h"
#include "gst/pylon/gstpylontlenumerate.h"
#include "gstpylondeviceprovider.h"
#include "gstpylonsrc.h"

#include <gst/pylon/gstpylonincludes.h>
#include <gst/video/video.h>

#include <string>

struct _GstPylonDevice {
  GstDevice parent;
  gchar* serial_number;
  gchar* user_name;
  gint fallback_index;
};

G_DEFINE_TYPE(GstPylonDevice, gst_pylon_device, GST_TYPE_DEVICE)

static GstElement* gst_pylon_device_create_element(GstDevice* device,
                                                   const gchar* name) {
  GstPylonDevice* self = GST_PYLON_DEVICE(device);
  GstElement* elem = gst_element_factory_make("pylonsrc", name);
  if (!elem) {
    return NULL;
  }

  if (self->serial_number && self->serial_number[0] != '\0') {
    g_object_set(elem, "device-serial-number", self->serial_number, NULL);
  } else if (self->user_name && self->user_name[0] != '\0') {
    g_object_set(elem, "device-user-name", self->user_name, NULL);
  } else if (self->fallback_index >= 0) {
    g_object_set(elem, "device-index", self->fallback_index, NULL);
  }

  return elem;
}

static gboolean gst_pylon_device_reconfigure_element(GstDevice* device,
                                                     GstElement* element) {
  GstPylonDevice* self = GST_PYLON_DEVICE(device);
  if (!GST_IS_PYLON_SRC(element)) {
    return FALSE;
  }

  if (self->serial_number && self->serial_number[0] != '\0') {
    g_object_set(element, "device-serial-number", self->serial_number,
                 "device-user-name", NULL, "device-index", -1, NULL);
    return TRUE;
  }
  if (self->user_name && self->user_name[0] != '\0') {
    g_object_set(element, "device-serial-number", NULL, "device-user-name",
                 self->user_name, "device-index", -1, NULL);
    return TRUE;
  }
  if (self->fallback_index >= 0) {
    g_object_set(element, "device-serial-number", NULL, "device-user-name",
                 NULL, "device-index", self->fallback_index, NULL);
    return TRUE;
  }

  return FALSE;
}

static void gst_pylon_device_finalize(GObject* object) {
  GstPylonDevice* self = GST_PYLON_DEVICE(object);
  g_free(self->serial_number);
  g_free(self->user_name);
  G_OBJECT_CLASS(gst_pylon_device_parent_class)->finalize(object);
}

static void gst_pylon_device_class_init(GstPylonDeviceClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  GstDeviceClass* dev_class = GST_DEVICE_CLASS(klass);

  gobject_class->finalize = gst_pylon_device_finalize;
  dev_class->create_element = gst_pylon_device_create_element;
  dev_class->reconfigure_element = gst_pylon_device_reconfigure_element;
}

static void gst_pylon_device_init(GstPylonDevice* self) {
  self->serial_number = NULL;
  self->user_name = NULL;
  self->fallback_index = -1;
}

// clang-format off
#if GST_CHECK_VERSION(1, 24, 0)
#  define GST_PYLON_BAYER_FORMATS \
    "rggb, bggr, gbrg, grbg, " \
    "rggb10le, bggr10le, gbrg10le, grbg10le, " \
    "rggb12le, bggr12le, gbrg12le, grbg12le, " \
    "rggb16le, bggr16le, gbrg16le, grbg16le"
#else
#  define GST_PYLON_BAYER_FORMATS "rggb, bggr, gbrg, grbg"
#endif

static GstStaticCaps pylon_device_template_caps = GST_STATIC_CAPS(
    GST_VIDEO_CAPS_MAKE("{ GRAY8, GRAY16_LE, RGB, BGR, YUY2, UYVY }") ";"
    "video/x-bayer, format = (string) { " GST_PYLON_BAYER_FORMATS " }, "
    "width = " GST_VIDEO_SIZE_RANGE ", "
    "height = " GST_VIDEO_SIZE_RANGE ", "
    "framerate = " GST_VIDEO_FPS_RANGE);
// clang-format on

static GstDevice* gst_pylon_device_new_from_info(const Pylon::CDeviceInfo& info,
                                                 gint index) {
  const std::string serial = info.GetSerialNumber().c_str();
  const std::string model = info.GetModelName().c_str();
  const std::string friendly = info.GetFriendlyName().c_str();
  const std::string user_name = info.GetUserDefinedName().c_str();
  const std::string tl_type =
      gst_pylon_get_tl_friendly_name(info.GetDeviceClass());
  const std::string full_name = info.GetFullName().c_str();

  std::string display_name = !friendly.empty() ? friendly : model;
  if (display_name.empty()) {
    display_name = "Basler Camera";
  }

  GstStructure* props = gst_structure_new(
      "pylon-proplist", "device.api", G_TYPE_STRING, "pylon",
      "pylon.transport-layer", G_TYPE_STRING, tl_type.c_str(),
      "pylon.full-name", G_TYPE_STRING, full_name.c_str(), NULL);

  if (!serial.empty()) {
    gst_structure_set(props, "device.serial", G_TYPE_STRING, serial.c_str(),
                      "pylon.serial-number", G_TYPE_STRING, serial.c_str(),
                      NULL);
  }
  if (!model.empty()) {
    gst_structure_set(props, "device.model", G_TYPE_STRING, model.c_str(),
                      NULL);
  }
  if (!user_name.empty()) {
    gst_structure_set(props, "pylon.user-defined-name", G_TYPE_STRING,
                      user_name.c_str(), NULL);
  }

  GstCaps* caps = gst_static_caps_get(&pylon_device_template_caps);

  auto* dev = static_cast<GstPylonDevice*>(g_object_new(
      GST_TYPE_PYLON_DEVICE, "display-name", display_name.c_str(), "caps", caps,
      "device-class", "Video/Source", "properties", props, NULL));

  gst_caps_unref(caps);
  gst_structure_free(props);

  dev->serial_number = serial.empty() ? NULL : g_strdup(serial.c_str());
  dev->user_name = user_name.empty() ? NULL : g_strdup(user_name.c_str());
  dev->fallback_index = index;

  return GST_DEVICE(dev);
}

struct _GstPylonDeviceProvider {
  GstDeviceProvider parent;
};

G_DEFINE_TYPE_WITH_CODE(GstPylonDeviceProvider, gst_pylon_device_provider,
                        GST_TYPE_DEVICE_PROVIDER, gst_pylon_debug_init();)

/* Metadata-only probe: enumerates transport-layer DeviceInfo entries without
 * opening any camera handle, so active OpenCV/GStreamer pipelines never lose
 * exclusive device access. */
static GList* gst_pylon_device_provider_probe(GstDeviceProvider* provider) {
  GList* devices = NULL;

  try {
    Pylon::PylonAutoInitTerm init_pylon;
    Pylon::DeviceInfoList_t device_list;
    gst_pylon_enumerate_devices(device_list);

    for (size_t i = 0; i < device_list.size(); ++i) {
      GstDevice* dev = gst_pylon_device_new_from_info(device_list.at(i),
                                                      static_cast<gint>(i));
      if (dev) {
        devices = g_list_prepend(devices, dev);
      }
    }
  } catch (const GenICam::GenericException& e) {
    GST_WARNING_OBJECT(provider, "Pylon device probe failed: %s",
                       e.GetDescription());
  } catch (const std::exception& e) {
    GST_WARNING_OBJECT(provider, "Pylon device probe failed: %s", e.what());
  }

  return g_list_reverse(devices);
}

static void gst_pylon_device_provider_class_init(
    GstPylonDeviceProviderClass* klass) {
  GstDeviceProviderClass* dm_class = GST_DEVICE_PROVIDER_CLASS(klass);

  dm_class->probe = gst_pylon_device_provider_probe;

  gst_device_provider_class_set_static_metadata(
      dm_class, "Basler/Pylon Device Provider", "Source/Video/Hardware",
      "List and provide Basler Pylon camera source devices",
      "Basler AG <support.europe@baslerweb.com>");
}

static void gst_pylon_device_provider_init(GstPylonDeviceProvider* self) {
  (void)self;
}
