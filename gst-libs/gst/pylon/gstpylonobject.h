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

#ifndef _GST_PYLON_OBJECT_H_
#define _GST_PYLON_OBJECT_H_

#include <gst/gst.h>
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/pylon/gstpylon-prelude.h>
#include <gst/pylon/gstpyloncache.h>
#include <gst/pylon/gstpylonincludes.h>

#include <memory>
#include <mutex>
#include <string>

G_DECLARE_DERIVABLE_TYPE(GstPylonObject, gst_pylon_object, GST, PYLON_OBJECT,
                         GstObject)

struct _GstPylonObjectClass {
  GstObjectClass parent_class;
  gint private_offset;
};

typedef void (*GstPylonFramerateChangedCb)(gpointer user_data, gdouble fps);

struct GstPylonObjectSchema {
  std::string device_full_name;
  std::string schema_cache_key;
  GstPylonCache* feature_cache;
  GenApi::INodeMap* nodemap;

  GstPylonCache& cache() const { return *feature_cache; }
  GenApi::INodeMap& node_map() const { return *nodemap; }
};

struct GstPylonObjectClassData {
  GstPylonObjectSchema schema;
  gint private_offset;
};

EXT_PYLONSRC_API GType
gst_pylon_object_register_schema(const GstPylonObjectSchema& schema);
EXT_PYLONSRC_API GObject* gst_pylon_object_new_for_schema(
    std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera,
    const GstPylonObjectSchema& schema, gboolean enable_correction,
    std::shared_ptr<std::recursive_mutex> camera_mutex = nullptr);
EXT_PYLONSRC_API void gst_pylon_object_rebind_nodemap(
    GObject* obj, GenApi::INodeMap* nodemap);
EXT_PYLONSRC_API void gst_pylon_object_set_enable_correction(
    GObject* obj, gboolean enable_correction);
EXT_PYLONSRC_API gboolean gst_pylon_object_get_enable_correction(GObject* obj);
EXT_PYLONSRC_API void gst_pylon_object_get_dimension_cache(GObject* obj,
                                                           gint* width,
                                                           gint* height);
EXT_PYLONSRC_API void gst_pylon_object_take_offset_cache(GObject* obj,
                                                         gint* offsetx,
                                                         gint* offsety);
EXT_PYLONSRC_API gboolean
gst_pylon_object_is_framerate_configured(GObject* obj);
EXT_PYLONSRC_API void gst_pylon_object_mark_framerate_configured(GObject* obj);
EXT_PYLONSRC_API void gst_pylon_object_set_framerate_callback(
    GObject* obj, GstPylonFramerateChangedCb cb, gpointer user_data);
EXT_PYLONSRC_API gboolean gst_pylon_object_execute_command(
    GObject* obj, const gchar* command_name, GError** err);

EXT_PYLONSRC_API void gst_pylon_object_set_pylon_selector(
    GenApi::INodeMap& nodemap, const gchar* selector_name,
    gint64& selector_value);

#ifdef GST_PYLON_ENABLE_TEST_HOOKS
EXT_PYLONSRC_API void gst_pylon_object_test_set_inject_std_exception(
    gboolean enable);
#endif

#endif
