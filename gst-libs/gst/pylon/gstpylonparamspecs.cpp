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

#include "gstpylondebug.h"
#include "gstpylonparamspecs.h"

#define QSTRING "GstPylonParamSpecSelector"
#define VALID_CHARS G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS

std::string gst_pylon_param_spec_sanitize_name(const gchar *name) {
  g_return_val_if_fail(name, NULL);
  gchar *sanitzed_name =
      g_strcanon(g_strdup_printf("_%s", name), VALID_CHARS, '_');
  std::string sanitzed_name_str = sanitzed_name;
  g_free(sanitzed_name);

  return sanitzed_name_str;
}

gchar *gst_pylon_create_selected_name(GenApi::INodeMap &nodemap,
                                      const gchar *feature_name,
                                      const gchar *selector_name,
                                      guint64 selector_value) {
  g_return_val_if_fail(feature_name, NULL);
  g_return_val_if_fail(selector_name, NULL);

  Pylon::CEnumParameter param(nodemap, selector_name);

  std::string selector_value_str;
  if (param.IsValid()) {
    selector_value_str = param.GetEntry(selector_value)->GetSymbolic();
  } else {
    /* Strip 'Selector' suffix for integer selectables */
    auto selector_prefix = std::string(selector_name);
    const auto suffix_pos = selector_prefix.find("Selector");
    selector_value_str =
        selector_prefix.substr(0, suffix_pos) + std::to_string(selector_value);
  }

  /* Build the property name based on the selector and the feature.
    Since this is no longer a static string, we need to ensure that
    the STATIC_NAME flag is not set */
  gchar *name =
      g_strdup_printf("%s-%s", feature_name, selector_value_str.c_str());

  return name;
}

static gchar *gst_pylon_param_spec_selector_prolog(GenApi::INodeMap &nodemap,
                                                   const gchar *feature_name,
                                                   const gchar *selector_name,
                                                   guint64 selector_value,
                                                   GParamFlags &flags) {
  g_return_val_if_fail(feature_name, NULL);
  g_return_val_if_fail(selector_name, NULL);

  gchar *name = gst_pylon_create_selected_name(nodemap, feature_name,
                                               selector_name, selector_value);

  gint int_flags = flags & ~G_PARAM_STATIC_NAME;
  int_flags |= GST_PYLON_PARAM_IS_SELECTOR;

  flags = static_cast<GParamFlags>(int_flags);

  return name;
}

static void gst_pylon_param_spec_data_free(
    GstPylonParamSpecSelectorData *self) {
  g_return_if_fail(self);

  g_free(self->selector);
  g_free(self->feature);

  g_slice_free(GstPylonParamSpecSelectorData, self);
}

static void gst_pylon_param_spec_selector_epilog(GParamSpec *spec,
                                                 const gchar *feature_name,
                                                 const gchar *selector_name,
                                                 guint64 selector_value) {
  static GQuark quark = g_quark_from_static_string(QSTRING);

  g_return_if_fail(feature_name);
  g_return_if_fail(selector_name);

  GstPylonParamSpecSelectorData *data =
      g_slice_new0(GstPylonParamSpecSelectorData);
  data->selector = g_strdup(selector_name);
  data->feature = g_strdup(feature_name);
  data->selector_value = selector_value;

  g_param_spec_set_qdata_full(spec, quark, data,
                              (GDestroyNotify)gst_pylon_param_spec_data_free);
}

GParamSpec *gst_pylon_param_spec_selector_int64(
    GenApi::INodeMap &nodemap, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, gint64 min, gint64 max, gint64 def, GParamFlags flags) {
  g_return_val_if_fail(def >= min && def <= max, NULL);

  gchar *name = gst_pylon_param_spec_selector_prolog(
      nodemap, feature_name, selector_name, selector_value, flags);

  GParamSpec *spec =
      g_param_spec_int64(name, nick, blurb, min, max, def, flags);
  g_free(name);

  gst_pylon_param_spec_selector_epilog(spec, feature_name, selector_name,
                                       selector_value);

  return spec;
}

GParamSpec *gst_pylon_param_spec_selector_boolean(
    GenApi::INodeMap &nodemap, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, gboolean def, GParamFlags flags) {
  gchar *name = gst_pylon_param_spec_selector_prolog(
      nodemap, feature_name, selector_name, selector_value, flags);

  GParamSpec *spec = g_param_spec_boolean(name, nick, blurb, def, flags);
  g_free(name);

  gst_pylon_param_spec_selector_epilog(spec, feature_name, selector_name,
                                       selector_value);

  return spec;
}

GParamSpec *gst_pylon_param_spec_selector_double(
    GenApi::INodeMap &nodemap, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, gdouble min, gdouble max, gdouble def,
    GParamFlags flags) {
  g_return_val_if_fail(def >= min && def <= max, NULL);

  gchar *name = gst_pylon_param_spec_selector_prolog(
      nodemap, feature_name, selector_name, selector_value, flags);

  GParamSpec *spec =
      g_param_spec_double(name, nick, blurb, min, max, def, flags);
  g_free(name);

  gst_pylon_param_spec_selector_epilog(spec, feature_name, selector_name,
                                       selector_value);

  return spec;
}

GParamSpec *gst_pylon_param_spec_selector_string(
    GenApi::INodeMap &nodemap, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, const gchar *def, GParamFlags flags) {
  gchar *name = gst_pylon_param_spec_selector_prolog(
      nodemap, feature_name, selector_name, selector_value, flags);

  GParamSpec *spec = g_param_spec_string(name, nick, blurb, def, flags);
  g_free(name);

  gst_pylon_param_spec_selector_epilog(spec, feature_name, selector_name,
                                       selector_value);

  return spec;
}

GParamSpec *gst_pylon_param_spec_selector_enum(
    GenApi::INodeMap &nodemap, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, GType type, gint64 def, GParamFlags flags) {
  gchar *name = gst_pylon_param_spec_selector_prolog(
      nodemap, feature_name, selector_name, selector_value, flags);

  GParamSpec *spec = g_param_spec_enum(name, nick, blurb, type, def, flags);
  g_free(name);

  gst_pylon_param_spec_selector_epilog(spec, feature_name, selector_name,
                                       selector_value);

  return spec;
}

GstPylonParamSpecSelectorData *gst_pylon_param_spec_selector_get_data(
    GParamSpec *spec) {
  static GQuark quark = g_quark_from_static_string(QSTRING);

  g_return_val_if_fail(spec, NULL);

  return static_cast<GstPylonParamSpecSelectorData *>(
      g_param_spec_get_qdata(spec, quark));
}
