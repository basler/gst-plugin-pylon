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

#include "gstpylonsysmembufferfactory.h"

#if defined(__GNUC__)
#  include <stdlib.h>
#  include <unistd.h>
#endif

#include <atomic>
#include <new>

#ifdef GST_PYLON_ENABLE_TEST_HOOKS
namespace {
std::atomic<int> g_test_fail_on_alloc{0};
std::atomic<int> g_test_alloc_counter{0};
}  // namespace

void GstPylonSysMemBufferFactory::TestSetFailOnAlloc(int nth) {
  g_test_alloc_counter.store(0, std::memory_order_relaxed);
  g_test_fail_on_alloc.store(nth, std::memory_order_relaxed);
}
#endif

void GstPylonSysMemBufferFactory::AllocateBuffer(
    size_t buffer_size, void **p_created_buffer,
    intptr_t & /*buffer_context*/) {
#ifdef GST_PYLON_ENABLE_TEST_HOOKS
  const int fail_nth = g_test_fail_on_alloc.load(std::memory_order_relaxed);
  if (fail_nth > 0) {
    const int n =
        g_test_alloc_counter.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == fail_nth) {
      *p_created_buffer = nullptr;
      throw std::bad_alloc();
    }
  }
#endif

#if defined(__GNUC__)
  const size_t PAGE_SIZE = getpagesize();
  const size_t aligned_buffer_size = RoundUp(buffer_size, PAGE_SIZE);
  void *mem = nullptr;
  int ret = posix_memalign(&mem, PAGE_SIZE, aligned_buffer_size);
  if (ret != 0 || mem == nullptr) {
    *p_created_buffer = nullptr;
    throw std::bad_alloc();
  }
  *p_created_buffer = mem;
#else
  void *mem = malloc(buffer_size);
  if (mem == nullptr) {
    *p_created_buffer = nullptr;
    throw std::bad_alloc();
  }
  *p_created_buffer = mem;
#endif
}

void GstPylonSysMemBufferFactory::FreeBuffer(void *p_created_buffer,
                                             intptr_t /* buffer_context */) {
  free(p_created_buffer);
}

void GstPylonSysMemBufferFactory::DestroyBufferFactory() {
  /* Owned by std::shared_ptr via Pylon::Cleanup_None; must not self-delete. */
  g_assert_not_reached();
}
