/* Copyright (C) 2026 Basler AG
 *
 * gstcheck tests for pylonsrc using PYLON_CAMEMU (no physical camera).
 * Buffer and format coverage lives in tests/camemu/run_camemu_tests.sh.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

#define EMU_SERIAL "0815-0000"

static void
require_camemu_devices (guint minimum)
{
  const gchar *env = g_getenv ("PYLON_CAMEMU");
  guint64 count;

  fail_unless (env != NULL, "PYLON_CAMEMU must be set for camemu tests");
  count = g_ascii_strtoull (env, NULL, 10);
  fail_unless (count >= minimum,
      "PYLON_CAMEMU must provide at least %u device(s), got %" G_GUINT64_FORMAT,
      minimum, count);
}

static GstElement *
make_pylonsrc_with_serial (const gchar * serial)
{
  GstElement *src;

  src = gst_element_factory_make ("pylonsrc", NULL);
  fail_if (src == NULL, "failed to create pylonsrc (is GST_PLUGIN_PATH set?)");
  g_object_set (src, "device-serial-number", serial, NULL);
  return src;
}

GST_START_TEST (test_pylonsrc_factory)
{
  GstElement *src;

  src = gst_element_factory_make ("pylonsrc", NULL);
  fail_unless (src != NULL);
  gst_object_unref (src);
}
GST_END_TEST;

GST_START_TEST (test_pylonsrc_null_state)
{
  GstElement *src;
  GstStateChangeReturn ret;

  src = make_pylonsrc_with_serial (EMU_SERIAL);
  ret = gst_element_set_state (src, GST_STATE_NULL);
  fail_unless (ret != GST_STATE_CHANGE_FAILURE);
  gst_object_unref (src);
}
GST_END_TEST;

GST_START_TEST (test_pylonsrc_ready_state)
{
  GstElement *src;
  GstStateChangeReturn ret;

  require_camemu_devices (1);
  src = make_pylonsrc_with_serial (EMU_SERIAL);
  ret = gst_element_set_state (src, GST_STATE_READY);
  fail_unless (ret != GST_STATE_CHANGE_FAILURE);
  gst_element_set_state (src, GST_STATE_NULL);
  gst_object_unref (src);
}
GST_END_TEST;

GST_START_TEST (test_pylonsrc_serial_property)
{
  GstElement *src;
  gchar *serial = NULL;

  src = make_pylonsrc_with_serial (EMU_SERIAL);
  g_object_get (src, "device-serial-number", &serial, NULL);
  fail_unless (serial != NULL);
  fail_unless_equals_string (serial, EMU_SERIAL);
  g_free (serial);
  gst_object_unref (src);
}
GST_END_TEST;

GST_START_TEST (test_pylonsrc_ambiguous_device_fails)
{
  GstElement *src;
  GstStateChangeReturn ret;

  require_camemu_devices (3);

  src = gst_element_factory_make ("pylonsrc", NULL);
  fail_if (src == NULL);
  g_object_set (src, "num-buffers", 1, NULL);

  ret = gst_element_set_state (src, GST_STATE_PLAYING);
  fail_unless (ret == GST_STATE_CHANGE_FAILURE,
      "expected PLAYING to fail without device selection when multiple devices exist");

  gst_element_set_state (src, GST_STATE_NULL);
  gst_object_unref (src);
}
GST_END_TEST;

static Suite *
pylonsrc_camemu_suite (void)
{
  Suite *s = suite_create ("pylonsrc_camemu");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, test_pylonsrc_factory);
  tcase_add_test (tc, test_pylonsrc_null_state);
  tcase_add_test (tc, test_pylonsrc_ready_state);
  tcase_add_test (tc, test_pylonsrc_serial_property);
  tcase_add_test (tc, test_pylonsrc_ambiguous_device_fails);

  return s;
}

GST_CHECK_MAIN (pylonsrc_camemu);
