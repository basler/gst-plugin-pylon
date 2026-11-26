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

static void require_camemu_devices(guint minimum) {
  const gchar *env = g_getenv("PYLON_CAMEMU");
  guint64 count;

  fail_unless(env != NULL, "PYLON_CAMEMU must be set for camemu tests");
  count = g_ascii_strtoull(env, NULL, 10);
  fail_unless(
      count >= minimum,
      "PYLON_CAMEMU must provide at least %u device(s), got %" G_GUINT64_FORMAT,
      minimum, count);
}

static GstElement *make_pylonsrc_with_serial(const gchar *serial) {
  GstElement *src;

  src = gst_element_factory_make("pylonsrc", NULL);
  fail_if(src == NULL, "failed to create pylonsrc (is GST_PLUGIN_PATH set?)");
  g_object_set(src, "device-serial-number", serial, NULL);
  return src;
}

GST_START_TEST(test_pylonsrc_factory) {
  GstElement *src;

  src = gst_element_factory_make("pylonsrc", NULL);
  fail_unless(src != NULL);
  gst_object_unref(src);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_null_state) {
  GstElement *src;
  GstStateChangeReturn ret;

  src = make_pylonsrc_with_serial(EMU_SERIAL);
  ret = gst_element_set_state(src, GST_STATE_NULL);
  fail_unless(ret != GST_STATE_CHANGE_FAILURE);
  gst_object_unref(src);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_ready_state) {
  GstElement *src;
  GstStateChangeReturn ret;

  require_camemu_devices(1);
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  ret = gst_element_set_state(src, GST_STATE_READY);
  fail_unless(ret != GST_STATE_CHANGE_FAILURE);
  gst_element_set_state(src, GST_STATE_NULL);
  gst_object_unref(src);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_serial_property) {
  GstElement *src;
  gchar *serial = NULL;

  src = make_pylonsrc_with_serial(EMU_SERIAL);
  g_object_get(src, "device-serial-number", &serial, NULL);
  fail_unless(serial != NULL);
  fail_unless_equals_string(serial, EMU_SERIAL);
  g_free(serial);
  gst_object_unref(src);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_ambiguous_device_fails) {
  GstElement *src;
  GstStateChangeReturn ret;

  require_camemu_devices(3);

  src = gst_element_factory_make("pylonsrc", NULL);
  fail_if(src == NULL);
  g_object_set(src, "num-buffers", 1, NULL);

  ret = gst_element_set_state(src, GST_STATE_PLAYING);
  fail_unless(ret == GST_STATE_CHANGE_FAILURE,
              "expected PLAYING to fail without device selection when multiple "
              "devices exist");

  gst_element_set_state(src, GST_STATE_NULL);
  gst_object_unref(src);
}
GST_END_TEST;

GST_START_TEST(test_pylon_device_provider_probe_and_create) {
  GstDeviceProvider *provider;
  GList *devices;
  GList *iter;
  GstDevice *matched = NULL;
  GstElement *elem;
  gchar *serial = NULL;

  require_camemu_devices(1);

  provider = gst_device_provider_factory_get_by_name("pylondeviceprovider");
  fail_unless(provider != NULL, "pylondeviceprovider should be registered");

  devices = gst_device_provider_get_devices(provider);
  fail_unless(devices != NULL, "expected at least one emulated Pylon device");

  for (iter = devices; iter != NULL; iter = iter->next) {
    GstDevice *dev = GST_DEVICE(iter->data);
    GstStructure *props = gst_device_get_properties(dev);
    if (props) {
      const gchar *s = gst_structure_get_string(props, "device.serial");
      if (g_strcmp0(s, EMU_SERIAL) == 0) {
        matched = dev;
      }
      gst_structure_free(props);
    }
  }
  fail_unless(matched != NULL,
              "expected CamEmu device " EMU_SERIAL " in device provider list");

  elem = gst_device_create_element(matched, "cam0");
  fail_unless(elem != NULL);
  g_object_get(elem, "device-serial-number", &serial, NULL);
  fail_unless_equals_string(serial, EMU_SERIAL);
  g_free(serial);
  gst_object_unref(elem);

  g_list_free_full(devices, gst_object_unref);
  gst_object_unref(provider);
}
GST_END_TEST;

GST_START_TEST(test_pylon_device_provider_skip_tl) {
  GstDeviceProvider *provider;
  GList *devices;

  require_camemu_devices(1);

  g_setenv("PYLONSRC_SKIP_TRANSPORT_LAYERS", "camemu", TRUE);
  provider = gst_device_provider_factory_get_by_name("pylondeviceprovider");
  fail_unless(provider != NULL);

  devices = gst_device_provider_get_devices(provider);
  for (GList *iter = devices; iter != NULL; iter = iter->next) {
    GstDevice *dev = GST_DEVICE(iter->data);
    GstStructure *props = gst_device_get_properties(dev);
    if (props) {
      const gchar *tl =
          gst_structure_get_string(props, "pylon.transport-layer");
      fail_if(g_strcmp0(tl, "camemu") == 0,
              "camemu transport layer should have been skipped");
      gst_structure_free(props);
    }
  }
  g_list_free_full(devices, gst_object_unref);
  gst_object_unref(provider);
  g_unsetenv("PYLONSRC_SKIP_TRANSPORT_LAYERS");
}
GST_END_TEST;

static Suite *pylonsrc_camemu_suite(void) {
  Suite *s = suite_create("pylonsrc_camemu");
  TCase *tc = tcase_create("general");

  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_pylonsrc_factory);
  tcase_add_test(tc, test_pylonsrc_null_state);
  tcase_add_test(tc, test_pylonsrc_ready_state);
  tcase_add_test(tc, test_pylonsrc_serial_property);
  tcase_add_test(tc, test_pylonsrc_ambiguous_device_fails);
  tcase_add_test(tc, test_pylon_device_provider_probe_and_create);
  tcase_add_test(tc, test_pylon_device_provider_skip_tl);

  return s;
}

GST_CHECK_MAIN(pylonsrc_camemu);
