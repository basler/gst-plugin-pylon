/* Copyright (C) 2026 Basler AG
 *
 * Unit tests for GstPylonMeta registration and buffer attachment.
 * Does not require a physical camera.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/pylon/gstpylonmeta.h>

GST_START_TEST (test_pylon_meta_api_registration)
{
  GType api_type;
  const GstMetaInfo *info;

  api_type = GST_PYLON_META_API_TYPE;
  info = GST_PYLON_META_INFO;

  fail_unless (api_type != 0);
  fail_unless (info != NULL);
  fail_unless_equals_string (g_type_name (api_type), "GstPylonMetaAPI");
}
GST_END_TEST;

GST_START_TEST (test_pylon_meta_absent_on_new_buffer)
{
  GstBuffer *buffer;

  buffer = gst_buffer_new ();
  fail_unless (buffer != NULL);
  fail_unless (gst_buffer_get_pylon_meta (buffer) == NULL);

  gst_buffer_unref (buffer);
}
GST_END_TEST;

GST_START_TEST (test_pylon_meta_attach_initializes_chunks)
{
  GstBuffer *buffer;
  GstPylonMeta *meta;

  buffer = gst_buffer_new ();
  fail_unless (buffer != NULL);

  meta = (GstPylonMeta *) gst_buffer_add_meta (buffer, GST_PYLON_META_INFO,
      NULL);
  fail_unless (meta != NULL);
  fail_unless (gst_buffer_get_pylon_meta (buffer) == meta);
  fail_unless (meta->chunks != NULL);
  fail_unless_equals_string (gst_structure_get_name (meta->chunks),
      "meta/x-pylon");
  fail_unless_equals_int (gst_structure_n_fields (meta->chunks), 0);

  gst_buffer_unref (buffer);
}
GST_END_TEST;

static Suite *
pylon_meta_suite (void)
{
  Suite *s = suite_create ("pylon_meta");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, test_pylon_meta_api_registration);
  tcase_add_test (tc, test_pylon_meta_absent_on_new_buffer);
  tcase_add_test (tc, test_pylon_meta_attach_initializes_chunks);

  return s;
}

GST_CHECK_MAIN (pylon_meta);
