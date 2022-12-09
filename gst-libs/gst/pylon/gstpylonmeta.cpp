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
#include "config.h"
#endif

#include "gstpylondebug.h"
#include "gstpylonfeaturewalker.h"
#include "gstpylonmeta.h"
#include "gstpylonmetaprivate.h"

#include <gst/video/video.h>

#ifdef _MSC_VER  // MSVC
#pragma warning(push)
#pragma warning(disable : 4265)
#elif __GNUC__  // GCC, CLANG, MinGW
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/PylonIncludes.h>

#ifdef _MSC_VER  // MSVC
#pragma warning(pop)
#elif __GNUC__  // GCC, CLANG, MinWG
#pragma GCC diagnostic pop
#endif

/* prototypes */
static gboolean gst_pylon_meta_init(GstMeta *meta, gpointer params,
                                    GstBuffer *buffer);
static void gst_pylon_meta_free(GstMeta *meta, GstBuffer *buffer);
static void gst_pylon_meta_add_chunk_as_meta(GstStructure *st,
                                             GenApi::INode *node,
                                             GenApi::INode *selector_node,
                                             const guint64 &selector_value);
static void gst_pylon_meta_fill_result_chunks(
    GstPylonMeta *self,
    const Pylon::CBaslerUniversalGrabResultPtr &grab_result_ptr);

GType gst_pylon_meta_api_get_type(void) {
  static GType type = 0;
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

static void gst_pylon_meta_add_chunk_as_meta(GstStructure *st,
                                             GenApi::INode *node,
                                             GenApi::INode *selector_node,
                                             const guint64 &selector_value) {
  g_return_if_fail(st);
  g_return_if_fail(node);

  GValue value = G_VALUE_INIT;
  gboolean is_valid = TRUE;
  gchar *name = NULL;

  if (selector_node) {
    Pylon::CEnumParameter selparam(selector_node);
    selparam.SetIntValue(selector_value);
    name = g_strdup_printf(
        "%s-%s", node->GetName().c_str(),
        selparam.GetEntry(selector_value)->GetSymbolic().c_str());
  } else {
    name = g_strdup(node->GetName().c_str());
  }

  GenApi::EInterfaceType iface = node->GetPrincipalInterfaceType();
  switch (iface) {
    case GenApi::intfIInteger:
      g_value_init(&value, G_TYPE_INT64);
      g_value_set_int64(&value, Pylon::CIntegerParameter(node).GetValue());
      break;
    case GenApi::intfIBoolean:
      g_value_init(&value, G_TYPE_BOOLEAN);
      g_value_set_boolean(&value, Pylon::CBooleanParameter(node).GetValue());
      break;
    case GenApi::intfIFloat:
      g_value_init(&value, G_TYPE_DOUBLE);
      g_value_set_double(&value, Pylon::CFloatParameter(node).GetValue());
      break;
    case GenApi::intfIString:
      g_value_init(&value, G_TYPE_STRING);
      g_value_set_string(&value, Pylon::CStringParameter(node).GetValue());
      break;
    case GenApi::intfIEnumeration:
      g_value_init(&value, G_TYPE_STRING);
      g_value_set_string(&value,
                         Pylon::CEnumParameter(node).GetValue().c_str());
      break;
    default:
      is_valid = FALSE;
      GST_WARNING("Chunk %s not added. Chunk of type %d is not supported", name,
                  node->GetPrincipalInterfaceType());
      break;
  }

  if (is_valid) {
    gst_structure_set_value(st, name, &value);
  }

  g_value_unset(&value);
  g_free(name);
}

static void gst_pylon_meta_fill_result_chunks(
    GstPylonMeta *self,
    const Pylon::CBaslerUniversalGrabResultPtr &grab_result_ptr) {
  g_return_if_fail(self);

  GstStructure *st = self->chunks;

  GenApi::INodeMap &chunk_nodemap = grab_result_ptr->GetChunkDataNodeMap();
  GenApi::NodeList_t chunk_nodes;
  chunk_nodemap.GetNodes(chunk_nodes);

  for (auto &node : chunk_nodes) {
    GenApi::INode *selector_node = NULL;
    guint64 selector_value = 0;

    /* Only take into account valid Chunk nodes */
    auto sel_node = dynamic_cast<GenApi::ISelector *>(node);
    if (!GenApi::IsAvailable(node) || !node->IsFeature() ||
        (node->GetName() == "Root") || !sel_node || sel_node->IsSelector()) {
      continue;
    }

    std::vector<std::string> enum_values;
    try {
      enum_values = gst_pylon_process_selector_features(node, &selector_node);
    } catch (const Pylon::GenericException &e) {
      GST_WARNING("Chunk %s not added: %s", node->GetName().c_str(),
                  e.GetDescription());
      continue;
    }

    Pylon::CEnumParameter param(selector_node);

    /* If the number of selector values (stored in enum_values) is 1, leave
     * selector_node NULL, hence treating the feature as a "direct" one. */
    if (1 == enum_values.size()) {
      selector_node = NULL;
    }

    for (auto const &sel_pair : enum_values) {
      if (param.IsValid()) {
        selector_value = param.GetEntryByName(sel_pair.c_str())->GetValue();
      }
      gst_pylon_meta_add_chunk_as_meta(st, node, selector_node, selector_value);
    }
  }
}

void gst_buffer_add_pylon_meta(
    GstBuffer *buffer,
    const Pylon::CBaslerUniversalGrabResultPtr &grab_result_ptr) {
  g_return_if_fail(buffer != NULL);

  GST_LOG("Adding Pylon chunk meta to buffer %p", buffer);

  GstPylonMeta *self =
      (GstPylonMeta *)gst_buffer_add_meta(buffer, GST_PYLON_META_INFO, NULL);

  /* Add meta to GstPylonMeta */
  self->block_id = grab_result_ptr->GetImageNumber();
  self->image_number = grab_result_ptr->GetImageNumber();
  self->skipped_images = grab_result_ptr->GetNumberOfSkippedImages();
  self->offset.offset_x = grab_result_ptr->GetOffsetX();
  self->offset.offset_y = grab_result_ptr->GetOffsetY();
  self->timestamp = grab_result_ptr->GetTimeStamp();
  grab_result_ptr->GetStride(self->stride);

  if (grab_result_ptr->IsChunkDataAvailable()) {
    gst_pylon_meta_fill_result_chunks(self, grab_result_ptr);
  }
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
