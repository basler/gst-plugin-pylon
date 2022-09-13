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

#include "gstpylonimagehandler.h"

GstPylonImageHandler::GstPylonImageHandler() {
  this->ptr_grab_result = NULL;
  this->grab_result_ready = false;
}

void GstPylonImageHandler::OnImageGrabbed(
    Pylon::CBaslerUniversalInstantCamera &camera,
    const Pylon::CBaslerUniversalGrabResultPtr &grab_result) {
  std::unique_lock<std::mutex> mutex_lock(this->grab_result_mutex);
  this->ptr_grab_result = new Pylon::CBaslerUniversalGrabResultPtr(grab_result);
  this->grab_result_ready = true;
  mutex_lock.unlock();
  this->grab_result_cv.notify_one();
}

Pylon::CBaslerUniversalGrabResultPtr *GstPylonImageHandler::WaitForImage() {
  std::unique_lock<std::mutex> mutex_lock(this->grab_result_mutex);
  this->grab_result_cv.wait(mutex_lock,
                            [this] { return this->grab_result_ready; });
  Pylon::CBaslerUniversalGrabResultPtr *grab_result = this->ptr_grab_result;
  this->ptr_grab_result = NULL;
  this->grab_result_ready = false;
  mutex_lock.unlock();

  return grab_result;
};

void GstPylonImageHandler::InterruptWaitForImage() {
  std::unique_lock<std::mutex> mutex_lock(this->grab_result_mutex);
  this->grab_result_ready = true;
  mutex_lock.unlock();

  this->grab_result_cv.notify_one();
}
