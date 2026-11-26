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
  gint64 orig_offset_x = 0;
  gint64 orig_offset_y = 0;
  const bool has_offset_x = self->camera->OffsetX.IsReadable();
  const bool has_offset_y = self->camera->OffsetY.IsReadable();

  if (has_offset_x) {
    orig_offset_x = self->camera->OffsetX.GetValue();
  }
  if (has_offset_y) {
    orig_offset_y = self->camera->OffsetY.GetValue();
  }

  const std::vector<std::pair<GstPylonQuery, const std::string>> queries = {
      {gst_pylon_query_width, "width"},
      {gst_pylon_query_height, "height"},
      {gst_pylon_query_framerate, "framerate"}};

  /* Offsets are set to 0 to get the true image geometry */
  if (has_offset_x) {
    self->camera->OffsetX.TrySetToMinimum();
  }
  if (has_offset_y) {
    self->camera->OffsetY.TrySetToMinimum();
  }

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
  if (has_offset_x) {
    self->camera->OffsetX.TrySetValue(orig_offset_x);
  }
  if (has_offset_y) {
    self->camera->OffsetY.TrySetValue(orig_offset_y);
  }
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
      if (offsety.IsWritable()) {
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
    }

    Pylon::CBooleanParameter framerate_enable(nodemap,
                                              "AcquisitionFrameRateEnable");

    /* Honor framerate only when an explicit config source set it. Camera
     * defaults may have AcquisitionFrameRateEnable already true. */
    const bool apply_caps_framerate =
        !gst_pylon_object_is_framerate_configured(gcamera);
    if (apply_caps_framerate) {
      /* Basler dart gen1 models have no framerate_enable feature */
      framerate_enable.TrySetValue(true);
      GST_INFO("Applying acquisition framerate from caps");
    } else {
      GST_INFO("Honoring acquisition framerate from PFS or child property");
    }

    if (apply_caps_framerate) {
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
