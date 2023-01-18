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

#include "bindpylonmeta.h"

#include <gst/pylon/gstpylonmeta.h>

namespace py = pybind11;
namespace pygstpylon {

void bindpylonmeta(py::module &m) {
  py::class_<GstPylonMeta>(m, "GstPylonMeta")
      .def(py::init<>())
      .def_readonly("block_id", &GstPylonMeta::block_id)
      .def_readonly("image_number", &GstPylonMeta::image_number)
      .def_readonly("skipped_images", &GstPylonMeta::skipped_images)
      .def_readonly("timestamp", &GstPylonMeta::timestamp)
      .def_readonly("stride", &GstPylonMeta::stride)
      .def_property_readonly("chunks", [](const GstPylonMeta &self) {
        py::dict dict;
        gint64 int_chunk;
        gdouble double_chunk;
        /* export chunks embedded in the stream to dict*/
        for (int idx = 0; idx < gst_structure_n_fields(self.chunks); idx++) {
          const gchar *chunk_name =
              gst_structure_nth_field_name(self.chunks, idx);
          GType chunk_type =
              gst_structure_get_field_type(self.chunks, chunk_name);
          /* display double and int types */
          switch (chunk_type) {
            case G_TYPE_INT64:
              gst_structure_get_int64(self.chunks, chunk_name, &int_chunk);
              dict[py::str{std::string(chunk_name)}] = int_chunk;
              break;
            case G_TYPE_DOUBLE:
              gst_structure_get_double(self.chunks, chunk_name, &double_chunk);
              dict[py::str{std::string(chunk_name)}] = double_chunk;
              break;
            default:
              g_print("Skip chunk %s\n", chunk_name);
          }
        }
        return dict;
      });
}

}  // namespace pygstpylon
