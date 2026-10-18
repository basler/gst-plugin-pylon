/* Copyright (C) 2026 Basler AG
 *
 * Unit tests for gst_pylon_nvmm_color_kind(). No DeepStream / Jetson headers
 * required. Foundation for NVMM full-range follow-ups (#154).
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

static void TestLimitedYuvDefaults() {
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_YUY2,
                                  GST_VIDEO_COLOR_RANGE_16_235) ==
            GstPylonNvmmColorKind::Yuyv,
        "limited YUY2 -> Yuyv");
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_UYVY,
                                  GST_VIDEO_COLOR_RANGE_16_235) ==
            GstPylonNvmmColorKind::Uyvy,
        "limited UYVY -> Uyvy");
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_YUY2,
                                  GST_VIDEO_COLOR_RANGE_UNKNOWN) ==
            GstPylonNvmmColorKind::Yuyv,
        "unknown-range YUY2 -> Yuyv (not ER)");
}

static void TestFullRangeYuvRequestsEr() {
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_YUY2,
                                  GST_VIDEO_COLOR_RANGE_0_255) ==
            GstPylonNvmmColorKind::YuyvEr,
        "full-range YUY2 -> YuyvEr");
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_UYVY,
                                  GST_VIDEO_COLOR_RANGE_0_255) ==
            GstPylonNvmmColorKind::UyvyEr,
        "full-range UYVY -> UyvyEr");
  CHECK(gst_pylon_nvmm_color_kind_is_extended(GstPylonNvmmColorKind::YuyvEr),
        "YuyvEr is extended");
  CHECK(gst_pylon_nvmm_color_kind_is_extended(GstPylonNvmmColorKind::UyvyEr),
        "UyvyEr is extended");
  CHECK(!gst_pylon_nvmm_color_kind_is_extended(GstPylonNvmmColorKind::Yuyv),
        "Yuyv is not extended");
}

static void TestNonYuvUnchangedByRange() {
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_GRAY8,
                                  GST_VIDEO_COLOR_RANGE_0_255) ==
            GstPylonNvmmColorKind::Gray8,
        "GRAY8 ignores range");
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_RGB,
                                  GST_VIDEO_COLOR_RANGE_16_235) ==
            GstPylonNvmmColorKind::Rgb,
        "RGB ignores range");
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_BGR,
                                  GST_VIDEO_COLOR_RANGE_0_255) ==
            GstPylonNvmmColorKind::Bgr,
        "BGR ignores range");
}

static void TestUnsupportedAndCapsDefaults() {
  CHECK(gst_pylon_nvmm_color_kind(GST_VIDEO_FORMAT_NV12,
                                  GST_VIDEO_COLOR_RANGE_0_255) ==
            GstPylonNvmmColorKind::Unsupported,
        "NV12 is unsupported by pylonsrc NVMM factory");

  /* GStreamer defaults YUV without colorimetry to limited range. */
  GstCaps* caps =
      gst_caps_from_string("video/x-raw,format=YUY2,width=64,height=64");
  GstVideoInfo info;
  CHECK(gst_video_info_from_caps(&info, caps), "parse bare YUY2 caps");
  CHECK(info.colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235,
        "bare YUY2 caps default to limited range");
  CHECK(gst_pylon_nvmm_color_kind(info.finfo->format, info.colorimetry.range) ==
            GstPylonNvmmColorKind::Yuyv,
        "bare YUY2 caps map to limited NVMM kind");
  gst_caps_unref(caps);

  caps = gst_caps_from_string(
      "video/x-raw,format=YUY2,width=64,height=64,colorimetry=(string)1:4:5:1");
  CHECK(gst_video_info_from_caps(&info, caps), "parse full-range YUY2 caps");
  CHECK(info.colorimetry.range == GST_VIDEO_COLOR_RANGE_0_255,
        "colorimetry 1:4:5:1 is full range");
  CHECK(gst_pylon_nvmm_color_kind(info.finfo->format, info.colorimetry.range) ==
            GstPylonNvmmColorKind::YuyvEr,
        "full-range YUY2 caps map to YuyvEr");
  gst_caps_unref(caps);
}

int main(int argc, char** argv) {
  gst_init(&argc, &argv);

  TestLimitedYuvDefaults();
  TestFullRangeYuvRequestsEr();
  TestNonYuvUnchangedByRange();
  TestUnsupportedAndCapsDefaults();

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All nvmm color tests passed\n");
  return EXIT_SUCCESS;
}
