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

#include "gstpylondsnvmmbufferfactory.h"

#if defined(__GNUC__)
#  include <stdlib.h>
#  include <unistd.h>
#endif

#include <cuda_runtime.h>
#include <gst/video/video.h>

#include <map>
#include <stdexcept>

static std::map<GstVideoFormat, NvBufSurfaceColorFormat> gst_to_nvbuf_format = {
    {GST_VIDEO_FORMAT_GRAY8, NVBUF_COLOR_FORMAT_GRAY8},
    {GST_VIDEO_FORMAT_RGB, NVBUF_COLOR_FORMAT_RGB},
    {GST_VIDEO_FORMAT_BGR, NVBUF_COLOR_FORMAT_BGR},
    {GST_VIDEO_FORMAT_UYVY, NVBUF_COLOR_FORMAT_UYVY},
    {GST_VIDEO_FORMAT_YUY2, NVBUF_COLOR_FORMAT_YUYV},
};

GstPylonDsNvmmBufferFactory::GstPylonDsNvmmBufferFactory(
    const GstPylonNvsurfaceLayoutEnum nvsurface_layout, const guint gpu_id) {
  this->gpu_id = gpu_id;

  switch (nvsurface_layout) {
    case ENUM_BLOCK_LINEAR:
      this->layout = NVBUF_LAYOUT_BLOCK_LINEAR;
      break;
    case ENUM_PITCH:
    default:
      this->layout = NVBUF_LAYOUT_PITCH;
      break;
  }
}

void GstPylonDsNvmmBufferFactory::SetConfig(const GstCaps* caps) {
  GstVideoInfo video_info = {};

  gst_video_info_from_caps(&video_info, caps);

  auto format_it = gst_to_nvbuf_format.find(video_info.finfo->format);
  if (format_it == gst_to_nvbuf_format.end()) {
    throw std::runtime_error("Unsupported GstVideoFormat for NVMM buffer");
  }

  create_params.params.gpuId = this->gpu_id;
  create_params.params.width = video_info.width;
  create_params.params.height = video_info.height;
  create_params.params.colorFormat = format_it->second;
  create_params.params.isContiguous = true;
  create_params.params.layout = this->layout;
  create_params.params.memType = NVBUF_MEM_DEFAULT;

  create_params.memtag = NvBufSurfaceTag_CAMERA;
}

void GstPylonDsNvmmBufferFactory::AllocateBuffer(size_t buffer_size,
                                                 void** p_created_buffer,
                                                 intptr_t& buffer_context) {
  void* buffer_mem = nullptr;

  const size_t PAGE_SIZE = getpagesize();
  const size_t aligned_buffer_size = RoundUp(buffer_size, PAGE_SIZE);

  int ret = posix_memalign(&buffer_mem, PAGE_SIZE, aligned_buffer_size);
  if (ret) buffer_mem = nullptr;

  create_params.params.size = aligned_buffer_size;

  /*
   * The optimal approach would be to us the nvsurface being allocated below
   *  this commend for pylon directly (as the *create_buffer output). However
   *  nvsurface has some restrictive stride requirements which pylon is unable
   *  to support for all cameras. The current approach means having the
   *  g_malloc memory block for pylon which then will be copied over to the
   *  nvsurface one after capture.
   */
  int batch_size = 1;
  NvBufSurface* surf = nullptr;

  int alloc_ret = NvBufSurfaceAllocate(&surf, batch_size, &create_params);
  if (alloc_ret != 0 || surf == nullptr) {
    free(buffer_mem);
    *p_created_buffer = nullptr;
    buffer_context = 0;
    throw std::runtime_error("NvBufSurfaceAllocate failed");
  }
  surf->numFilled = 1;

  if (NvBufSurfaceMap(surf, -1, -1, NVBUF_MAP_READ_WRITE) != 0) {
    NvBufSurfaceDestroy(surf);
    free(buffer_mem);
    *p_created_buffer = nullptr;
    buffer_context = 0;
    throw std::runtime_error("NvBufSurfaceMap failed");
  }

  *p_created_buffer = buffer_mem;
  buffer_context = reinterpret_cast<intptr_t>(surf);
}

void GstPylonDsNvmmBufferFactory::FreeBuffer(void* p_created_buffer,
                                             intptr_t buffer_context) {
  NvBufSurface* surf = reinterpret_cast<NvBufSurface*>(buffer_context);

  if (surf) {
    NvBufSurfaceUnMap(surf, -1, -1);
    NvBufSurfaceDestroy(surf);
  }
  free(p_created_buffer);
}

void GstPylonDsNvmmBufferFactory::DestroyBufferFactory() { delete this; }
