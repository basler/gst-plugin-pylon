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

#include "gstpylonbufferpool.h"

#include <gst/pylon/gstpylondebug.h>
#include <gst/video/video.h>

typedef struct _GstPylonBufferPool {
  GstBufferPool base;

  GstAllocator *allocator;
  guint size;

} GstPylonBufferPool;

G_DEFINE_TYPE_WITH_CODE(GstPylonBufferPool, gst_pylon_buffer_pool,
                        GST_TYPE_BUFFER_POOL, gst_pylon_debug_init());

/* prototypes */
static gboolean gst_pylon_buffer_pool_set_config(GstBufferPool *pool,
                                                 GstStructure *config);
static void gst_pylon_buffer_pool_finalize(GObject *object);

static void gst_pylon_buffer_pool_class_init(GstPylonBufferPoolClass *klass) {
  GObjectClass *o_class = G_OBJECT_CLASS(klass);
  GstBufferPoolClass *bp_class = GST_BUFFER_POOL_CLASS(klass);

  o_class->finalize = gst_pylon_buffer_pool_finalize;

  bp_class->set_config = GST_DEBUG_FUNCPTR(gst_pylon_buffer_pool_set_config);
}

static void gst_pylon_buffer_pool_init(GstPylonBufferPool *self) {
  GST_INFO_OBJECT(self, "New Pylon buffer pool");
}

static gboolean gst_pylon_buffer_pool_set_config(GstBufferPool *pool,
                                                 GstStructure *config) {
  GstPylonBufferPool *self = GST_PYLON_BUFFER_POOL(pool);
  GstAllocator *allocator = NULL;
  GstCaps *caps = NULL;
  guint min_buffers = 0;
  guint max_buffers = 0;
  guint size = 0;

  if (!gst_buffer_pool_config_get_params(config, &caps, &size, &min_buffers,
                                         &max_buffers)) {
    GST_ERROR_OBJECT(self, "Error getting parameters from buffer pool config");
    goto error;
  }

  if (NULL == caps) {
    GST_ERROR_OBJECT(self, "Requested buffer pool configuration without caps");
    goto error;
  }

  gst_buffer_pool_config_get_allocator(config, &allocator, NULL);
  if (NULL == allocator) {
    allocator = gst_allocator_find(NULL);
    gst_buffer_pool_config_set_allocator(config, GST_ALLOCATOR(allocator),
                                         NULL);
  } else {
    g_object_ref(allocator);
  }

  if (self->allocator) {
    g_clear_object(&self->allocator);
  }
  self->allocator = allocator;
  self->size = size;

  GST_DEBUG_OBJECT(
      self, "Setting Pylon pool configuration with caps %" GST_PTR_FORMAT,
      caps);

  return GST_BUFFER_POOL_CLASS(gst_pylon_buffer_pool_parent_class)
      ->set_config(pool, config);

error:
  return FALSE;
}

static void gst_pylon_buffer_pool_finalize(GObject *object) {
  GstPylonBufferPool *self = GST_PYLON_BUFFER_POOL(object);

  GST_DEBUG_OBJECT(self, "Finalizing Pylon buffer pool");

  g_clear_object(&self->allocator);

  G_OBJECT_CLASS(gst_pylon_buffer_pool_parent_class)->finalize(object);
}
