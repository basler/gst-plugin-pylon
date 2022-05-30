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

#ifndef _GST_PYLON_H_
#define _GST_PYLON_H_

#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstPylon GstPylon;

void gst_pylon_initialize ();

GstPylon *gst_pylon_new (const gchar *device_user_name,
    const gchar *device_serial_number, gint device_index, GError ** err);
gboolean gst_pylon_set_user_config (GstPylon *self, const gchar * user_set, GError **err);
void gst_pylon_free (GstPylon * self);

gboolean gst_pylon_start (GstPylon * self, GError ** err);
gboolean gst_pylon_stop (GstPylon * self, GError ** err);
gboolean gst_pylon_capture (GstPylon * self, GstBuffer ** buf, GError ** err);
GstCaps *gst_pylon_query_configuration (GstPylon * self, GError ** err);
gboolean gst_pylon_set_configuration (GstPylon * self, const GstCaps *conf,
    GError ** err);

G_END_DECLS

#endif
