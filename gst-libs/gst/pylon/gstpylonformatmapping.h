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

#include <gst/gstversion.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

/* Pixel format definitions */
struct PixelFormatMappingType {
  std::string_view pfnc_name;
  std::string_view gst_name;
};

struct PixelFormatMappingView {
  const PixelFormatMappingType* data = nullptr;
  std::size_t size = 0;

  constexpr PixelFormatMappingView() = default;
  template <std::size_t N>
  constexpr PixelFormatMappingView(
      const std::array<PixelFormatMappingType, N>& arr)
      : data(arr.data()), size(N) {}

  constexpr const PixelFormatMappingType* begin() const { return data; }
  constexpr const PixelFormatMappingType* end() const { return data + size; }
};

/* Prefer full 16-bit before 12/10-bit when downstream negotiates GRAY16_LE */
inline constexpr std::array<PixelFormatMappingType, 13>
    pixel_format_mapping_raw = {{
        {"Mono8", "GRAY8"},
        {"Mono16", "GRAY16_LE"},
        {"Mono12", "GRAY16_LE"},
        {"Mono10", "GRAY16_LE"},
        {"RGB8Packed", "RGB"},
        {"BGR8Packed", "BGR"},
        {"RGB8", "RGB"},
        {"BGR8", "BGR"},
        {"YCbCr422_8", "YUY2"},
        {"YUV422_8_UYVY", "UYVY"},
        {"YUV422_8", "YUY2"},
        {"YUV422Packed", "UYVY"},
        {"YUV422_YUYV_Packed", "YUY2"},
    }};

/* Subset of raw formats directly supported by NvBufSurface NVMM surfaces */
inline constexpr std::array<PixelFormatMappingType, 10>
    pixel_format_mapping_nvmm_raw = {{
        {"Mono8", "GRAY8"},
        {"RGB8Packed", "RGB"},
        {"BGR8Packed", "BGR"},
        {"RGB8", "RGB"},
        {"BGR8", "BGR"},
        {"YCbCr422_8", "YUY2"},
        {"YUV422_8_UYVY", "UYVY"},
        {"YUV422_8", "YUY2"},
        {"YUV422Packed", "UYVY"},
        {"YUV422_YUYV_Packed", "YUY2"},
    }};

#if GST_CHECK_VERSION(1, 24, 0)
inline constexpr std::array<PixelFormatMappingType, 16>
    pixel_format_mapping_bayer = {{
        {"BayerBG8", "bggr"},
        {"BayerGR8", "grbg"},
        {"BayerRG8", "rggb"},
        {"BayerGB8", "gbrg"},
        {"BayerBG10", "bggr10le"},
        {"BayerGR10", "grbg10le"},
        {"BayerRG10", "rggb10le"},
        {"BayerGB10", "gbrg10le"},
        {"BayerBG12", "bggr12le"},
        {"BayerGR12", "grbg12le"},
        {"BayerRG12", "rggb12le"},
        {"BayerGB12", "gbrg12le"},
        {"BayerBG16", "bggr16le"},
        {"BayerGR16", "grbg16le"},
        {"BayerRG16", "rggb16le"},
        {"BayerGB16", "gbrg16le"},
    }};
#else
inline constexpr std::array<PixelFormatMappingType, 4>
    pixel_format_mapping_bayer = {{
        {"BayerBG8", "bggr"},
        {"BayerGR8", "grbg"},
        {"BayerRG8", "rggb"},
        {"BayerGB8", "gbrg"},
    }};
#endif

inline bool isSupportedPylonFormat(std::string_view format) {
  for (const auto& fd : pixel_format_mapping_raw) {
    if (fd.pfnc_name == format) {
      return true;
    }
  }
  for (const auto& fd : pixel_format_mapping_bayer) {
    if (fd.pfnc_name == format) {
      return true;
    }
  }
  return false;
}

#endif
