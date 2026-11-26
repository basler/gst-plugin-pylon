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

#include "gstpyloninspectcache.h"

#include <errno.h>
#include <glib/gfileutils.h>
#include <glib/gstdio.h>
#include <gst/pylon/gstpylonincludes.h>

#include <cstring>

static constexpr const char* GST_PYLON_INTROSPECTION_CACHE_MAGIC =
    "gstpylon-introspection-cache-v1\n";
static constexpr const char* GST_PYLON_INTROSPECTION_CACHE_LENGTH =
    "content-length=";

static std::string gst_pylon_inspect_cache_filepath(
    const std::string& schema_key) {
  gchar* filename_hash =
      g_compute_checksum_for_string(G_CHECKSUM_SHA256, schema_key.c_str(),
                                    static_cast<gssize>(schema_key.size()));
  std::string dirpath = std::string(g_get_user_cache_dir()) + "/" + "gstpylon";
  g_mkdir_with_parents(dirpath.c_str(), 0700);
  std::string filepath = dirpath + "/" + filename_hash + ".introspection";
  g_free(filename_hash);
  return filepath;
}

gchar* GstPylonInspectCache::Get(const std::string& schema_key) {
  std::string filepath = gst_pylon_inspect_cache_filepath(schema_key);
  if (!g_file_test(filepath.c_str(), G_FILE_TEST_EXISTS)) {
    return NULL;
  }
  gchar* contents = NULL;
  gsize length = 0;
  if (!g_file_get_contents(filepath.c_str(), &contents, &length, NULL)) {
    return NULL;
  }

  const std::string serialized(contents, length);
  g_free(contents);

  const std::string magic = GST_PYLON_INTROSPECTION_CACHE_MAGIC;
  if (serialized.compare(0, magic.size(), magic) != 0) {
    GST_DEBUG("Ignoring stale introspection cache %s", filepath.c_str());
    return NULL;
  }

  const size_t length_start = magic.size();
  const size_t length_end = serialized.find('\n', length_start);
  if (length_end == std::string::npos) {
    GST_WARNING("Ignoring truncated introspection cache %s", filepath.c_str());
    return NULL;
  }

  const std::string length_line =
      serialized.substr(length_start, length_end - length_start);
  const std::string length_prefix = GST_PYLON_INTROSPECTION_CACHE_LENGTH;
  if (length_line.compare(0, length_prefix.size(), length_prefix) != 0) {
    GST_WARNING("Ignoring malformed introspection cache %s", filepath.c_str());
    return NULL;
  }

  gchar* endptr = NULL;
  const guint64 expected_length =
      g_ascii_strtoull(length_line.c_str() + length_prefix.size(), &endptr, 10);
  if (!endptr || *endptr != '\0' || expected_length == 0) {
    GST_WARNING("Ignoring invalid introspection cache length in %s",
                filepath.c_str());
    return NULL;
  }

  const size_t content_start = length_end + 2;
  if (length_end + 1 >= serialized.size() ||
      serialized[length_end + 1] != '\n' ||
      serialized.size() - content_start != expected_length) {
    GST_WARNING("Ignoring corrupt introspection cache %s", filepath.c_str());
    return NULL;
  }

  return g_strndup(serialized.data() + content_start, expected_length);
}

void GstPylonInspectCache::Set(const std::string& schema_key,
                               const std::string& content) {
  if (content.empty()) {
    GST_WARNING("Refusing to write empty introspection cache for %s",
                schema_key.c_str());
    return;
  }

  std::string filepath = gst_pylon_inspect_cache_filepath(schema_key);
  const std::string serialized =
      std::string(GST_PYLON_INTROSPECTION_CACHE_MAGIC) +
      GST_PYLON_INTROSPECTION_CACHE_LENGTH + std::to_string(content.size()) +
      "\n\n" + content;
  GError* err = NULL;
  if (!g_file_set_contents(filepath.c_str(), serialized.c_str(),
                           static_cast<gssize>(serialized.size()), &err)) {
    GST_WARNING("Could not write introspection cache to %s: %s",
                filepath.c_str(), err ? err->message : "unknown error");
    if (err) g_error_free(err);
  } else if (g_chmod(filepath.c_str(), 0600) != 0) {
    GST_WARNING("Failed to set permissions on introspection cache file %s: %s",
                filepath.c_str(), strerror(errno));
  }
}
