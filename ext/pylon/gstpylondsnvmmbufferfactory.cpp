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

#include <cuda_runtime.h>
#include <gst/video/video.h>

#include <map>

static std::map<GstVideoFormat, NvBufSurfaceColorFormat> gst_to_nvbuf_format = {
    {GST_VIDEO_FORMAT_GRAY8, NVBUF_COLOR_FORMAT_GRAY8},
    {GST_VIDEO_FORMAT_RGB, NVBUF_COLOR_FORMAT_RGB},
    {GST_VIDEO_FORMAT_BGR, NVBUF_COLOR_FORMAT_BGR},
    {GST_VIDEO_FORMAT_UYVY, NVBUF_COLOR_FORMAT_UYVY},
    {GST_VIDEO_FORMAT_YUY2, NVBUF_COLOR_FORMAT_YUYV},
};

void GstPylonDsNvmmBufferFactory::SetConfig(const GstCaps *caps) {
  GstVideoInfo video_info = {};

  gst_video_info_from_caps(&video_info, caps);

  create_params.params.gpuId = 0;
  create_params.params.width = video_info.width;
  create_params.params.height = video_info.height;
  create_params.params.colorFormat =
      gst_to_nvbuf_format[video_info.finfo->format];
  create_params.params.isContiguous = false;
  create_params.params.layout = NVBUF_LAYOUT_PITCH;
  create_params.params.memType = NVBUF_MEM_DEFAULT;

  create_params.memtag = NvBufSurfaceTag_CAMERA;
}

void GstPylonDsNvmmBufferFactory::AllocateBuffer(size_t buffer_size,
                                                 void **p_created_buffer,
                                                 intptr_t &buffer_context) {
  void *buffer_mem = nullptr;

  cudaError_t err = cudaMallocHost(&buffer_mem, buffer_size);
  if (err != cudaSuccess) {
    std::cerr << "CUDA error allocating memory" << std::endl;
    return;
  }

  create_params.params.size = buffer_size;

  int batch_size = 1;
  NvBufSurface *surf = nullptr;

  NvBufSurfaceAllocate(&surf, batch_size, &create_params);
  surf->numFilled = 1;

  NvBufSurfaceMap(surf, -1, -1, NVBUF_MAP_READ_WRITE);

  *p_created_buffer = buffer_mem;
  buffer_context = reinterpret_cast<intptr_t>(surf);
}

void GstPylonDsNvmmBufferFactory::FreeBuffer(void *p_created_buffer,
                                             intptr_t buffer_context) {
  NvBufSurface *surf = reinterpret_cast<NvBufSurface *>(buffer_context);

  NvBufSurfaceDestroy(surf);
  cudaFreeHost(p_created_buffer);
}

void GstPylonDsNvmmBufferFactory::DestroyBufferFactory() { delete this; }
