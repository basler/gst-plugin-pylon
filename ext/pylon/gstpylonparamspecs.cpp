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

#define VALID_CHARS G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS

gchar *gst_pylon_param_spec_sanitize_name(const gchar *name) {
  g_return_val_if_fail(name, NULL);

  return g_strcanon(g_strdup_printf("_%s", name), VALID_CHARS, '_');
}

/* --- Selector int64 type functions --- */

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

  g_free(spec->feature);
  g_free(spec->selector);
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
    Pylon::CBaslerUniversalInstantCamera *camera, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, gint64 min, gint64 max, gint64 def, GParamFlags flags) {
  GstPylonParamSpecSelectorInt64 *spec;
  gchar *name = NULL;
  gint int_flags = static_cast<gint>(flags);

  g_return_val_if_fail(camera, NULL);

  g_return_val_if_fail(def >= min && def <= max, NULL);

  GenApi::INodeMap &nodemap = camera->GetNodeMap();
  Pylon::CEnumParameter param(nodemap, selector_name);

  /* Build the property name based on the selector and the feature.
    Since this is no longer a static string, we need to ensure that
    the STATIC_NAME flag is not set */
  name = g_strdup_printf("%s-%s", feature_name,
                         param.GetEntry(selector_value)->GetSymbolic().c_str());
  int_flags &= ~G_PARAM_STATIC_NAME;

  spec = static_cast<GstPylonParamSpecSelectorInt64 *>(
      g_param_spec_internal(GST_PYLON_TYPE_PARAM_SELECTOR_INT64, name, nick,
                            blurb, static_cast<GParamFlags>(int_flags)));

  spec->selector = g_strdup(selector_name);
  spec->feature = g_strdup(feature_name);
  spec->selector_value = selector_value;

  spec->base = g_param_spec_int64(name, nick, blurb, min, max, def,
                                  static_cast<GParamFlags>(int_flags));
  g_free(name);

  return G_PARAM_SPEC(spec);
}

/* --- Selector boolean type functions --- */

static void _gst_pylon_param_selector_bool_init(GParamSpec *pspec) {
  GstPylonParamSpecSelectorBool *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_BOOL(pspec);

  spec->feature = NULL;
  spec->selector = NULL;
  spec->selector_value = G_GUINT64_CONSTANT(0);
  spec->base = NULL;
}

static void _gst_pylon_param_selector_bool_finalize(GParamSpec *pspec) {
  GstPylonParamSpecSelectorBool *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_BOOL(pspec);

  g_free(spec->feature);
  g_free(spec->selector);
  g_param_spec_unref(spec->base);
}

static void _gst_pylon_param_selector_bool_set_default(GParamSpec *pspec,
                                                       GValue *value) {
  GstPylonParamSpecSelectorBool *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_BOOL(pspec);

  g_param_value_set_default(spec->base, value);
}

static gboolean _gst_pylon_param_selector_bool_validate(GParamSpec *pspec,
                                                        GValue *value) {
  GstPylonParamSpecSelectorBool *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_BOOL(pspec);

  return g_param_value_validate(spec->base, value);
}

static gint _gst_pylon_param_selector_bool_values_cmp(GParamSpec *pspec,
                                                      const GValue *value1,
                                                      const GValue *value2) {
  GstPylonParamSpecSelectorBool *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_BOOL(pspec);

  return g_param_values_cmp(spec->base, value1, value2);
}

GType gst_pylon_param_spec_selector_bool_get_type(void) {
  static GType gst_pylon_selector_bool_type = 0;

  /* register GST_PYLON_TYPE_PARAM_SELECTOR_BOOL */
  if (g_once_init_enter(&gst_pylon_selector_bool_type)) {
    GType type;
    static GParamSpecTypeInfo pspec_info = {
        sizeof(GstPylonParamSpecSelectorBool),      /* instance_size     */
        0,                                          /* n_preallocs       */
        _gst_pylon_param_selector_bool_init,        /* instance_init     */
        G_TYPE_BOOLEAN,                             /* value_type        */
        _gst_pylon_param_selector_bool_finalize,    /* finalize          */
        _gst_pylon_param_selector_bool_set_default, /* value_set_default */
        _gst_pylon_param_selector_bool_validate,    /* value_validate    */
        _gst_pylon_param_selector_bool_values_cmp,  /* values_cmp        */
    };

    type =
        g_param_type_register_static("GstPylonParamSelectorBool", &pspec_info);
    g_once_init_leave(&gst_pylon_selector_bool_type, type);
  }

  return gst_pylon_selector_bool_type;
}

GParamSpec *gst_pylon_param_spec_selector_bool(
    Pylon::CBaslerUniversalInstantCamera *camera, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, gboolean def, GParamFlags flags) {
  GstPylonParamSpecSelectorBool *spec;
  gchar *name = NULL;
  gint int_flags = static_cast<gint>(flags);

  g_return_val_if_fail(camera, NULL);

  GenApi::INodeMap &nodemap = camera->GetNodeMap();
  Pylon::CEnumParameter param(nodemap, selector_name);

  /* Build the property name based on the selector and the feature.
     Since this is no longer a static string, we need to ensure that
     the STATIC_NAME flag is not set */
  name = g_strdup_printf("%s-%s", feature_name,
                         param.GetEntry(selector_value)->GetSymbolic().c_str());
  int_flags &= ~G_PARAM_STATIC_NAME;

  spec = static_cast<GstPylonParamSpecSelectorBool *>(
      g_param_spec_internal(GST_PYLON_TYPE_PARAM_SELECTOR_BOOL, name, nick,
                            blurb, static_cast<GParamFlags>(int_flags)));

  spec->selector = g_strdup(selector_name);
  spec->feature = g_strdup(feature_name);
  spec->selector_value = selector_value;

  spec->base = g_param_spec_boolean(name, nick, blurb, def,
                                    static_cast<GParamFlags>(int_flags));
  g_free(name);

  return G_PARAM_SPEC(spec);
}

/* --- Selector float type functions --- */

static void _gst_pylon_param_selector_float_init(GParamSpec *pspec) {
  GstPylonParamSpecSelectorFloat *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec);

  spec->feature = NULL;
  spec->selector = NULL;
  spec->selector_value = G_GUINT64_CONSTANT(0);
  spec->base = NULL;
}

static void _gst_pylon_param_selector_float_finalize(GParamSpec *pspec) {
  GstPylonParamSpecSelectorFloat *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec);

  g_free(spec->feature);
  g_free(spec->selector);
  g_param_spec_unref(spec->base);
}

static void _gst_pylon_param_selector_float_set_default(GParamSpec *pspec,
                                                        GValue *value) {
  GstPylonParamSpecSelectorFloat *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec);

  g_param_value_set_default(spec->base, value);
}

static gboolean _gst_pylon_param_selector_float_validate(GParamSpec *pspec,
                                                         GValue *value) {
  GstPylonParamSpecSelectorFloat *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec);

  return g_param_value_validate(spec->base, value);
}

static gint _gst_pylon_param_selector_float_values_cmp(GParamSpec *pspec,
                                                       const GValue *value1,
                                                       const GValue *value2) {
  GstPylonParamSpecSelectorFloat *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec);

  return g_param_values_cmp(spec->base, value1, value2);
}

GType gst_pylon_param_spec_selector_float_get_type(void) {
  static GType gst_pylon_selector_float_type = 0;

  /* register GST_PYLON_TYPE_PARAM_SELECTOR_FLOAT */
  if (g_once_init_enter(&gst_pylon_selector_float_type)) {
    GType type;
    static GParamSpecTypeInfo pspec_info = {
        sizeof(GstPylonParamSpecSelectorFloat),      /* instance_size     */
        0,                                           /* n_preallocs       */
        _gst_pylon_param_selector_float_init,        /* instance_init     */
        G_TYPE_FLOAT,                                /* value_type        */
        _gst_pylon_param_selector_float_finalize,    /* finalize          */
        _gst_pylon_param_selector_float_set_default, /* value_set_default */
        _gst_pylon_param_selector_float_validate,    /* value_validate    */
        _gst_pylon_param_selector_float_values_cmp,  /* values_cmp        */
    };

    type =
        g_param_type_register_static("GstPylonParamSelectorFloat", &pspec_info);
    g_once_init_leave(&gst_pylon_selector_float_type, type);
  }

  return gst_pylon_selector_float_type;
}

GParamSpec *gst_pylon_param_spec_selector_float(
    Pylon::CBaslerUniversalInstantCamera *camera, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, gdouble min, gdouble max, gdouble def,
    GParamFlags flags) {
  GstPylonParamSpecSelectorFloat *spec;
  gchar *name = NULL;
  gint int_flags = static_cast<gint>(flags);

  g_return_val_if_fail(camera, NULL);

  g_return_val_if_fail(def >= min && def <= max, NULL);

  GenApi::INodeMap &nodemap = camera->GetNodeMap();
  Pylon::CEnumParameter param(nodemap, selector_name);

  /* Build the property name based on the selector and the feature.
    Since this is no longer a static string, we need to ensure that
    the STATIC_NAME flag is not set */
  name = g_strdup_printf("%s-%s", feature_name,
                         param.GetEntry(selector_value)->GetSymbolic().c_str());
  int_flags &= ~G_PARAM_STATIC_NAME;

  spec = static_cast<GstPylonParamSpecSelectorFloat *>(
      g_param_spec_internal(GST_PYLON_TYPE_PARAM_SELECTOR_FLOAT, name, nick,
                            blurb, static_cast<GParamFlags>(int_flags)));

  spec->selector = g_strdup(selector_name);
  spec->feature = g_strdup(feature_name);
  spec->selector_value = selector_value;

  spec->base = g_param_spec_float(name, nick, blurb, min, max, def,
                                  static_cast<GParamFlags>(int_flags));
  g_free(name);

  return G_PARAM_SPEC(spec);
}

/* --- Selector string type functions --- */

static void _gst_pylon_param_selector_str_init(GParamSpec *pspec) {
  GstPylonParamSpecSelectorStr *spec = GST_PYLON_PARAM_SPEC_SELECTOR_STR(pspec);

  spec->feature = NULL;
  spec->selector = NULL;
  spec->selector_value = G_GUINT64_CONSTANT(0);
  spec->base = NULL;
}

static void _gst_pylon_param_selector_str_finalize(GParamSpec *pspec) {
  GstPylonParamSpecSelectorStr *spec = GST_PYLON_PARAM_SPEC_SELECTOR_STR(pspec);

  g_free(spec->feature);
  g_free(spec->selector);
  g_param_spec_unref(spec->base);
}

static void _gst_pylon_param_selector_str_set_default(GParamSpec *pspec,
                                                      GValue *value) {
  GstPylonParamSpecSelectorStr *spec = GST_PYLON_PARAM_SPEC_SELECTOR_STR(pspec);

  g_param_value_set_default(spec->base, value);
}

static gboolean _gst_pylon_param_selector_str_validate(GParamSpec *pspec,
                                                       GValue *value) {
  GstPylonParamSpecSelectorStr *spec = GST_PYLON_PARAM_SPEC_SELECTOR_STR(pspec);

  return g_param_value_validate(spec->base, value);
}

static gint _gst_pylon_param_selector_str_values_cmp(GParamSpec *pspec,
                                                     const GValue *value1,
                                                     const GValue *value2) {
  GstPylonParamSpecSelectorStr *spec = GST_PYLON_PARAM_SPEC_SELECTOR_STR(pspec);

  return g_param_values_cmp(spec->base, value1, value2);
}

GType gst_pylon_param_spec_selector_str_get_type(void) {
  static GType gst_pylon_selector_str_type = 0;

  /* register GST_PYLON_TYPE_PARAM_SELECTOR_STR */
  if (g_once_init_enter(&gst_pylon_selector_str_type)) {
    GType type;
    static GParamSpecTypeInfo pspec_info = {
        sizeof(GstPylonParamSpecSelectorStr),      /* instance_size     */
        0,                                         /* n_preallocs       */
        _gst_pylon_param_selector_str_init,        /* instance_init     */
        G_TYPE_STRING,                             /* value_type        */
        _gst_pylon_param_selector_str_finalize,    /* finalize          */
        _gst_pylon_param_selector_str_set_default, /* value_set_default */
        _gst_pylon_param_selector_str_validate,    /* value_validate    */
        _gst_pylon_param_selector_str_values_cmp,  /* values_cmp        */
    };

    type =
        g_param_type_register_static("GstPylonParamSelectorStr", &pspec_info);
    g_once_init_leave(&gst_pylon_selector_str_type, type);
  }

  return gst_pylon_selector_str_type;
}

GParamSpec *gst_pylon_param_spec_selector_str(
    Pylon::CBaslerUniversalInstantCamera *camera, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, const gchar *def, GParamFlags flags) {
  GstPylonParamSpecSelectorStr *spec;
  gchar *name = NULL;
  gint int_flags = static_cast<gint>(flags);

  g_return_val_if_fail(camera, NULL);

  GenApi::INodeMap &nodemap = camera->GetNodeMap();
  Pylon::CEnumParameter param(nodemap, selector_name);

  /* Build the property name based on the selector and the feature.
     Since this is no longer a static string, we need to ensure that
     the STATIC_NAME flag is not set */
  name = g_strdup_printf("%s-%s", feature_name,
                         param.GetEntry(selector_value)->GetSymbolic().c_str());
  int_flags &= ~G_PARAM_STATIC_NAME;

  spec = static_cast<GstPylonParamSpecSelectorStr *>(
      g_param_spec_internal(GST_PYLON_TYPE_PARAM_SELECTOR_STR, name, nick,
                            blurb, static_cast<GParamFlags>(int_flags)));

  spec->selector = g_strdup(selector_name);
  spec->feature = g_strdup(feature_name);
  spec->selector_value = selector_value;

  spec->base = g_param_spec_string(name, nick, blurb, def,
                                   static_cast<GParamFlags>(int_flags));
  g_free(name);

  return G_PARAM_SPEC(spec);
}

/* --- Selector enumeration type functions --- */

static void _gst_pylon_param_selector_enum_init(GParamSpec *pspec) {
  GstPylonParamSpecSelectorEnum *spec = (GstPylonParamSpecSelectorEnum *)pspec;

  spec->feature = NULL;
  spec->selector = NULL;
  spec->selector_value = G_GUINT64_CONSTANT(0);
  spec->base = NULL;
}

static void _gst_pylon_param_selector_enum_finalize(GParamSpec *pspec) {
  GstPylonParamSpecSelectorEnum *spec = (GstPylonParamSpecSelectorEnum *)pspec;

  g_free(spec->feature);
  g_free(spec->selector);
  g_param_spec_unref(spec->base);
}

static void _gst_pylon_param_selector_enum_set_default(GParamSpec *pspec,
                                                       GValue *value) {
  GstPylonParamSpecSelectorEnum *spec = (GstPylonParamSpecSelectorEnum *)pspec;

  g_param_value_set_default(spec->base, value);
}

static gboolean _gst_pylon_param_selector_enum_validate(GParamSpec *pspec,
                                                        GValue *value) {
  GstPylonParamSpecSelectorEnum *spec = (GstPylonParamSpecSelectorEnum *)pspec;

  return g_param_value_validate(spec->base, value);
}

static gint _gst_pylon_param_selector_enum_values_cmp(GParamSpec *pspec,
                                                      const GValue *value1,
                                                      const GValue *value2) {
  GstPylonParamSpecSelectorEnum *spec = (GstPylonParamSpecSelectorEnum *)pspec;

  return g_param_values_cmp(spec->base, value1, value2);
}

GType gst_pylon_param_spec_selector_enum_register(
    Pylon::CBaslerUniversalInstantCamera *camera, const gchar *feature_name,
    GType enum_feature_type) {
  GType selector_type = G_TYPE_INVALID;

  gchar *full_name = g_strdup_printf(
      "%s_%s", camera->GetDeviceInfo().GetFullName().c_str(), feature_name);
  gchar *name = gst_pylon_param_spec_sanitize_name(full_name);
  g_free(full_name);

  selector_type = g_type_from_name(name);
  /* register GST_PYLON_TYPE_PARAM_SELECTOR_ENUM */
  if (0 == selector_type) {
    GParamSpecTypeInfo pspec_info = {
        sizeof(GstPylonParamSpecSelectorEnum),      /* instance_size     */
        0,                                          /* n_preallocs       */
        _gst_pylon_param_selector_enum_init,        /* instance_init     */
        enum_feature_type,                          /* value_type        */
        _gst_pylon_param_selector_enum_finalize,    /* finalize          */
        _gst_pylon_param_selector_enum_set_default, /* value_set_default */
        _gst_pylon_param_selector_enum_validate,    /* value_validate    */
        _gst_pylon_param_selector_enum_values_cmp,  /* values_cmp        */
    };

    selector_type = g_param_type_register_static(name, &pspec_info);
  }

  g_free(name);

  return selector_type;
}

GParamSpec *gst_pylon_param_spec_selector_enum(
    Pylon::CBaslerUniversalInstantCamera *camera, const gchar *feature_name,
    const gchar *selector_name, guint64 selector_value, const gchar *nick,
    const gchar *blurb, GType type, gint64 def, GParamFlags flags) {
  GstPylonParamSpecSelectorEnum *spec;
  gchar *name = NULL;
  gint int_flags = static_cast<gint>(flags);

  g_return_val_if_fail(camera, NULL);

  GenApi::INodeMap &nodemap = camera->GetNodeMap();
  Pylon::CEnumParameter param(nodemap, selector_name);

  /* Build the property name based on the selector and the feature.
    Since this is no longer a static string, we need to ensure that
    the STATIC_NAME flag is not set */
  name = g_strdup_printf("%s-%s", feature_name,
                         param.GetEntry(selector_value)->GetSymbolic().c_str());
  int_flags &= ~G_PARAM_STATIC_NAME;

  spec = static_cast<GstPylonParamSpecSelectorEnum *>(g_param_spec_internal(
      gst_pylon_param_spec_selector_enum_register(camera, name, type), name,
      nick, blurb, static_cast<GParamFlags>(int_flags)));

  spec->selector = g_strdup(selector_name);
  spec->feature = g_strdup(feature_name);
  spec->selector_value = selector_value;

  spec->base = g_param_spec_enum(name, nick, blurb, type, def,
                                 static_cast<GParamFlags>(int_flags));
  g_free(name);

  return G_PARAM_SPEC(spec);
}
