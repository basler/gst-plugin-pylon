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

#include "gst/pylon/gstpyloninspectcache.h"
#include "gst/pylon/gstpylonobject.h"
#include "gstchildinspector.h"
#include "gstpyloninspect.h"
#include "gstpylonschema.h"

#include <gst/pylon/gstpylonincludes.h>

#include <string>

static constexpr gint DEFAULT_ALIGNMENT = 35;

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

  GstPylonCache feature_cache(gst_pylon_camera_schema_cache_key(*camera),
                              FALSE);
  GstPylonObjectSchema schema =
      gst_pylon_make_camera_schema(*camera, feature_cache);
  gchar* block = NULL;
  gst_pylon_append_properties(camera, schema, "Camera", &block, alignment);
  return block;
}

static gchar* gst_pylon_get_stream_properties_block(
    Pylon::CBaslerUniversalInstantCamera* camera, guint alignment) {
  g_return_val_if_fail(camera, NULL);

  GstPylonCache feature_cache(gst_pylon_stream_schema_cache_key(*camera),
                              FALSE);
  GstPylonObjectSchema schema =
      gst_pylon_make_stream_schema(*camera, feature_cache);
  gchar* block = NULL;
  gst_pylon_append_properties(camera, schema, "Stream Grabber", &block,
                              alignment);
  return block;
}

using GstPylonPropertiesBlockBuilder =
    gchar* (*)(Pylon::CBaslerUniversalInstantCamera * camera, guint alignment);

static void gst_pylon_append_cached_properties_block(
    gchar** properties, Pylon::CBaslerUniversalInstantCamera* camera,
    const std::string& cache_key, gboolean use_cache,
    GstPylonPropertiesBlockBuilder build_block) {
  g_return_if_fail(properties);
  g_return_if_fail(camera);
  g_return_if_fail(build_block);

  gchar* block = NULL;
  if (use_cache) {
    block = GstPylonInspectCache::Get(cache_key);
  }

  if (!block) {
    block = build_block(camera, DEFAULT_ALIGNMENT);
    if (use_cache && block && block[0] != '\0') {
      GstPylonInspectCache::Set(cache_key, block);
    }
  }

  if (!block) {
    return;
  }

  if (*properties) {
    gchar* tmp = g_strconcat(*properties, "\n", block, NULL);
    g_free(*properties);
    g_free(block);
    *properties = tmp;
  } else {
    *properties = block;
  }
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

      camera.DeviceFeaturePersistenceEnd.TryExecute();
      camera.DeviceRegistersStreamingEnd.TryExecute();

      const std::string camera_key = gst_pylon_camera_schema_cache_key(camera);
      const std::string stream_key = gst_pylon_stream_schema_cache_key(camera);
      const gboolean use_cache = device_list.size() == 1;

      gst_pylon_append_cached_properties_block(
          &camera_properties, &camera, camera_key, use_cache,
          gst_pylon_get_camera_properties_block);
      gst_pylon_append_cached_properties_block(
          &stream_properties, &camera, stream_key, use_cache,
          gst_pylon_get_stream_properties_block);

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

gchar* gst_pylon_inspect_camera_properties() {
  gchar* cam = NULL;
  gchar* stream = NULL;
  gst_pylon_get_introspection_strings(&cam, &stream);
  g_free(stream);
  return cam;
}

gchar* gst_pylon_inspect_stream_properties() {
  gchar* cam = NULL;
  gchar* stream = NULL;
  gst_pylon_get_introspection_strings(&cam, &stream);
  g_free(cam);
  return stream;
}
