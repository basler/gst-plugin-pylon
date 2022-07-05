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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstpylonparamspecs.h"

static void _gst_pylon_param_selector_int64_init(GParamSpec *pspec) {
  GstPylonParamSpecSelectorInt64 *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);

  spec->feature = NULL;
  spec->selector = NULL;
  spec->selector_value = G_GUINT64_CONSTANT(0);
  spec->base = NULL;
}

static void _gst_pylon_param_selector_int64_finalize(GParamSpec *pspec) {
  GstPylonParamSpecSelectorInt64 *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);

  g_param_spec_unref(spec->base);
}

static void _gst_pylon_param_selector_int64_set_default(GParamSpec *pspec,
                                                        GValue *value) {
  GstPylonParamSpecSelectorInt64 *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);

  g_param_value_set_default(spec->base, value);
}

static gboolean _gst_pylon_param_selector_int64_validate(GParamSpec *pspec,
                                                         GValue *value) {
  GstPylonParamSpecSelectorInt64 *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);

  return g_param_value_validate(spec->base, value);
}

static gint _gst_pylon_param_selector_int64_values_cmp(GParamSpec *pspec,
                                                       const GValue *value1,
                                                       const GValue *value2) {
  GstPylonParamSpecSelectorInt64 *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);

  return g_param_values_cmp(spec->base, value1, value2);
}

GType gst_pylon_param_spec_selector_int64_get_type(void) {
  static GType gst_pylon_selector_int64_type = 0;

  /* register GST_PYLON_TYPE_PARAM_SELECTOR_INT64 */
  if (g_once_init_enter(&gst_pylon_selector_int64_type)) {
    GType type;
    static GParamSpecTypeInfo pspec_info = {
        sizeof(GstPylonParamSpecSelectorInt64),      /* instance_size     */
        0,                                           /* n_preallocs       */
        _gst_pylon_param_selector_int64_init,        /* instance_init     */
        G_TYPE_INT64,                                /* value_type        */
        _gst_pylon_param_selector_int64_finalize,    /* finalize          */
        _gst_pylon_param_selector_int64_set_default, /* value_set_default */
        _gst_pylon_param_selector_int64_validate,    /* value_validate    */
        _gst_pylon_param_selector_int64_values_cmp,  /* values_cmp        */
    };

    type =
        g_param_type_register_static("GstPylonParamSelectorInt64", &pspec_info);
    g_once_init_leave(&gst_pylon_selector_int64_type, type);
  }

  return gst_pylon_selector_int64_type;
}

GParamSpec *gst_pylon_param_spec_selector_int64(
    GenApi::INode *feature, GenApi::INode *selector, guint64 selector_value,
    const gchar *nick, const gchar *blurb, gint64 min, gint64 max, gint64 def,
    GParamFlags flags) {
  GstPylonParamSpecSelectorInt64 *spec;
  gchar *name = NULL;
  gint int_flags = static_cast<gint>(flags);

  g_return_val_if_fail(selector, NULL);
  g_return_val_if_fail(feature, NULL);

  g_return_val_if_fail(def >= min && def <= max, NULL);

  /* Build the property name based on the selector and the feature.
     Since this is no longer a static string, we need to ensure that
     the STATIC_NAME flag is not set */

  Pylon::CEnumParameter param(selector);

  name = g_strdup_printf("%s-%s", feature->GetName().c_str(),
                         param.GetEntry(selector_value)->GetSymbolic().c_str());
  int_flags &= ~G_PARAM_STATIC_NAME;

  spec = static_cast<GstPylonParamSpecSelectorInt64 *>(
      g_param_spec_internal(GST_PYLON_TYPE_PARAM_SELECTOR_INT64, name, nick,
                            blurb, static_cast<GParamFlags>(int_flags)));

  spec->selector = selector;
  spec->feature = feature;
  spec->selector_value = selector_value;

  spec->base = g_param_spec_int64(name, nick, blurb, min, max, def,
                                  static_cast<GParamFlags>(int_flags));
  g_free(name);

  return G_PARAM_SPEC(spec);
}
