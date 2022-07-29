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

#include "gstchildinspector.h"

#include "gstpylonparamspecs.h"

typedef struct _GstChildInspectorFlag GstChildInspectorFlag;
typedef struct _GstChildInspectorType GstChildInspectorType;

typedef gchar *(*GstChildInspectorTypeToString)(GParamSpec *pspec,
                                                GValue *value, gint alignment);

static GString *gst_child_inspector_build_string_for_enum(GParamSpecEnum *penum,
                                                          GValue *value,
                                                          gint alignment);
static gchar *gst_child_inspector_type_int64_to_string(GParamSpec *pspec,
                                                       GValue *value,
                                                       gint alignment);
static gchar *gst_child_inspector_type_bool_to_string(GParamSpec *pspec,
                                                      GValue *value,
                                                      gint alignment);
static gchar *gst_child_inspector_type_float_to_string(GParamSpec *pspec,
                                                       GValue *value,
                                                       gint alignment);
static gchar *gst_child_inspector_type_string_to_string(GParamSpec *pspec,
                                                        GValue *value,
                                                        gint alignment);
static gchar *gst_child_inspector_type_enum_to_string(GParamSpec *pspec,
                                                      GValue *value,
                                                      gint alignment);
static gchar *gst_child_inspector_type_selector_int64_to_string(
    GParamSpec *pspec, GValue *value, gint alignment);
static gchar *gst_child_inspector_type_selector_float_to_string(
    GParamSpec *pspec, GValue *value, gint alignment);
static gchar *gst_child_inspector_type_selector_enum_to_string(
    GParamSpec *pspec, GValue *value, gint alignment);

struct _GstChildInspectorFlag {
  gint value;
  const gchar *to_string;
};

struct _GstChildInspectorType {
  GType value;
  GstChildInspectorTypeToString type_to_string;
  GstChildInspectorTypeToString selector_to_string;
};

static GstChildInspectorFlag flags[] = {
    {G_PARAM_READABLE, "readable"},
    {G_PARAM_WRITABLE, "writable"},
    {GST_PARAM_MUTABLE_READY, "changeable only in NULL or READY state"},
    {GST_PARAM_MUTABLE_PLAYING,
     "changeable in NULL, READY, PAUSED or PLAYING state"},
    {0, NULL}};

static GstChildInspectorType types[] = {
    {G_TYPE_INT64, gst_child_inspector_type_int64_to_string,
     gst_child_inspector_type_selector_int64_to_string},
    {G_TYPE_BOOLEAN, gst_child_inspector_type_bool_to_string,
     gst_child_inspector_type_bool_to_string},
    {G_TYPE_FLOAT, gst_child_inspector_type_float_to_string,
     gst_child_inspector_type_selector_float_to_string},
    {G_TYPE_STRING, gst_child_inspector_type_string_to_string,
     gst_child_inspector_type_string_to_string},
    {G_TYPE_ENUM, gst_child_inspector_type_enum_to_string,
     gst_child_inspector_type_selector_enum_to_string},
    {0, NULL, NULL}};

static GString *gst_child_inspector_build_string_for_enum(GParamSpecEnum *penum,
                                                          GValue *value,
                                                          gint alignment) {
  GType type = G_TYPE_FROM_CLASS(penum->enum_class);
  gint def = g_value_get_enum(value);
  gchar *sdef = g_enum_to_string(type, def);
  GString *desc = g_string_new(NULL);
  GEnumValue *iter = NULL;

  g_string_append_printf(desc, "Enum \"%s\" Default: %d, \"%s\"",
                         g_type_name(type), def, sdef);
  g_free(sdef);

  for (iter = penum->enum_class->values; iter->value_name; iter++) {
    if (iter->value_nick[0] == '\0') {
      g_string_append_printf(desc, "\n%*s(%d): %-18s", alignment + 40, "",
                             iter->value, iter->value_name);
    } else {
      g_string_append_printf(desc, "\n%*s(%d): %-18s - %s", alignment + 40, "",
                             iter->value, iter->value_name, iter->value_nick);
    }
  }
  return desc;
}

static gchar *gst_child_inspector_type_string_to_string(GParamSpec *pspec,
                                                        GValue *value,
                                                        gint alignment) {
  return g_strdup_printf("String. Default: \"%s\"", g_value_get_string(value));
}

static gchar *gst_child_inspector_type_int64_to_string(GParamSpec *pspec,
                                                       GValue *value,
                                                       gint alignment) {
  GParamSpecInt64 *pint = G_PARAM_SPEC_INT64(pspec);

  return g_strdup_printf("Integer64. Range: %" G_GINT64_FORMAT
                         " - %" G_GINT64_FORMAT " Default: %" G_GINT64_FORMAT,
                         pint->minimum, pint->maximum,
                         g_value_get_int64(value));
}

static gchar *gst_child_inspector_type_float_to_string(GParamSpec *pspec,
                                                       GValue *value,
                                                       gint alignment) {
  GParamSpecFloat *pfloat = G_PARAM_SPEC_FLOAT(pspec);

  return g_strdup_printf("Float. Range: %.2f - %.2f Default: %.2f",
                         pfloat->minimum, pfloat->maximum,
                         g_value_get_float(value));
}

static gchar *gst_child_inspector_type_bool_to_string(GParamSpec *pspec,
                                                      GValue *value,
                                                      gint alignment) {
  return g_strdup_printf("Boolean. Default: %s",
                         g_value_get_boolean(value) ? "true" : "false");
}

static gchar *gst_child_inspector_type_enum_to_string(GParamSpec *pspec,
                                                      GValue *value,
                                                      gint alignment) {
  GParamSpecEnum *penum = G_PARAM_SPEC_ENUM(pspec);
  return g_string_free(
      gst_child_inspector_build_string_for_enum(penum, value, alignment),
      FALSE);
}

static gchar *gst_child_inspector_type_selector_int64_to_string(
    GParamSpec *pspec, GValue *value, gint alignment) {
  GstPylonParamSpecSelectorInt64 *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);
  GParamSpecInt64 *pint = G_PARAM_SPEC_INT64(spec->base);

  return gst_child_inspector_type_int64_to_string(G_PARAM_SPEC(pint), value,
                                                  alignment);
}

static gchar *gst_child_inspector_type_selector_float_to_string(
    GParamSpec *pspec, GValue *value, gint alignment) {
  GstPylonParamSpecSelectorFloat *spec =
      GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec);
  GParamSpecFloat *pfloat = G_PARAM_SPEC_FLOAT(spec->base);

  return gst_child_inspector_type_float_to_string(G_PARAM_SPEC(pfloat), value,
                                                  alignment);
}

static gchar *gst_child_inspector_type_selector_enum_to_string(
    GParamSpec *pspec, GValue *value, gint alignment) {
  GstPylonParamSpecSelectorEnum *spec = (GstPylonParamSpecSelectorEnum *)pspec;
  GParamSpecEnum *penum = G_PARAM_SPEC_ENUM(spec->base);

  return gst_child_inspector_type_enum_to_string(G_PARAM_SPEC(penum), value,
                                                 alignment);
}

static const gchar *gst_child_inspector_flag_to_string(GParamFlags flag) {
  GstChildInspectorFlag *current_flag;
  const gchar *to_string = NULL;

  for (current_flag = flags; current_flag->to_string; ++current_flag) {
    if (current_flag->value == flag) {
      to_string = current_flag->to_string;
      break;
    }
  }

  return to_string;
}

static gchar *gst_child_inspector_flags_to_string(GParamFlags flags) {
  guint32 bit_flags;
  gint i;
  gchar *serial_flags = NULL;

  /* Walk through all the bits in the flags */
  bit_flags = flags;
  for (i = 31; i >= 0; --i) {
    const gchar *serial_flag = NULL;
    guint32 bit_flag = 0;

    /* Filter the desired bit */
    bit_flag = bit_flags & (1 << i);
    serial_flag =
        gst_child_inspector_flag_to_string(static_cast<GParamFlags>(bit_flag));

    /* Is this a flag we want to serialize? */
    if (serial_flag) {
      /* No trailing comma needed on the first case */
      if (!serial_flags) {
        serial_flags = g_strdup(serial_flag);
      } else {
        serial_flags = g_strdup_printf("%s, %s", serial_flag, serial_flags);
      }
    }
  }

  return serial_flags;
}

static gchar *gst_child_inspector_type_to_string(GParamSpec *pspec,
                                                 GValue *value,
                                                 gint alignment) {
  GstChildInspectorType *current_type;
  const GType value_type = G_VALUE_TYPE(value);
  gchar *to_string = NULL;

  g_return_val_if_fail(pspec, NULL);
  g_return_val_if_fail(value, NULL);

  for (current_type = types; current_type->type_to_string; ++current_type) {
    if (g_type_is_a(value_type, current_type->value)) {
      if (GST_PYLON_PARAM_FLAG_IS_SET(pspec)) {
        to_string = current_type->selector_to_string(pspec, value, alignment);
      } else {
        to_string = current_type->type_to_string(pspec, value, alignment);
      }
      break;
    }
  }

  return to_string;
}

gchar *gst_child_inspector_property_to_string(GObject *object,
                                              GParamSpec *param,
                                              guint alignment) {
  GValue value = {
      0,
  };
  const gchar *name;
  const gchar *blurb;
  gchar *flags = NULL;
  gchar *type;
  gchar *prop;

  g_return_val_if_fail(param, NULL);
  g_return_val_if_fail(G_IS_OBJECT(object), NULL);

  name = g_param_spec_get_name(param);
  blurb = g_param_spec_get_blurb(param);

  flags = gst_child_inspector_flags_to_string(param->flags);

  g_value_init(&value, param->value_type);
  g_param_value_set_default(param, &value);

  type = gst_child_inspector_type_to_string(param, &value, alignment);
  g_value_unset(&value);

  prop = g_strdup_printf(
      "%*s%-35s: %s\n"
      "%*s%-36.36s flags: %s\n"
      "%*s%-36.36s %s",
      alignment, "", name, blurb, alignment, "", "", flags, alignment, "", "",
      type);

  g_free(type);
  g_free(flags);

  return prop;
}

gchar *gst_child_inspector_properties_to_string(GObject *object,
                                                guint alignment, gchar *title) {
  GParamSpec **property_specs;
  guint num_properties, i;
  GString *props = NULL;
  gchar *prop = NULL;

  gchar *props_ = NULL;

  g_return_val_if_fail(G_IS_OBJECT(object), NULL);

  GST_LOG_OBJECT(object, "Getting properties to string");

  property_specs = g_object_class_list_properties(G_OBJECT_GET_CLASS(object),
                                                  &num_properties);

  props = g_string_new(title);
  for (i = 0; i < num_properties; i++) {
    prop = gst_child_inspector_property_to_string(object, property_specs[i],
                                                  alignment);
    g_string_append_printf(props, "\n%s", prop);
    g_free(prop);
  }

  g_free(property_specs);
  props_ = g_string_free(props, FALSE);
  return props_;
}
