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

#pragma once

#include "gstpylonbufferpool.h"

#include <gst/gst.h>
#include <pylon/PylonIncludes.h>

#include <memory>

struct gstDeleter {
  void operator()(GstCaps *caps) const { gst_caps_unref(caps); }
  void operator()(GstPylonBufferPool *pool) const { g_object_unref(pool); }
};

class GstPylonBufferFactory : public Pylon::IBufferFactory {
 public:
  GstPylonBufferFactory(const GstCaps *caps);
  virtual ~GstPylonBufferFactory();
  virtual void AllocateBuffer(size_t bufferSize, void **pCreatedBuffer,
                              intptr_t &bufferContext);
  virtual void FreeBuffer(void *pCreatedBuffer, intptr_t bufferContext);
  virtual void DestroyBufferFactory();

 protected:
  std::unique_ptr<GstCaps, gstDeleter> caps;
  std::unique_ptr<GstPylonBufferPool, gstDeleter> pool;

  guint max_buffers{32};  // TODO get this from the property
};