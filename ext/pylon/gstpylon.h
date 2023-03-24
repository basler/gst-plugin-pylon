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

#ifndef _GST_PYLON_H_
#define _GST_PYLON_H_

#include <glib.h>
#include <gst/gst.h>

typedef struct _GstPylon GstPylon;

typedef enum {
  ENUM_KEEP = 0,
  ENUM_SKIP = 1,
  ENUM_ABORT = 2,
} GstPylonCaptureErrorEnum;

void gst_pylon_initialize();

GstPylon *gst_pylon_new(GstElement *gstpylonsrc, const gchar *device_user_name,
                        const gchar *device_serial_number, gint device_index,
                        gboolean enable_correction, GError **err);
gboolean gst_pylon_set_user_config(GstPylon *self, const gchar *user_set,
                                   GError **err);
void gst_pylon_free(GstPylon *self);

gboolean gst_pylon_start(GstPylon *self, GError **err);
gboolean gst_pylon_stop(GstPylon *self, GError **err);
void gst_pylon_interrupt_capture(GstPylon *self);
gboolean gst_pylon_capture(GstPylon *self, GstBuffer **buf,
                           GstPylonCaptureErrorEnum capture_error,
                           GError **err);
GstCaps *gst_pylon_query_configuration(GstPylon *self, GError **err);
gboolean gst_pylon_get_startup_geometry(GstPylon *self, gint *start_width,
                                        gint *start_height);
gboolean gst_pylon_set_configuration(GstPylon *self, const GstCaps *conf,
                                     GError **err);
gboolean gst_pylon_set_pfs_config(GstPylon *self, const gchar *pfs_location,
                                  GError **err);
gchar *gst_pylon_camera_get_string_properties();
gchar *gst_pylon_stream_grabber_get_string_properties();

GObject *gst_pylon_get_camera(GstPylon *self);
GObject *gst_pylon_get_stream_grabber(GstPylon *self);

gboolean gst_pylon_is_same_device(GstPylon *self, const gint device_index,
                                  const gchar *device_user_name,
                                  const gchar *device_serial_number);

#endif
