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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef NVMM_ENABLED
#  include "cuda_runtime.h"
#  include "gstpylondsnvmmbufferfactory.h"
#endif

#include "gst/pylon/gstpyloncache.h"
#include "gst/pylon/gstpylondebug.h"
#include "gst/pylon/gstpylonformatmapping.h"
#include "gst/pylon/gstpylonincludes.h"
#include "gst/pylon/gstpylonmetaprivate.h"
#include "gst/pylon/gstpylonobject.h"
#include "gstpylon-private.h"
#include "gstpylon.h"
#include "gstpylondisconnecthandler.h"
#include "gstpylonimagehandler.h"
#include "gstpyloninspect.h"
#include "gstpylonschema.h"
#include "gstpylonsysmembufferfactory.h"

#include <exception>
#include <map>
#include <set>
#include <vector>

GstPylon* gst_pylon_new(GstElement* gstpylonsrc, const gchar* device_user_name,
                        const gchar* device_serial_number, gint device_index,
                        gboolean enable_correction, GError** err) {
  GstPylon* self = new GstPylon(gstpylonsrc);

  g_return_val_if_fail(self, NULL);
  g_return_val_if_fail(err && *err == NULL, NULL);

  try {
    self->Open(device_user_name, device_serial_number, device_index,
               enable_correction);
  } catch (const GenICam::GenericException& e) {
    GST_ERROR_OBJECT(gstpylonsrc, "gst_pylon_new failed opening camera: %s",
                     e.GetDescription());
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    delete self;
    self = NULL;
  } catch (const std::exception& e) {
    GST_ERROR_OBJECT(gstpylonsrc, "gst_pylon_new caught std::exception: %s",
                     e.what());
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.what());
    delete self;
    self = NULL;
  }

  return self;
}

gboolean gst_pylon_set_user_config(GstPylon* self, const gchar* user_set,
                                   GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->ApplyUserConfig(user_set, err);
}

gboolean gst_pylon_set_pfs_config(GstPylon* self, const gchar* pfs_location,
                                  GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->LoadPfsConfig(pfs_location, err);
}

gboolean gst_pylon_apply_session_config(GstPylon* self, const gchar* user_set,
                                        const gchar* pfs_location,
                                        GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->ApplySessionConfig(user_set, pfs_location, err);
}

gboolean gst_pylon_ensure_configured(GstPylon* self, const gchar* user_set,
                                     const gchar* pfs_location,
                                     gboolean enable_correction, GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->EnsureConfigured(user_set, pfs_location, enable_correction, err);
}

gboolean gst_pylon_get_startup_geometry(GstPylon* self, gint* start_width,
                                        gint* start_height) {
  g_return_val_if_fail(self, FALSE);
  return self->GetStartupGeometry(start_width, start_height);
}

void gst_pylon_free(GstPylon* self) {
  g_return_if_fail(self);
  delete self;
}

gboolean gst_pylon_start(GstPylon* self, GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->Start(err);
}

gboolean gst_pylon_stop(GstPylon* self, GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->Stop(err);
}

void gst_pylon_interrupt_capture(GstPylon* self) {
  g_return_if_fail(self);
  self->InterruptCapture();
}

void gst_pylon_clear_capture_interrupt(GstPylon* self) {
  g_return_if_fail(self);
  self->ClearCaptureInterrupt();
}

gboolean gst_pylon_capture(GstPylon* self, GstBuffer** buf,
                           GstPylonCaptureErrorEnum capture_error,
                           GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->Capture(buf, capture_error, err);
}

gboolean gst_pylon_set_configuration(GstPylon* self, const GstCaps* conf,
                                     GError** err) {
  g_return_val_if_fail(self, FALSE);
  return self->SetConfiguration(conf, err);
}

gchar* gst_pylon_camera_get_string_properties() {
  return gst_pylon_inspect_camera_properties();
}

gchar* gst_pylon_stream_grabber_get_string_properties() {
  return gst_pylon_inspect_stream_properties();
}

GObject* gst_pylon_get_camera(GstPylon* self) {
  g_return_val_if_fail(self, NULL);
  return self->RefCamera();
}

GObject* gst_pylon_get_stream_grabber(GstPylon* self) {
  g_return_val_if_fail(self, NULL);
  return self->RefStreamGrabber();
}

gboolean gst_pylon_is_same_device(GstPylon* self, const gint device_index,
                                  const gchar* device_user_name,
                                  const gchar* device_serial_number) {
  g_return_val_if_fail(self, FALSE);
  return self->MatchesRequestedDevice(device_index, device_user_name,
                                      device_serial_number);
}

gboolean gst_pylon_is_config_applied(GstPylon* self, const gchar* user_set,
                                     const gchar* pfs_location,
                                     gboolean enable_correction) {
  g_return_val_if_fail(self, FALSE);
  return self->IsConfigApplied(user_set, pfs_location, enable_correction);
}

void gst_pylon_set_enable_correction(GstPylon* self,
                                     gboolean enable_correction) {
  g_return_if_fail(self);
  self->SetEnableCorrection(enable_correction);
}

#ifdef NVMM_ENABLED
void gst_pylon_set_nvsurface_layout(
    GstPylon* self, const GstPylonNvsurfaceLayoutEnum nvsurface_layout) {
  g_return_if_fail(self);

  self->nvsurface_layout = nvsurface_layout;
}

GstPylonNvsurfaceLayoutEnum gst_pylon_get_nvsurface_layout(GstPylon* self) {
  g_return_val_if_fail(self, PROP_NVSURFACE_LAYOUT_DEFAULT);

  return self->nvsurface_layout;
}

void gst_pylon_set_gpu_id(GstPylon* self, const gint gpu_id) {
  g_return_if_fail(self);

  self->gpu_id = gpu_id;
}

guint gst_pylon_get_gpu_id(GstPylon* self) {
  g_return_val_if_fail(self, PROP_GPU_ID_DEFAULT);

  return self->gpu_id;
}
#endif
