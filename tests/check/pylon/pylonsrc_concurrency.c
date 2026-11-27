/* Copyright (C) 2026 Basler AG
 *
 * Concurrency regression tests for pylonsrc (intended to run under TSan/ASan
 * as well as default builds).
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

#define EMU_SERIAL "0815-0000"

static GstElement *make_pinned_pylonsrc(const gchar *serial) {
  GstElement *src = gst_element_factory_make("pylonsrc", NULL);
  fail_if(src == NULL, "failed to create pylonsrc");
  g_object_set(src, "device-serial-number", serial, NULL);
  return src;
}

GST_START_TEST(test_pylonsrc_basic_pinned_lifecycle) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  pipeline = gst_pipeline_new("concurrency-baseline");
  src = make_pinned_pylonsrc(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_if(sink == NULL);

  g_object_set(src, "num-buffers", 3, NULL);
  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  fail_unless(ret != GST_STATE_CHANGE_FAILURE);

  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(
      bus, 15 * GST_SECOND,
      (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
  fail_unless(msg != NULL, "timed out waiting for EOS");
  fail_unless(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS,
              "expected EOS on baseline concurrency lifecycle");
  gst_message_unref(msg);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}
GST_END_TEST;

typedef struct {
  GstElement *src;
  gint stop;
} ChildPropWorkerCtx;

static gpointer child_prop_worker(gpointer data) {
  ChildPropWorkerCtx *ctx = (ChildPropWorkerCtx *)data;
  gdouble val = 1000.0;

  while (!g_atomic_int_get(&ctx->stop)) {
    GObject *cam =
        gst_child_proxy_get_child_by_name(GST_CHILD_PROXY(ctx->src), "cam");
    if (cam) {
      if (g_object_class_find_property(G_OBJECT_GET_CLASS(cam),
                                       "ExposureTime")) {
        g_object_set(cam, "ExposureTime", val, NULL);
        val = (val > 2000.0) ? 1000.0 : val + 100.0;
      } else if (g_object_class_find_property(G_OBJECT_GET_CLASS(cam),
                                              "ExposureTimeAbs")) {
        g_object_set(cam, "ExposureTimeAbs", val, NULL);
        val = (val > 2000.0) ? 1000.0 : val + 100.0;
      }
      g_object_unref(cam);
    }
    g_usleep(2000);
  }
  return NULL;
}

GST_START_TEST(test_pylonsrc_session_swap_race) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  ChildPropWorkerCtx ctx;
  GThread *worker;

  pipeline = gst_pipeline_new("session-swap-race");
  src = make_pinned_pylonsrc(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_if(sink == NULL);
  g_object_set(sink, "sync", FALSE, NULL);

  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  ctx.src = src;
  g_atomic_int_set(&ctx.stop, 0);
  worker = g_thread_new("child-prop-worker", child_prop_worker, &ctx);

  for (int i = 0; i < 4; i++) {
    GstStateChangeReturn ret =
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
    fail_unless(ret != GST_STATE_CHANGE_FAILURE);
    gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
    g_usleep(50000);
    ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    fail_unless(ret != GST_STATE_CHANGE_FAILURE);
  }

  g_atomic_int_set(&ctx.stop, 1);
  g_thread_join(worker);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}
GST_END_TEST;

#ifdef GST_PYLON_ENABLE_TEST_HOOKS
GST_START_TEST(test_pylonsrc_nodemap_swap_during_reconnect) {
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;
  GstStructure *s;
  GstQuery *q;
  gint64 deadline;
  ChildPropWorkerCtx ctx;
  GThread *worker;
  gboolean saw_reconnected = FALSE;

  pipeline = gst_pipeline_new("nodemap-swap-reconnect");
  src = make_pinned_pylonsrc(EMU_SERIAL);
  sink = gst_element_factory_make("fakesink", NULL);
  fail_if(sink == NULL);

  g_object_set(src, "auto-reconnect", TRUE, NULL);
  g_object_set(sink, "sync", FALSE, NULL);
  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  fail_unless(gst_element_link(src, sink));

  fail_unless(gst_element_set_state(pipeline, GST_STATE_PLAYING) !=
              GST_STATE_CHANGE_FAILURE);
  fail_unless(gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND) !=
              GST_STATE_CHANGE_FAILURE);

  ctx.src = src;
  g_atomic_int_set(&ctx.stop, 0);
  worker = g_thread_new("reconnect-prop-worker", child_prop_worker, &ctx);

  /* Trigger simulated disconnect while worker hammers cam:: properties. */
  s = gst_structure_new_empty("pylonsrc-test-trigger-disconnect");
  q = gst_query_new_custom(GST_QUERY_CUSTOM, s);
  fail_unless(gst_element_query(src, q));
  gst_query_unref(q);

  bus = gst_element_get_bus(pipeline);
  deadline = g_get_monotonic_time() + 10 * G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time() < deadline && !saw_reconnected) {
    const GstStructure *ms;
    msg =
        gst_bus_timed_pop_filtered(bus, 200 * GST_MSECOND, GST_MESSAGE_ELEMENT);
    if (!msg) {
      continue;
    }
    ms = gst_message_get_structure(msg);
    if (ms && gst_structure_has_name(ms, "pylonsrc-reconnect")) {
      const gchar *status = gst_structure_get_string(ms, "status");
      if (g_strcmp0(status, "reconnected") == 0) {
        saw_reconnected = TRUE;
      }
    }
    gst_message_unref(msg);
  }
  gst_object_unref(bus);

  g_atomic_int_set(&ctx.stop, 1);
  g_thread_join(worker);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  fail_unless(saw_reconnected, "expected reconnect to complete");
}
GST_END_TEST;
#endif

static Suite *pylonsrc_concurrency_suite(void) {
  Suite *s = suite_create("pylonsrc_concurrency");
  TCase *tc = tcase_create("concurrency");

  tcase_add_test(tc, test_pylonsrc_basic_pinned_lifecycle);
  tcase_add_test(tc, test_pylonsrc_session_swap_race);
#ifdef GST_PYLON_ENABLE_TEST_HOOKS
  tcase_add_test(tc, test_pylonsrc_nodemap_swap_during_reconnect);
#endif
  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(pylonsrc_concurrency);
