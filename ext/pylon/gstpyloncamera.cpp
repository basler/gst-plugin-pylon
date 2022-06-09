/* Copyright (C) 2022 Basler AG
 *
 * Created by RidgeRun, LLC <support@ridgerun.com>
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

#include "gstpyloncamera.h"

struct _GstPylonCamera {
  GObject parent;
};

static void gst_pylon_camera_class_init(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera) {
  std::cout << std::string(camera->GetDeviceInfo().GetFullName()) << std::endl;
}

static void gst_pylon_camera_init(GstPylonCamera* self) {}

gboolean gst_pylon_camera_register(
    const Pylon::CBaslerUniversalInstantCamera& camera) {
  GTypeInfo typeinfo = {
      sizeof(GstPylonCameraClass),
      NULL,
      NULL,
      (GClassInitFunc)gst_pylon_camera_class_init,
      NULL,
      &camera,
      sizeof(GstPylonCamera),
      0,
      (GInstanceInitFunc)gst_pylon_camera_init,
  };
  GType type;

  Pylon::CDeviceInfo cam_info = camera.GetDeviceInfo();
  type = g_type_from_name(cam_info.GetFullName().c_str());
  if (!type) {
    type = g_type_register_static(G_TYPE_OBJECT, cam_info.GetFullName().c_str(),
                                  &typeinfo, static_cast<GTypeFlags>(0));
  }

  return TRUE;
}
