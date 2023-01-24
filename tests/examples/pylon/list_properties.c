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

/*
 * Example showing how to list pylon device properties
 */

#include <gst/gst.h>
#include <stdlib.h>

static gchar *
flags_to_string (GParamSpec * spec)
{
  GString *flags = g_string_new (NULL);

  g_return_val_if_fail (spec, NULL);

  if (spec->flags & G_PARAM_READABLE) {
    g_string_append_printf (flags, "readable,");
  }
  if (spec->flags & G_PARAM_WRITABLE) {
    g_string_append_printf (flags, "writable,");
  }
  if (spec->flags & GST_PARAM_MUTABLE_READY) {
    g_string_append_printf (flags, "changeable only in NULL or READY state");
  }
  if (spec->flags & GST_PARAM_MUTABLE_PLAYING) {
    g_string_append_printf (flags,
        "changeable only in NULL, READY, PAUSED or PLAYING state");
  }

  return g_string_free (flags, FALSE);
}

static gchar *
string_to_string (GParamSpec * spec)
{
  GParamSpecString *pstring = G_PARAM_SPEC_STRING (spec);

  return g_strdup_printf ("String. Default: \"%s\"", pstring->default_value);
}

static gchar *
boolean_to_string (GParamSpec * spec)
{
  GParamSpecBoolean *pboolean = G_PARAM_SPEC_BOOLEAN (spec);
  const gchar *value = pboolean->default_value ? "true" : "false";

  return g_strdup_printf ("Boolean. Default: %s", value);
}

static gchar *
int64_to_string (GParamSpec * spec)
{
  GParamSpecInt64 *pint = G_PARAM_SPEC_INT64 (spec);

  return g_strdup_printf ("Integer64. Range: %" G_GINT64_FORMAT
      " - %" G_GINT64_FORMAT " Default: %" G_GINT64_FORMAT,
      pint->minimum, pint->maximum, pint->default_value);
}

static gchar *
double_to_string (GParamSpec * spec)
{
  GParamSpecDouble *pdouble = G_PARAM_SPEC_DOUBLE (spec);

  return g_strdup_printf ("Double. Range: %.2f - %.2f Default: %.2f",
      pdouble->minimum, pdouble->maximum, pdouble->default_value);
}

static gchar *
enum_to_string (GParamSpec * spec)
{
  GParamSpecEnum *penum = G_PARAM_SPEC_ENUM (spec);
  GString *desc = g_string_new (NULL);
  GType type = G_TYPE_FROM_CLASS (penum->enum_class);
  gchar *def = g_enum_to_string (type, penum->default_value);
  const gchar *type_name = g_type_name (type);
  GEnumValue *iter = NULL;

  g_string_append_printf (desc, "Enum \"%s\" Default: %d, \"%s\"",
      type_name, penum->default_value, def);
  g_free (def);

  for (iter = penum->enum_class->values; iter->value_name; iter++) {
    if (iter->value_nick[0] == '\0') {
      g_string_append_printf (desc, "\n\t(%d): %s", iter->value,
          iter->value_name);
    } else {
      g_string_append_printf (desc, "\n\t(%d): %s - %s", iter->value,
          iter->value_name, iter->value_nick);
    }
  }

  return g_string_free (desc, FALSE);
}

static gchar *
property_to_string (GParamSpec * spec)
{
  const gchar *name = NULL;
  const gchar *blurb = NULL;
  gchar *flags = NULL;
  gchar *details = NULL;
  gchar *prop = NULL;

  g_return_val_if_fail (spec, NULL);

  name = g_param_spec_get_name (spec);
  blurb = g_param_spec_get_blurb (spec);
  flags = flags_to_string (spec);

  switch (G_TYPE_FUNDAMENTAL (spec->value_type)) {
    case G_TYPE_INT64:
      details = int64_to_string (spec);
      break;
    case G_TYPE_STRING:
      details = string_to_string (spec);
      break;
    case G_TYPE_DOUBLE:
      details = double_to_string (spec);
      break;
    case G_TYPE_ENUM:
      details = enum_to_string (spec);
      break;
    case G_TYPE_BOOLEAN:
      details = boolean_to_string (spec);
      break;
    default:
      details = g_strdup ("unknown type");
      break;
  }

  prop = g_strdup_printf ("Name: %s\nBlurb: %s\nFlags: %s\nDetails: %s\n",
      name, blurb, flags, details);

  g_free (flags);
  g_free (details);

  return prop;
}

static void
print_device_properties (GObject * object)
{
  GParamSpec **property_specs = NULL;
  guint num_properties = 0, i = 0;

  g_return_if_fail (object);

  property_specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (object),
      &num_properties);

  for (i = 0; i < num_properties; i++) {
    gchar *property = property_to_string (property_specs[i]);

    g_print ("%s", property);
    g_print ("-----------\n");

    g_free (property);
  }
}

int
main (int argc, char **argv)
{
  GstElement *element = NULL;
  GstChildProxy *child_proxy = NULL;
  gint device = 0;
  gint num_subdevices = 0;
  gint ret = EXIT_FAILURE;

  /* Make sure we have at least an emulator running */
  g_setenv ("PYLON_CAMEMU", "1", FALSE);

  gst_init (&argc, &argv);

  element = gst_element_factory_make ("pylonsrc", NULL);
  if (NULL == element) {
    g_printerr
        ("Unable to create a pylon camera, activate GST_DEBUG=2 to find the cause\n");
    goto out;
  }

  child_proxy = GST_CHILD_PROXY (element);

  /* After selecting each device, a number of child subdevices may be accessed */
  num_subdevices = gst_child_proxy_get_children_count (child_proxy);

  for (device = 0;; device++) {
    gint i = 0;
    GObject *subdevice = NULL;

    g_object_set (element, "device-index", device, NULL);

    for (i = 0; i < num_subdevices; ++i) {
      subdevice = gst_child_proxy_get_child_by_index (child_proxy, i);

      if (NULL == subdevice) {
        /* No more devices */
        goto no_more_devices;
      }

      /* Only print main title if there is a subdevice available */
      if (0 == i) {
        g_print
            ("**************************\nDevice Index: %d\n**************************\n",
            device);
      }

      g_print ("\n%s:\n==========================\n",
          GST_OBJECT_NAME (subdevice));
      print_device_properties (subdevice);

      gst_object_unref (subdevice);
    }
  }

no_more_devices:
  gst_object_unref (element);

out:
  gst_deinit ();

  return ret;
}
