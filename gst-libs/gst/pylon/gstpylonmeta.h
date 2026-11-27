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

#ifndef __GST_PYLON_META_H__
#define __GST_PYLON_META_H__

#include <gst/gst.h>
#include <gst/pylon/gstpylon-prelude.h>

G_BEGIN_DECLS

#define GST_PYLON_META_API_TYPE (gst_pylon_meta_api_get_type())
#define GST_PYLON_META_INFO (gst_pylon_meta_get_info())
typedef struct _GstPylonOffset GstPylonOffset;
typedef struct _GstPylonMeta GstPylonMeta;

/**
 * @brief Region-of-interest offset reported by the camera for a grabbed frame.
 */
struct _GstPylonOffset {
  guint64 offset_x; /**< Horizontal offset in pixels. */
  guint64 offset_y; /**< Vertical offset in pixels. */
};

/**
 * @brief Per-frame Basler pylon metadata attached to a GstBuffer.
 *
 * Carries grab-result metadata (block ID, image number, skipped image count,
 * ROI offset, camera timestamp, stride) and optional GenICam chunk values.
 */
struct _GstPylonMeta {
  GstMeta meta; /**< Base GStreamer metadata header. */

  GstStructure *chunks;   /**< Optional GenICam chunk key/value structure. */
  guint64 block_id;       /**< Camera/transport-layer block identifier. */
  guint64 image_number;   /**< Sequential grabbed image number. */
  guint64 skipped_images; /**< Number of images skipped before this frame. */
  GstPylonOffset offset;  /**< Frame ROI X/Y offset in pixels. */
  GstClockTime timestamp; /**< Camera hardware timestamp. */
  gsize stride;           /**< Row stride in bytes (0 if unavailable). */
};

/**
 * @brief Returns the GType registered for the GstPylonMeta API.
 * @return The GType of the pylon metadata API.
 */
EXT_PYLONSRC_API GType gst_pylon_meta_api_get_type(void);

/**
 * @brief Returns the GstMetaInfo descriptor for GstPylonMeta.
 * @return Pointer to the static GstMetaInfo instance.
 */
EXT_PYLONSRC_API const GstMetaInfo *gst_pylon_meta_get_info(void);

/**
 * @brief Retrieves the GstPylonMeta attached to a GstBuffer, if present.
 * @param buffer The GstBuffer to inspect (must not be NULL).
 * @return Pointer to the buffer's GstPylonMeta, or NULL if none is attached.
 */
EXT_PYLONSRC_API GstPylonMeta *gst_buffer_get_pylon_meta(GstBuffer *buffer);

G_END_DECLS
#endif
