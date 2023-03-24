/* Copyright (C) 2023 Basler AG
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
