/* Copyright (C) 2022 Basler AG
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

#ifndef _GST_PYLON_IMAGE_HANDLER_H_
#define _GST_PYLON_IMAGE_HANDLER_H_

#include <gst/gstclock.h>
#include <gst/pylon/gstpylonincludes.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

enum class GstPylonImageHandlerResult {
  /* A grab result is available */
  ok,
  /* unlock/flush interrupted the wait */
  flushing,
  /* Camera was disconnected */
  disconnected,
  /* Timed out waiting for a frame */
  timeout,
};

struct GstPylonPendingFrame {
  Pylon::CBaslerUniversalGrabResultPtr grab;
  GstClockTime arrival_time = GST_CLOCK_TIME_NONE;
};

class GstPylonImageHandler : public Pylon::CBaslerUniversalImageEventHandler {
 public:
  using ClockCallback = std::function<GstClockTime()>;

  GstPylonImageHandler();
  ~GstPylonImageHandler() override;
  void OnImageGrabbed(
      Pylon::CBaslerUniversalInstantCamera& camera,
      const Pylon::CBaslerUniversalGrabResultPtr& grab_result) override;

  GstPylonImageHandlerResult WaitForImage(GstPylonPendingFrame& out_frame,
                                          uint32_t timeout_ms = 0);
  GstPylonImageHandlerResult WaitForImage(
      Pylon::CBaslerUniversalGrabResultPtr** grab_result,
      uint32_t timeout_ms = 0);

  /* Waits up to timeout_ms, returning false immediately if interrupted. */
  bool WaitInterruptible(uint32_t timeout_ms);
  bool IsInterrupted() const;

  void SetClockCallback(ClockCallback cb);
  void InterruptWaitForImage();
  void SignalDisconnect();
  void ResetForReconnect();
  void ClearInterrupt();
  void SetMaxPendingFrames(size_t max_pending_frames);
  size_t GetMaxPendingFrames();

#ifdef GST_PYLON_IMAGEHANDLER_ENABLE_TEST_HOOKS
  /* Test only: FrameReady with an empty queue to exercise WaitForImage recover.
   */
  void TestHookForceFrameReadyEmpty();
#endif

 private:
  enum class State { Idle, FrameReady, Interrupted, Disconnected };

  void ClearPendingFrames();

  mutable std::mutex grab_result_mutex;
  std::condition_variable grab_result_cv;
  std::deque<GstPylonPendingFrame> pending_frames;
  ClockCallback clock_cb;
  size_t max_pending_frames;
  State state;
};

#endif
