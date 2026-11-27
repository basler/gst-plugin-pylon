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

GST_START_TEST(test_pylon_meta_api_registration) {
  GType api_type;
  const GstMetaInfo *info;

  api_type = GST_PYLON_META_API_TYPE;
  info = GST_PYLON_META_INFO;

  fail_unless(api_type != 0);
  fail_unless(info != NULL);
  fail_unless_equals_string(g_type_name(api_type), "GstPylonMetaAPI");
}
GST_END_TEST;

GST_START_TEST(test_pylon_meta_absent_on_new_buffer) {
  GstBuffer *buffer;

  buffer = gst_buffer_new();
  fail_unless(buffer != NULL);
  fail_unless(gst_buffer_get_pylon_meta(buffer) == NULL);

  gst_buffer_unref(buffer);
}
GST_END_TEST;

GST_START_TEST(test_pylon_meta_attach_initializes_chunks) {
  GstBuffer *buffer;
  GstPylonMeta *meta;

  buffer = gst_buffer_new();
  fail_unless(buffer != NULL);

  meta = (GstPylonMeta *)gst_buffer_add_meta(buffer, GST_PYLON_META_INFO, NULL);
  fail_unless(meta != NULL);
  fail_unless(gst_buffer_get_pylon_meta(buffer) == meta);
  fail_unless(meta->chunks != NULL);
  fail_unless_equals_string(gst_structure_get_name(meta->chunks),
                            "meta/x-pylon");
  fail_unless_equals_int(gst_structure_n_fields(meta->chunks), 0);

  gst_buffer_unref(buffer);
}
GST_END_TEST;

GST_START_TEST(test_pylon_meta_survives_buffer_copy) {
  GstBuffer *src, *copy;
  GstPylonMeta *meta, *copied;

  src = gst_buffer_new_allocate(NULL, 16, NULL);
  meta = (GstPylonMeta *)gst_buffer_add_meta(src, GST_PYLON_META_INFO, NULL);
  meta->block_id = 42;
  meta->image_number = 7;
  meta->skipped_images = 3;
  meta->offset.offset_x = 16;
  meta->offset.offset_y = 8;
  meta->timestamp = 1234;
  meta->stride = 640;
  gst_structure_set(meta->chunks, "ExposureTime", G_TYPE_DOUBLE, 1000.0, NULL);

  copy = gst_buffer_copy(src);
  copied = gst_buffer_get_pylon_meta(copy);
  fail_unless(copied != NULL);
  fail_unless(copied != meta);
  fail_unless_equals_uint64(copied->block_id, 42);
  fail_unless_equals_uint64(copied->image_number, 7);
  fail_unless_equals_uint64(copied->skipped_images, 3);
  fail_unless_equals_uint64(copied->offset.offset_x, 16);
  fail_unless_equals_uint64(copied->offset.offset_y, 8);
  fail_unless_equals_uint64(copied->timestamp, 1234);
  fail_unless_equals_uint64(copied->stride, 640);
  fail_unless(copied->chunks != meta->chunks);
  fail_unless(gst_structure_has_field(copied->chunks, "ExposureTime"));

  gst_buffer_unref(copy);
  gst_buffer_unref(src);
}
GST_END_TEST;

static Suite *pylon_meta_suite(void) {
  Suite *s = suite_create("pylon_meta");
  TCase *tc = tcase_create("general");

  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_pylon_meta_api_registration);
  tcase_add_test(tc, test_pylon_meta_absent_on_new_buffer);
  tcase_add_test(tc, test_pylon_meta_attach_initializes_chunks);
  tcase_add_test(tc, test_pylon_meta_survives_buffer_copy);

  return s;
}

GST_CHECK_MAIN(pylon_meta);
