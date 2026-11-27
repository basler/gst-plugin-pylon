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

static Suite *pylonsrc_concurrency_suite(void) {
  Suite *s = suite_create("pylonsrc_concurrency");
  TCase *tc = tcase_create("concurrency");

  tcase_add_test(tc, test_pylonsrc_basic_pinned_lifecycle);
  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(pylonsrc_concurrency);
