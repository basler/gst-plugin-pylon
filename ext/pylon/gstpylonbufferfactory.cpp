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

#include "gstpylonbufferfactory.h"

struct bufferAndMap {
  GstBuffer *buffer;
  GstMapInfo info;
};

GstPylonBufferFactory::GstPylonBufferFactory(const GstCaps *caps) {
  gstDeleter d;

  this->caps = std::unique_ptr<GstCaps, gstDeleter>(gst_caps_copy(caps), d);
  this->pool = std::unique_ptr<GstPylonBufferPool, gstDeleter>(
      reinterpret_cast<GstPylonBufferPool *>(
          g_object_new(GST_TYPE_PYLON_BUFFER_POOL, NULL)),
      d);
}

GstPylonBufferFactory::~GstPylonBufferFactory() {}

void GstPylonBufferFactory::AllocateBuffer(size_t bufferSize,
                                           void **pCreatedBuffer,
                                           intptr_t &bufferContext) {
  if (FALSE == gst_buffer_pool_is_active(GST_BUFFER_POOL(this->pool.get()))) {
    GstStructure *st =
        gst_buffer_pool_get_config(GST_BUFFER_POOL(this->pool.get()));
    gst_buffer_pool_config_set_params(st, this->caps.get(), bufferSize, 1,
                                      max_buffers);

    gst_buffer_pool_set_config(GST_BUFFER_POOL(this->pool.get()), st);
    gst_buffer_pool_set_active(GST_BUFFER_POOL(this->pool.get()), true);
  }

  GstBuffer *buffer = nullptr;
  GstFlowReturn ret = gst_buffer_pool_acquire_buffer(
      GST_BUFFER_POOL(this->pool.get()), &buffer, NULL);

  if (GST_FLOW_OK != ret) {
    GST_WARNING_OBJECT(this->pool.get(), "Unable to acquire buffer from pool");
    return;
  }

  // Map the buffer to read its contents
  GstMapInfo info = GST_MAP_INFO_INIT;
  gst_buffer_map(buffer, &info,
                 GST_MAP_WRITE);  // Should be unmap at some point

  *pCreatedBuffer = info.data;

  bufferContext = reinterpret_cast<intptr_t>(new bufferAndMap{buffer, info});
}

void GstPylonBufferFactory::FreeBuffer(void *pCreatedBuffer,
                                       intptr_t bufferContext) {
  struct bufferAndMap *buffer_and_map =
      reinterpret_cast<struct bufferAndMap *>(bufferContext);

  gst_buffer_unmap(buffer_and_map->buffer, &buffer_and_map->info);
  gst_buffer_unref(buffer_and_map->buffer);

  delete buffer_and_map;
}

void GstPylonBufferFactory::DestroyBufferFactory() { delete this; }
