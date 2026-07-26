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

#include "bindpylonmeta.h"

#include <gst/pylon/gstpylonmeta.h>

#include <memory>

namespace py = pybind11;
using namespace pybind11::literals;

namespace pygstpylon {

struct GstPylonMetaSnapshot {
  guint64 block_id = 0;
  guint64 image_number = 0;
  guint64 skipped_images = 0;
  guint64 offset_x = 0;
  guint64 offset_y = 0;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  gsize stride = 0;
  py::dict chunks;
};

static py::dict gst_pylon_chunks_to_dict(GstStructure* chunks) {
  py::dict dict;
  if (!chunks) {
    return dict;
  }

  for (int idx = 0; idx < gst_structure_n_fields(chunks); idx++) {
    const gchar* chunk_name = gst_structure_nth_field_name(chunks, idx);
    GType chunk_type = gst_structure_get_field_type(chunks, chunk_name);
    switch (chunk_type) {
      case G_TYPE_INT64: {
        gint64 int_chunk = 0;
        gst_structure_get_int64(chunks, chunk_name, &int_chunk);
        dict[py::str{std::string(chunk_name)}] = int_chunk;
        break;
      }
      case G_TYPE_DOUBLE: {
        gdouble double_chunk = 0;
        gst_structure_get_double(chunks, chunk_name, &double_chunk);
        dict[py::str{std::string(chunk_name)}] = double_chunk;
        break;
      }
      case G_TYPE_BOOLEAN: {
        gboolean bool_chunk = FALSE;
        gst_structure_get_boolean(chunks, chunk_name, &bool_chunk);
        dict[py::str{std::string(chunk_name)}] = static_cast<bool>(bool_chunk);
        break;
      }
      case G_TYPE_STRING: {
        const gchar* string_chunk =
            gst_structure_get_string(chunks, chunk_name);
        if (string_chunk) {
          dict[py::str{std::string(chunk_name)}] = std::string(string_chunk);
        }
        break;
      }
      default:
        break;
    }
  }

  return dict;
}

void bindpylonmeta(py::module& m) {
  py::class_<GstPylonMetaSnapshot>(m, "GstPylonMeta")
      .def_readonly("block_id", &GstPylonMetaSnapshot::block_id)
      .def_readonly("image_number", &GstPylonMetaSnapshot::image_number)
      .def_readonly("skipped_images", &GstPylonMetaSnapshot::skipped_images)
      .def_readonly("timestamp", &GstPylonMetaSnapshot::timestamp)
      .def_readonly("stride", &GstPylonMetaSnapshot::stride)
      .def_readonly("offset_x", &GstPylonMetaSnapshot::offset_x)
      .def_readonly("offset_y", &GstPylonMetaSnapshot::offset_y)
      .def_readonly("chunks", &GstPylonMetaSnapshot::chunks);

  m.def(
      "gst_buffer_get_pylon_meta",
      [](size_t gst_buffer) -> std::unique_ptr<GstPylonMetaSnapshot> {
        auto* buffer = reinterpret_cast<GstBuffer*>(gst_buffer);
        GstPylonMeta* meta = gst_buffer_get_pylon_meta(buffer);
        if (!meta) {
          return nullptr;
        }

        auto snap = std::make_unique<GstPylonMetaSnapshot>();
        snap->block_id = meta->block_id;
        snap->image_number = meta->image_number;
        snap->skipped_images = meta->skipped_images;
        snap->offset_x = meta->offset.offset_x;
        snap->offset_y = meta->offset.offset_y;
        snap->timestamp = meta->timestamp;
        snap->stride = meta->stride;
        snap->chunks = gst_pylon_chunks_to_dict(meta->chunks);
        return snap;
      },
      "buffer"_a,
      "Return a Python-owned copy of Pylon meta for the buffer pointer "
      "(typically hash(buf)). Safe to use after the buffer is released.");
}

}  // namespace pygstpylon
