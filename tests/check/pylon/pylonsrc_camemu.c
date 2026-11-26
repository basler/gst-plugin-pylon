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

GST_START_TEST(test_pylonsrc_execute_command_and_grab_timeout) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;
  gboolean cmd_ok = FALSE;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("cmd-timeout-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_unless(pipeline && src && sink);

  /* Configure software trigger without firing a second trigger so grab-timeout
   * triggers cleanly, after verifying execute-command works on an open cam. */
  g_object_set(src, "grab-timeout", 150, NULL);
  gst_child_proxy_set(GST_CHILD_PROXY(src), "cam::TriggerMode-FrameStart", 1,
                      "cam::TriggerSource-FrameStart", 0, NULL);

  g_signal_emit_by_name(src, "execute-command", "NonExistentCommandXYZ",
                        &cmd_ok);
  fail_if(cmd_ok, "expected unknown command to return FALSE");

  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);

  /* Fire one software trigger via selector-aware action signal, then let next
   * frame time out */
  g_signal_emit_by_name(src, "execute-command", "TriggerSoftware-FrameStart",
                        &cmd_ok);
  fail_unless(cmd_ok, "expected TriggerSoftware command to succeed in PLAYING");

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus, 3 * GST_SECOND, GST_MESSAGE_ERROR);
  fail_unless(msg != NULL, "expected grab-timeout error message on bus");
  gst_message_unref(msg);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_runtime_framerate_updates_latency) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *capsfilter;
  GstElement *sink;
  GstCaps *caps;
  GstQuery *query;
  GstClockTime min_lat = 0;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("fps-latency-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  capsfilter = gst_element_factory_make("capsfilter", NULL);
  sink = gst_element_factory_make("fakesink", NULL);
  caps = gst_caps_from_string(
      "video/x-raw,format=GRAY8,width=640,height=480,framerate=20/1");
  g_object_set(capsfilter, "caps", caps, NULL);
  gst_caps_unref(caps);

  gst_bin_add_many(GST_BIN(pipeline), src, capsfilter, sink, NULL);
  fail_unless(gst_element_link_many(src, capsfilter, sink, NULL));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);
  /* Wait for PLAYING negotiation */
  fail_unless(gst_element_get_state(pipeline, NULL, NULL, 3 * GST_SECOND) ==
              GST_STATE_CHANGE_SUCCESS);

  gst_child_proxy_set(GST_CHILD_PROXY(src), "cam::AcquisitionFrameRateEnable",
                      TRUE, "cam::AcquisitionFrameRate", 50.0, NULL);

  query = gst_query_new_latency();
  fail_unless(gst_element_query(src, query));
  gst_query_parse_latency(query, NULL, &min_lat, NULL);
  gst_query_unref(query);

  /* 50 fps -> 20 ms frame duration */
  fail_unless(
      min_lat >= 19 * GST_MSECOND && min_lat <= 21 * GST_MSECOND,
      "expected ~20ms latency after setting 50fps, got %" GST_TIME_FORMAT,
      GST_TIME_ARGS(min_lat));

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
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
  tcase_add_test(tc, test_pylonsrc_execute_command_and_grab_timeout);
  tcase_add_test(tc, test_pylonsrc_runtime_framerate_updates_latency);

  return s;
}

GST_CHECK_MAIN(pylonsrc_camemu);
