#ifndef _GST_PYLON_FORMAT_MAPPING_
#define _GST_PYLON_FORMAT_MAPPING_

#include <string>
#include <vector>

bool isSupportedPylonFormat(const std::string &format);

/* Pixel format definitions */
typedef struct {
  std::string pfnc_name;
  std::string gst_name;
} PixelFormatMappingType;

const std::vector<PixelFormatMappingType> pixel_format_mapping_raw = {
    {"Mono8", "GRAY8"},        {"RGB8Packed", "RGB"},
    {"BGR8Packed", "BGR"},     {"RGB8", "RGB"},
    {"BGR8", "BGR"},           {"YCbCr422_8", "YUY2"},
    {"YUV422_8_UYVY", "UYVY"}, {"YUV422_8", "YUY2"},
    {"YUV422Packed", "UYVY"},  {"YUV422_YUYV_Packed", "YUY2"}};

const std::vector<PixelFormatMappingType> pixel_format_mapping_bayer = {
    {"BayerBG8", "bggr"},
    {"BayerGR8", "grbg"},
    {"BayerRG8", "rggb"},
    {"BayerGB8", "gbrg"}};

bool isSupportedPylonFormat(const std::string &format) {
  bool res = false;
  for (const auto &fd : pixel_format_mapping_raw) {
    if (fd.pfnc_name == format) {
      res = true;
      break;
    }
  }
  if (!res) {
    for (const auto &fd : pixel_format_mapping_bayer) {
      if (fd.pfnc_name == format) {
        res = true;
        break;
      }
    }
  }
  return res;
}

#endif
