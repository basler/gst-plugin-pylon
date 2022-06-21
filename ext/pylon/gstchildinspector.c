/*  
 *  Copyright (C) 2019 RidgeRun, LLC (http://www.ridgerun.com)
 *  All Rights Reserved. 
 *
 *  The contents of this software are proprietary and confidential to RidgeRun, 
 *  LLC.  No part of this program may be photocopied, reproduced or translated 
 *  into another programming language without prior written consent of
 *  RidgeRun, LLC.  The user is free to modify the source code after obtaining 
 *  a software license from RidgeRun.  All source code changes must be provided 
 *  back to RidgeRun without any encumbrance. 
 */

#include "gstchildinspector.h"

typedef struct _GstChildInspectorFlag GstChildInspectorFlag;
typedef struct _GstChildInspectorType GstChildInspectorType;

typedef gchar *(*GstChildInspectorTypeToString) (GParamSpec * pspec,
    GValue * value);

static gchar *gst_child_inspector_type_int_to_string (GParamSpec * pspec,
    GValue * value);
static gchar *gst_child_inspector_type_bool_to_string (GParamSpec * pspec,
    GValue * value);
static gchar *gst_child_inspector_type_float_to_string (GParamSpec * pspec,
    GValue * value);
static gchar *gst_child_inspector_type_string_to_string (GParamSpec * pspec,
    GValue * value);

struct _GstChildInspectorFlag
{
  gint value;
  const gchar *to_string;
};

struct _GstChildInspectorType
{
  GType value;
  GstChildInspectorTypeToString to_string;
};

static GstChildInspectorFlag flags[] = {
  {G_PARAM_READABLE, "readable"},
  {G_PARAM_WRITABLE, "writable"},
  {GST_PARAM_MUTABLE_READY, "changeable only in NULL or READY state"},
  {GST_PARAM_MUTABLE_PLAYING,
      "changeable in NULL, READY, PAUSED or PLAYING state"},
  {}
};

static GstChildInspectorType types[] = {
  {G_TYPE_INT, gst_child_inspector_type_int_to_string},
  {G_TYPE_BOOLEAN, gst_child_inspector_type_bool_to_string},
  {G_TYPE_FLOAT, gst_child_inspector_type_float_to_string},
  {G_TYPE_STRING, gst_child_inspector_type_string_to_string},
  {}
};

static gchar *
gst_child_inspector_type_string_to_string (GParamSpec * pspec, GValue * value)
{
  return g_strdup_printf ("String. Default: \"%s\"",
      g_value_get_string (value));
}

static gchar *
gst_child_inspector_type_int_to_string (GParamSpec * pspec, GValue * value)
{
  GParamSpecInt *pint = G_PARAM_SPEC_INT (pspec);

  return g_strdup_printf ("Integer. Range: %d - %d Default: %d",
      pint->minimum, pint->maximum, g_value_get_int (value));
}

static gchar *
gst_child_inspector_type_float_to_string (GParamSpec * pspec, GValue * value)
{
  GParamSpecFloat *pint = G_PARAM_SPEC_FLOAT (pspec);

  return g_strdup_printf ("Float. Range: %.2f - %.2f Default: %.2f",
      pint->minimum, pint->maximum, g_value_get_float (value));
}

static gchar *
gst_child_inspector_type_bool_to_string (GParamSpec * pspec, GValue * value)
{
  return g_strdup_printf ("Boolean. Default: %s",
      g_value_get_boolean (value) ? "true" : "false");
}

static const gchar *
gst_child_inspector_flag_to_string (GParamFlags flag)
{
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

static gchar *
gst_child_inspector_flags_to_string (GParamFlags flags)
{
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
    serial_flag = gst_child_inspector_flag_to_string (bit_flag);

    /* Is this a flag we want to serialize? */
    if (serial_flag) {
      /* No trailing comma needed on the first case */
      if (!serial_flags) {
        serial_flags = g_strdup (serial_flag);
      } else {
        serial_flags = g_strdup_printf ("%s, %s", serial_flag, serial_flags);
      }
    }
  }

  return serial_flags;
}

static gchar *
gst_child_inspector_type_to_string (GParamSpec * pspec, GValue * value)
{
  GstChildInspectorType *current_type;
  const GType value_type = G_VALUE_TYPE (value);
  gchar *to_string = NULL;

  g_return_val_if_fail (pspec, NULL);
  g_return_val_if_fail (value, NULL);

  for (current_type = types; current_type->to_string; ++current_type) {
    if (current_type->value == value_type) {
      to_string = current_type->to_string (pspec, value);
      break;
    }
  }

  return to_string;
}

gchar *
gst_child_inspector_property_to_string (GObject * object, GParamSpec * param,
    guint alignment)
{
  GValue value = { 0, };
  const gchar *name;
  const gchar *blurb;
  gchar *flags = NULL;
  gchar *type;
  gchar *prop;

  g_return_val_if_fail (param, NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  name = g_param_spec_get_name (param);
  blurb = g_param_spec_get_blurb (param);

  flags = gst_child_inspector_flags_to_string (param->flags);

  g_value_init (&value, param->value_type);
  g_param_value_set_default (param, &value);

  type = gst_child_inspector_type_to_string (param, &value);
  g_value_unset (&value);

  prop = g_strdup_printf ("%*s%-30s: %s\n"
      "%*s%-31.31s flags: %s\n"
      "%*s%-31.31s %s",
      alignment, "", name, blurb,
      alignment, "", "", flags, alignment, "", "", type);

  g_free (type);
  g_free (flags);

  return prop;
}

gchar *
gst_child_inspector_properties_to_string (GObject * object, guint alignment,
    gchar * title)
{
  GParamSpec **property_specs;
  guint num_properties, i;
  GString *props = NULL;
  gchar *prop = NULL;

  gchar *props_ = NULL;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  GST_LOG_OBJECT (object, "Getting properties to string");

  property_specs = g_object_class_list_properties
      (G_OBJECT_GET_CLASS (object), &num_properties);

  props = g_string_new (title);
  for (i = 0; i < num_properties; i++) {
    prop =
        gst_child_inspector_property_to_string (object, property_specs[i],
        alignment);
    g_string_append_printf (props, "\n%s", prop);
  }

  g_free (property_specs);
  props_ = g_string_free (props, FALSE);
  return props_;
}
