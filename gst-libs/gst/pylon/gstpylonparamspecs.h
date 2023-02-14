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

#ifndef __GST_PYLON_PARAM_SPECS_H__
#define __GST_PYLON_PARAM_SPECS_H__

#include <gst/gst.h>

#include "gst/pylon/gstpylonincludes.h"


G_BEGIN_DECLS

/* Set flag for features with selectors */
#define GST_PYLON_PARAM_IS_SELECTOR (1 << (G_PARAM_USER_SHIFT + 1))
#define GST_PYLON_PARAM_FLAG_IS_SET(pspec, flag) ((pspec)->flags & (flag))

/* --- typedefs & structures --- */

typedef struct _GstPylonParamSpecSelectorData GstPylonParamSpecSelectorData;

/**
 * GstPylonParamSpecSelectorData:
 * @feature: the name of the feature controlled by the selector
 * @selector: the name of the selector that controls the feature
 * @selector_value: the numerical value to set on the selector
 *
 * A structure to append to the different GParamSpecs.
 */
struct _GstPylonParamSpecSelectorData {
  gchar* feature;
  gchar* selector;
  gint64 selector_value;
};

/* --- GParamSpec prototypes --- */

GParamSpec* gst_pylon_param_spec_selector_int64(
    GenApi::INodeMap& nodemap, const gchar* selector_name,
    const gchar* feature_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, gint64 min, gint64 max, gint64 def,
    GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_boolean(
    GenApi::INodeMap& nodemap, const gchar* selector_name,
    const gchar* feature_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, gboolean def, GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_double(
    GenApi::INodeMap& nodemap, const gchar* feature_name,
    const gchar* selector_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, gdouble min, gdouble max, gdouble def,
    GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_string(
    GenApi::INodeMap& nodemap, const gchar* feature_name,
    const gchar* selector_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, const gchar* def, GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_enum(
    GenApi::INodeMap& nodemap, const gchar* feature_name,
    const gchar* selector_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, GType type, gint64 def,
    GParamFlags flags) G_GNUC_MALLOC;

/* --- Utility prototypes --- */

std::string gst_pylon_param_spec_sanitize_name(const gchar* name);
GstPylonParamSpecSelectorData* gst_pylon_param_spec_selector_get_data(
    GParamSpec* spec);
gchar *gst_pylon_create_selected_name(GenApi::INodeMap &nodemap,
                                                   const gchar *feature_name,
                                                   const gchar *selector_name,
                                                   guint64 selector_value);


G_END_DECLS

#endif /* __GST_PYLON_PARAM_SPECS_H__ */
