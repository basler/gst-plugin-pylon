/* Copyright (C) 2026 Basler AG
 *
 * gstcheck tests for pylonsrc using PYLON_CAMEMU (no physical camera).
 * Buffer and format coverage lives in tests/camemu/run_camemu_tests.sh.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/pylon/gstpylonmeta.h>

#define EMU_SERIAL "0815-0000"
#define EMU_SERIAL_2 "0815-0001"

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

#ifdef GST_PYLON_ENABLE_TEST_HOOKS
static guint query_runtime_use_count(GstElement *src) {
  GstStructure *s = gst_structure_new_empty("pylonsrc-test-runtime-use-count");
  GstQuery *q = gst_query_new_custom(GST_QUERY_CUSTOM, s);
  guint count = 9999;
  fail_unless(gst_element_query(src, q));
  fail_unless(
      gst_structure_get_uint(gst_query_get_structure(q), "count", &count));
  gst_query_unref(q);
  return count;
}

static void query_trigger_disconnect(GstElement *src) {
  GstStructure *s = gst_structure_new_empty("pylonsrc-test-trigger-disconnect");
  GstQuery *q = gst_query_new_custom(GST_QUERY_CUSTOM, s);
  fail_unless(gst_element_query(src, q));
  gst_query_unref(q);
}

static void query_set_fail_alloc_nth(GstElement *src, gint nth) {
  GstStructure *s = gst_structure_new("pylonsrc-test-fail-alloc-nth", "nth",
                                      G_TYPE_INT, nth, NULL);
  GstQuery *q = gst_query_new_custom(GST_QUERY_CUSTOM, s);
  fail_unless(gst_element_query(src, q));
  gst_query_unref(q);
}

static void query_set_inject_std_exception(GstElement *src, gboolean enabled) {
  GstStructure *s = gst_structure_new("pylonsrc-test-inject-std-exception",
                                      "enable", G_TYPE_BOOLEAN, enabled, NULL);
  GstQuery *q = gst_query_new_custom(GST_QUERY_CUSTOM, s);
  fail_unless(gst_element_query(src, q));
  gst_query_unref(q);
}

GST_START_TEST(test_pylonsrc_runtime_init_balance) {
  GstElement *src;

  require_camemu_devices(2);
  src = make_pylonsrc_with_serial(EMU_SERIAL);

  for (int i = 0; i < 20; i++) {
    const gchar *serial = (i % 2 == 0) ? EMU_SERIAL : EMU_SERIAL_2;
    g_object_set(src, "device-serial-number", serial, NULL);
    fail_unless(gst_element_set_state(src, GST_STATE_READY) !=
                GST_STATE_CHANGE_FAILURE);
    fail_unless(gst_element_set_state(src, GST_STATE_NULL) !=
                GST_STATE_CHANGE_FAILURE);
  }

  fail_unless_equals_int(query_runtime_use_count(src), 0);
  gst_object_unref(src);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_buffers_outlive_session) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstSample *samples[3] = {NULL, NULL, NULL};

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("buffers-outlive-session");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  sink = gst_element_factory_make("appsink", NULL);
  fail_unless(pipeline && src && sink);
  g_object_set(sink, "sync", FALSE, "max-buffers", 10, NULL);

  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);

  for (int i = 0; i < 3; i++) {
    g_signal_emit_by_name(sink, "pull-sample", &samples[i]);
    fail_unless(samples[i] != NULL, "failed to pull sample %d", i);
  }

  /* Shut down the session completely while samples are still held. */
  fail_unless(gst_element_set_state(pipeline, GST_STATE_NULL) !=
              GST_STATE_CHANGE_FAILURE);

  /* Runtime must still be alive because buffers hold GstPylonRuntime refs. */
  fail_unless(query_runtime_use_count(src) > 0,
              "expected runtime to remain alive while buffers are held");

  for (int i = 0; i < 3; i++) {
    GstBuffer *buf = gst_sample_get_buffer(samples[i]);
    GstMapInfo map;
    volatile guint8 sum;
    fail_unless(gst_buffer_map(buf, &map, GST_MAP_READ));
    fail_unless(map.size > 0);
    /* Touch first and last byte to ensure memory is valid under ASan. */
    sum = map.data[0] + map.data[map.size - 1];
    (void)sum;
    gst_buffer_unmap(buf, &map);
    gst_sample_unref(samples[i]);
  }

  fail_unless_equals_int(query_runtime_use_count(src), 0);

  /* Run a second cycle to confirm clean re-initialization. */
  {
    GstSample *second = NULL;
    fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
                GST_STATE_CHANGE_FAILURE);
    g_signal_emit_by_name(sink, "pull-sample", &second);
    fail_unless(second != NULL);
    gst_sample_unref(second);
  }

  fail_unless(gst_element_set_state(pipeline, GST_STATE_NULL) !=
              GST_STATE_CHANGE_FAILURE);
  fail_unless_equals_int(query_runtime_use_count(src), 0);
  gst_object_unref(pipeline);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_disconnect_warning_not_error) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;
  gboolean saw_warning = FALSE;
  gboolean saw_reconnecting = FALSE;
  gboolean saw_reconnected = FALSE;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("disconnect-warning-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_unless(pipeline && src && sink);

  g_object_set(src, "auto-reconnect", TRUE, NULL);
  g_object_set(sink, "sync", FALSE, NULL);
  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);
  fail_unless(gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND) !=
              GST_STATE_CHANGE_FAILURE);

  query_trigger_disconnect(src);

  bus = gst_element_get_bus(pipeline);
  {
    gint64 deadline = g_get_monotonic_time() + 10 * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline && !saw_reconnected) {
      msg = gst_bus_timed_pop_filtered(
          bus, 200 * GST_MSECOND,
          (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING |
                           GST_MESSAGE_ELEMENT));
      if (!msg) {
        continue;
      }
      if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        gst_message_unref(msg);
        fail(
            "auto-reconnect=true must not post GST_MESSAGE_ERROR on "
            "disconnect");
      } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_WARNING) {
        saw_warning = TRUE;
      } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT) {
        const GstStructure *ms = gst_message_get_structure(msg);
        if (ms && gst_structure_has_name(ms, "pylonsrc-reconnect")) {
          const gchar *status = gst_structure_get_string(ms, "status");
          if (g_strcmp0(status, "reconnecting") == 0) {
            saw_reconnecting = TRUE;
          } else if (g_strcmp0(status, "reconnected") == 0) {
            saw_reconnected = TRUE;
          }
        }
      }
      gst_message_unref(msg);
    }
  }
  gst_object_unref(bus);

  fail_unless(saw_warning, "expected GST_MESSAGE_WARNING on disconnect");
  fail_unless(saw_reconnecting, "expected pylonsrc-reconnect reconnecting");
  fail_unless(saw_reconnected, "expected pylonsrc-reconnect reconnected");

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_reconnect_fast_shutdown) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;
  gboolean saw_reconnecting = FALSE;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("reconnect-shutdown-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_unless(pipeline && src && sink);

  g_object_set(src, "auto-reconnect", TRUE, NULL);
  g_object_set(sink, "sync", FALSE, NULL);
  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);
  fail_unless(gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND) !=
              GST_STATE_CHANGE_FAILURE);

  query_trigger_disconnect(src);

  bus = gst_element_get_bus(pipeline);
  {
    gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline && !saw_reconnecting) {
      const GstStructure *ms;
      msg = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND,
                                       GST_MESSAGE_ELEMENT);
      if (!msg) {
        continue;
      }
      ms = gst_message_get_structure(msg);
      if (ms && gst_structure_has_name(ms, "pylonsrc-reconnect")) {
        const gchar *status = gst_structure_get_string(ms, "status");
        if (g_strcmp0(status, "reconnecting") == 0) {
          saw_reconnecting = TRUE;
        }
      }
      gst_message_unref(msg);
    }
  }
  gst_object_unref(bus);
  fail_unless(saw_reconnecting, "expected to enter reconnecting state");

  {
    gint64 t0 = g_get_monotonic_time();
    gint64 elapsed_ms;
    fail_unless(gst_element_set_state(pipeline, GST_STATE_NULL) !=
                GST_STATE_CHANGE_FAILURE);
    elapsed_ms = (g_get_monotonic_time() - t0) / 1000;
    fail_unless(elapsed_ms < 500,
                "NULL transition during reconnect took %" G_GINT64_FORMAT
                " ms (expected < 500 ms)",
                elapsed_ms);
  }

  gst_object_unref(pipeline);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_std_exception_in_property) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("std-exception-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_unless(pipeline && src && sink);

  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));
  fail_unless(gst_element_set_state(pipeline, GST_STATE_READY) !=
              GST_STATE_CHANGE_FAILURE);

  query_set_inject_std_exception(src, TRUE);
  gst_child_proxy_set(GST_CHILD_PROXY(src), "cam::AcquisitionFrameRate", 25.0,
                      NULL);
  query_set_inject_std_exception(src, FALSE);

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus, 2 * GST_SECOND, GST_MESSAGE_ERROR);
  fail_unless(msg != NULL,
              "expected GST_MESSAGE_ERROR when std::exception is thrown");
  gst_message_unref(msg);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_factory_alloc_failure) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("alloc-fail-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_unless(pipeline && src && sink);
  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  query_set_fail_alloc_nth(src, 3);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND, GST_MESSAGE_ERROR);
  fail_unless(
      msg != NULL,
      "expected GST_MESSAGE_ERROR when buffer factory allocation fails");
  gst_message_unref(msg);
  gst_object_unref(bus);

  query_set_fail_alloc_nth(src, 0);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  fail_unless_equals_int(query_runtime_use_count(src), 0);
  gst_object_unref(pipeline);
}
GST_END_TEST;
#endif /* GST_PYLON_ENABLE_TEST_HOOKS */

GST_START_TEST(test_pylonsrc_queued_frame_arrival_timestamps) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *capsfilter;
  GstElement *sink;
  GstCaps *caps;
  GstSample *sample = NULL;
  GstClockTime pts[4];

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("arrival-ts-pipe");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  capsfilter = gst_element_factory_make("capsfilter", NULL);
  sink = gst_element_factory_make("appsink", NULL);
  fail_unless(pipeline && src && capsfilter && sink);

  g_object_set(src, "max-pending-frames", 4, NULL);
  g_object_set(sink, "sync", FALSE, "max-buffers", 1, NULL);
  caps = gst_caps_from_string(
      "video/x-raw,format=GRAY8,width=640,height=480,framerate=20/1");
  g_object_set(capsfilter, "caps", caps, NULL);
  gst_caps_unref(caps);

  gst_bin_add_many(GST_BIN(pipeline), src, capsfilter, sink, NULL);
  fail_unless(gst_element_link_many(src, capsfilter, sink, NULL));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);

  /* Pull warmup frame, then sleep ~3.5 frame periods (175ms at 20fps) so
   * multiple frames queue up in GstPylonImageHandler. */
  g_signal_emit_by_name(sink, "pull-sample", &sample);
  fail_unless(sample != NULL);
  gst_sample_unref(sample);

  g_usleep(180000);

  for (int i = 0; i < 4; i++) {
    GstBuffer *buf;
    sample = NULL;
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    fail_unless(sample != NULL);
    buf = gst_sample_get_buffer(sample);
    pts[i] = GST_BUFFER_PTS(buf);
    fail_unless(GST_CLOCK_TIME_IS_VALID(pts[i]), "expected valid PTS");
    gst_sample_unref(sample);
  }

  /* At 20 fps (50 ms/frame), consecutive queued frames should have ~50 ms
   * spacing (well within 30..75 ms), NOT bunched near 0 ms at dequeue time. */
  for (int i = 1; i < 4; i++) {
    GstClockTime delta_ms;
    fail_unless(pts[i] > pts[i - 1], "PTS must be strictly increasing");
    delta_ms = (pts[i] - pts[i - 1]) / GST_MSECOND;
    fail_unless(delta_ms >= 30 && delta_ms <= 80,
                "expected ~50ms arrival PTS spacing, got %" G_GUINT64_FORMAT
                " ms",
                delta_ms);
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}
GST_END_TEST;

GST_START_TEST(test_pylonsrc_caps_query_no_side_effects) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *capsfilter;
  GstElement *sink;
  GstCaps *caps;
  GstPad *srcpad;
  gint64 offset_x = -1;

  require_camemu_devices(1);

  pipeline = gst_pipeline_new("caps-query-side-effects");
  src = make_pylonsrc_with_serial(EMU_SERIAL);
  capsfilter = gst_element_factory_make("capsfilter", NULL);
  sink = gst_element_factory_make("appsink", NULL);
  fail_unless(pipeline && src && capsfilter && sink);

  g_object_set(sink, "sync", FALSE, "max-buffers", 10, NULL);
  caps = gst_caps_from_string("video/x-raw,format=GRAY8,width=320,height=240");
  g_object_set(capsfilter, "caps", caps, NULL);
  gst_caps_unref(caps);

  gst_child_proxy_set(GST_CHILD_PROXY(src), "cam::OffsetX", (gint64)16, NULL);

  gst_bin_add_many(GST_BIN(pipeline), src, capsfilter, sink, NULL);
  fail_unless(gst_element_link_many(src, capsfilter, sink, NULL));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);
  fail_unless(gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND) !=
              GST_STATE_CHANGE_FAILURE);

  srcpad = gst_element_get_static_pad(src, "src");
  fail_unless(srcpad != NULL);

  for (int i = 0; i < 20; i++) {
    GstCaps *qcaps = gst_pad_query_caps(srcpad, NULL);
    fail_unless(qcaps != NULL && !gst_caps_is_empty(qcaps));
    gst_caps_unref(qcaps);
  }
  gst_object_unref(srcpad);

  gst_child_proxy_get(GST_CHILD_PROXY(src), "cam::OffsetX", &offset_x, NULL);
  fail_unless_equals_int64(offset_x, 16);

  for (int i = 0; i < 3; i++) {
    GstSample *sample = NULL;
    GstBuffer *buf;
    GstPylonMeta *meta;
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    fail_unless(sample != NULL);
    buf = gst_sample_get_buffer(sample);
    meta = gst_buffer_get_pylon_meta(buf);
    fail_unless(meta != NULL);
    fail_unless_equals_uint64(meta->offset.offset_x, 16);
    gst_sample_unref(sample);
  }

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
  tcase_add_test(tc, test_pylonsrc_queued_frame_arrival_timestamps);
  tcase_add_test(tc, test_pylonsrc_caps_query_no_side_effects);
#ifdef GST_PYLON_ENABLE_TEST_HOOKS
  tcase_add_test(tc, test_pylonsrc_runtime_init_balance);
  tcase_add_test(tc, test_pylonsrc_buffers_outlive_session);
  tcase_add_test(tc, test_pylonsrc_disconnect_warning_not_error);
  tcase_add_test(tc, test_pylonsrc_reconnect_fast_shutdown);
  tcase_add_test(tc, test_pylonsrc_std_exception_in_property);
  tcase_add_test(tc, test_pylonsrc_factory_alloc_failure);
#endif

  return s;
}

GST_CHECK_MAIN(pylonsrc_camemu);
