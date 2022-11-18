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

#include "gstpylonmeta.h"

#include <gst/video/video.h>

static gboolean gst_pylon_meta_init(GstMeta *meta, gpointer params,
                                    GstBuffer *buffer);
static void gst_pylon_meta_free(GstMeta *meta, GstBuffer *buffer);

GType gst_pylon_meta_api_get_type(void) {
  static volatile GType type = 0;
  static const gchar *tags[] = {GST_META_TAG_VIDEO_STR, NULL};

  if (g_once_init_enter(&type)) {
    GType _type = gst_meta_api_type_register("GstPylonMetaAPI", tags);
    g_once_init_leave(&type, _type);
  }
  return type;
}

const GstMetaInfo *gst_pylon_meta_get_info(void) {
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter(&info)) {
    const GstMetaInfo *meta = gst_meta_register(
        GST_PYLON_META_API_TYPE, "GstPylonMeta", sizeof(GstPylonMeta),
        gst_pylon_meta_init, gst_pylon_meta_free, NULL);
    g_once_init_leave(&info, meta);
  }
  return info;
}

GstPylonMeta *gst_buffer_add_pylon_meta(
    GstBuffer *buffer,
    const Pylon::CBaslerUniversalGrabResultPtr &grab_result_ptr) {
  g_return_val_if_fail(buffer != NULL, NULL);

  GST_LOG("Adding Pylon chunk meta to buffer %p", buffer);

  GstPylonMeta *meta =
      (GstPylonMeta *)gst_buffer_add_meta(buffer, GST_PYLON_META_INFO, NULL);

  /* Add meta to GstPylonMeta */
  meta->block_id = grab_result_ptr->GetBlockID();
  meta->offset.offset_x = grab_result_ptr->GetOffsetX();
  meta->offset.offset_y = grab_result_ptr->GetOffsetY();
  meta->timestamp = grab_result_ptr->GetTimeStamp();
  grab_result_ptr->GetStride(meta->stride);

  GstCaps *ref = gst_caps_from_string("timestamp/x-pylon");
  gst_buffer_add_reference_timestamp_meta(
      buffer, ref, grab_result_ptr->GetTimeStamp(), GST_CLOCK_TIME_NONE);

  return meta;
}

static gboolean gst_pylon_meta_init(GstMeta *meta, gpointer params,
                                    GstBuffer *buffer) {
  GstPylonMeta *pylon_meta = (GstPylonMeta *)meta;

  pylon_meta->chunks = gst_structure_new_empty("meta/x-pylon");

  return TRUE;
}

static void gst_pylon_meta_free(GstMeta *meta, GstBuffer *buffer) {
  GstPylonMeta *pylon_meta = (GstPylonMeta *)meta;

  gst_structure_free(pylon_meta->chunks);
}
