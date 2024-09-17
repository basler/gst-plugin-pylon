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

#ifndef GST_PYLON_SYSMEM_BUFFER_FACTORY_H
#define GST_PYLON_SYSMEM_BUFFER_FACTORY_H

#include <gst/gst.h>
#include <gst/pylon/gstpylonincludes.h>
#include <gstpylonbufferfactory.h>

class GstPylonSysMemBufferFactory : public GstPylonBufferFactory {
 public:
  GstPylonSysMemBufferFactory() = default;
  virtual void SetConfig(const GstCaps *caps) override{};
  virtual void AllocateBuffer(size_t buffer_size, void **p_created_buffer,
                              intptr_t &buffer_context) override;
  virtual void FreeBuffer(void *p_created_buffer,
                          intptr_t buffer_context) override;
  virtual void DestroyBufferFactory() override;

 private:
  size_t RoundUp(size_t N, size_t S) { return ((((N) + (S)-1) / (S)) * (S)); }
};

#endif
