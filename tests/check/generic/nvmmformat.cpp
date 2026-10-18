/* Copyright (C) 2026 Basler AG
 *
 * Maps gst_pylon_nvmm_color_format() onto real NvBufSurfaceColorFormat values.
 * Build with -DNVMM_ENABLED and nvbufsurface.h on the include path (see
 * tools/test_nvmm_color_format_docker.sh).
 */

#include "../../../ext/pylon/gstpylonnvmmcolor.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;

#define CHECK(cond, msg)                       \
  do {                                         \
    if (!(cond)) {                             \
      std::fprintf(stderr, "FAIL: %s\n", msg); \
      failures++;                              \
    } else {                                   \
      std::printf("ok: %s\n", msg);            \
    }                                          \
  } while (0)

#ifndef NVMM_ENABLED
#  error "nvmmformat test requires -DNVMM_ENABLED and nvbufsurface.h"
#endif

static void TestDirectKindMapping() {
  CHECK(gst_pylon_nvmm_kind_to_nvbuf(GstPylonNvmmColorKind::Yuyv) ==
            NVBUF_COLOR_FORMAT_YUYV,
        "Yuyv -> NVBUF_COLOR_FORMAT_YUYV");
  CHECK(gst_pylon_nvmm_kind_to_nvbuf(GstPylonNvmmColorKind::Uyvy) ==
            NVBUF_COLOR_FORMAT_UYVY,
        "Uyvy -> NVBUF_COLOR_FORMAT_UYVY");
  CHECK(gst_pylon_nvmm_kind_to_nvbuf(GstPylonNvmmColorKind::YuyvEr) ==
            NVBUF_COLOR_FORMAT_YUYV_ER,
        "YuyvEr -> NVBUF_COLOR_FORMAT_YUYV_ER");
  CHECK(gst_pylon_nvmm_kind_to_nvbuf(GstPylonNvmmColorKind::UyvyEr) ==
            NVBUF_COLOR_FORMAT_UYVY_ER,
        "UyvyEr -> NVBUF_COLOR_FORMAT_UYVY_ER");
  CHECK(gst_pylon_nvmm_kind_to_nvbuf(GstPylonNvmmColorKind::Gray8) ==
            NVBUF_COLOR_FORMAT_GRAY8,
        "Gray8 -> NVBUF_COLOR_FORMAT_GRAY8");
}

static void TestCapsDrivenFormats() {
  GstCaps* caps =
      gst_caps_from_string("video/x-raw,format=YUY2,width=64,height=64");
  GstVideoInfo info;
  CHECK(gst_video_info_from_caps(&info, caps), "parse bare YUY2 caps");
  CHECK(
      gst_pylon_nvmm_color_format(info.finfo->format, info.colorimetry.range) ==
          NVBUF_COLOR_FORMAT_YUYV,
      "bare YUY2 caps -> NVBUF_COLOR_FORMAT_YUYV (limited)");
  gst_caps_unref(caps);

  caps = gst_caps_from_string(
      "video/x-raw,format=YUY2,width=64,height=64,colorimetry=(string)1:4:5:1");
  CHECK(gst_video_info_from_caps(&info, caps), "parse full-range YUY2 caps");
  CHECK(
      gst_pylon_nvmm_color_format(info.finfo->format, info.colorimetry.range) ==
          NVBUF_COLOR_FORMAT_YUYV_ER,
      "full-range YUY2 caps -> NVBUF_COLOR_FORMAT_YUYV_ER");
  gst_caps_unref(caps);

  caps = gst_caps_from_string(
      "video/x-raw,format=UYVY,width=64,height=64,colorimetry=(string)1:4:5:1");
  CHECK(gst_video_info_from_caps(&info, caps), "parse full-range UYVY caps");
  CHECK(
      gst_pylon_nvmm_color_format(info.finfo->format, info.colorimetry.range) ==
          NVBUF_COLOR_FORMAT_UYVY_ER,
      "full-range UYVY caps -> NVBUF_COLOR_FORMAT_UYVY_ER");
  gst_caps_unref(caps);

  CHECK(gst_pylon_nvmm_color_format(GST_VIDEO_FORMAT_YUY2,
                                    GST_VIDEO_COLOR_RANGE_0_255) ==
            NVBUF_COLOR_FORMAT_YUYV_ER,
        "direct full-range YUY2 -> YUYV_ER");
  CHECK(gst_pylon_nvmm_color_format(GST_VIDEO_FORMAT_YUY2,
                                    GST_VIDEO_COLOR_RANGE_16_235) ==
            NVBUF_COLOR_FORMAT_YUYV,
        "direct limited YUY2 -> YUYV");
}

int main(int argc, char** argv) {
  gst_init(&argc, &argv);

  std::printf("NVBUF_COLOR_FORMAT_YUYV=%d YUYV_ER=%d UYVY=%d UYVY_ER=%d\n",
              (int)NVBUF_COLOR_FORMAT_YUYV, (int)NVBUF_COLOR_FORMAT_YUYV_ER,
              (int)NVBUF_COLOR_FORMAT_UYVY, (int)NVBUF_COLOR_FORMAT_UYVY_ER);

  TestDirectKindMapping();
  TestCapsDrivenFormats();

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All nvmm format mapping tests passed\n");
  return EXIT_SUCCESS;
}
