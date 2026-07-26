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

#include "gstpyloncache.h"

#include <errno.h>
#include <glib/gfileutils.h>
#include <glib/gstdio.h>
#include <gst/pylon/gstpylonincludes.h>

#define DIRERR -1

/* prototypes */
static std::string gst_pylon_cache_create_filepath(
    const std::string& cache_filename);

static std::string gst_pylon_cache_create_filepath(
    const std::string& cache_filename) {
  gchar* filename_hash =
      g_compute_checksum_for_string(G_CHECKSUM_SHA256, cache_filename.c_str(),
                                    strlen(cache_filename.c_str()));
  std::string filename_hash_str = filename_hash;
  g_free(filename_hash);

  std::string dirpath = std::string(g_get_user_cache_dir()) + "/" + "gstpylon";

  /* Create gstpylon directory */
  gint dir_permissions = 0700;
  gint ret = g_mkdir_with_parents(dirpath.c_str(), dir_permissions);
  std::string filepath = dirpath + "/" + filename_hash_str + ".config";
  if (DIRERR == ret) {
    std::string msg =
        "Failed to create " + dirpath + ": " + std::string(strerror(errno));
    GST_WARNING("%s", msg.c_str());
  }

  return filepath;
}

static std::string gst_pylon_cache_introspection_filepath(
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

static constexpr const char* GST_PYLON_INTROSPECTION_CACHE_MAGIC =
    "gstpylon-introspection-cache-v1\n";
static constexpr const char* GST_PYLON_INTROSPECTION_CACHE_LENGTH =
    "content-length=";

gchar* GstPylonCache::GetIntrospection(const std::string& schema_key) {
  std::string filepath = gst_pylon_cache_introspection_filepath(schema_key);
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

void GstPylonCache::SetIntrospection(const std::string& schema_key,
                                     const std::string& content) {
  if (content.empty()) {
    GST_WARNING("Refusing to write empty introspection cache for %s",
                schema_key.c_str());
    return;
  }

  std::string filepath = gst_pylon_cache_introspection_filepath(schema_key);
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

GstPylonCache::GstPylonCache(const std::string& name,
                             gboolean enable_limit_probe)
    : filepath(gst_pylon_cache_create_filepath(name)),
      feature_cache_dict(g_key_file_new()),
      is_modified(FALSE),
      enable_limit_probe(enable_limit_probe) {
  /* load initial cache file */
  if (!LoadCacheFile()) {
    GST_LOG("No feature cache file found");
  }
}

GstPylonCache::~GstPylonCache() { g_key_file_free(this->feature_cache_dict); }

gboolean GstPylonCache::IsLimitProbeEnabled() const {
  const gchar* env = g_getenv("GST_PYLON_PROBE_LIMITS");
  if (env) {
    if (g_strcmp0(env, "0") == 0 || g_ascii_strcasecmp(env, "false") == 0) {
      return FALSE;
    }
    if (g_strcmp0(env, "1") == 0 || g_ascii_strcasecmp(env, "true") == 0) {
      return TRUE;
    }
  }

  return enable_limit_probe;
}

gboolean GstPylonCache::LoadCacheFile() {
  gboolean ret = TRUE;

  /* Check if file exists */
  ret = g_file_test(this->filepath.c_str(), G_FILE_TEST_EXISTS);
  if (!ret) {
    return FALSE;
  }

  /* Check if file contents are valid, this also sets the content of the
   * GKeyFile object if valid */
  ret = g_key_file_load_from_file(
      this->feature_cache_dict, this->filepath.c_str(), G_KEY_FILE_NONE, NULL);
  if (!ret) {
    return FALSE;
  }
  return TRUE;
}

gboolean GstPylonCache::HasNewSettings() { return is_modified; }

void GstPylonCache::CreateCacheFile() {
  GError* file_err = NULL;

#if defined(GLIB_VERSION_2_66) && GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_66
  gchar* contents = NULL;
  gsize length = 0;

  contents = g_key_file_to_data(this->feature_cache_dict, &length, NULL);
  g_assert(contents != NULL);

  gboolean ret = g_file_set_contents_full(
      this->filepath.c_str(), contents, length,
      static_cast<GFileSetContentsFlags>(G_FILE_SET_CONTENTS_CONSISTENT), 0600,
      &file_err);

  g_free(contents);
#else
  gboolean ret = g_key_file_save_to_file(this->feature_cache_dict,
                                         this->filepath.c_str(), &file_err);
#endif

  if (!ret) {
    std::string file_err_str = file_err->message;
    g_error_free(file_err);
    throw Pylon::GenericException(file_err_str.c_str(), __FILE__, __LINE__);
  }

  if (g_chmod(this->filepath.c_str(), 0600) != 0) {
    GST_WARNING("Failed to set permissions on cache file %s: %s",
                this->filepath.c_str(), strerror(errno));
  }
}

void GstPylonCache::SetIntegerAttribute(const char* feature,
                                        const char* attribute,
                                        const gint64 val) {
  g_key_file_set_int64(this->feature_cache_dict, feature, attribute, val);
  is_modified = true;
}

void GstPylonCache::SetDoubleAttribute(const char* feature,
                                       const char* attribute,
                                       const gdouble val) {
  g_key_file_set_double(this->feature_cache_dict, feature, attribute, val);
  is_modified = true;
}

bool GstPylonCache::GetIntegerAttribute(const char* feature,
                                        const char* attribute, gint64& val) {
  GError* err = NULL;

  gint64 value =
      g_key_file_get_int64(this->feature_cache_dict, feature, attribute, &err);
  if (err) {
    GST_WARNING("Could not read values for feature %s from file %s: %s",
                feature, this->filepath.c_str(), err->message);
    g_error_free(err);
    return false;
  }
  val = value;
  return true;
}

bool GstPylonCache::GetDoubleAttribute(const char* feature,
                                       const char* attribute, gdouble& val) {
  GError* err = NULL;

  gdouble value =
      g_key_file_get_double(this->feature_cache_dict, feature, attribute, &err);
  if (err) {
    GST_WARNING("Could not read values for feature %s from file %s: %s",
                feature, this->filepath.c_str(), err->message);
    g_error_free(err);
    return false;
  }
  val = value;
  return true;
}

void GstPylonCache::SetIntProps(const gchar* feature_name, const gint64 min,
                                const gint64 max, const GParamFlags flags) {
  SetIntegerAttribute(feature_name, "min", min);
  SetIntegerAttribute(feature_name, "max", max);
  SetIntegerAttribute(feature_name, "flags", static_cast<gint64>(flags));
}
void GstPylonCache::SetDoubleProps(const gchar* feature_name, const gdouble min,
                                   const gdouble max, const GParamFlags flags) {
  SetDoubleAttribute(feature_name, "min", min);
  SetDoubleAttribute(feature_name, "max", max);
  SetIntegerAttribute(feature_name, "flags", static_cast<gint64>(flags));
}

bool GstPylonCache::GetIntProps(const gchar* feature_name, gint64& min,
                                gint64& max, GParamFlags& flags) {
  if (!GetIntegerAttribute(feature_name, "min", min)) return false;
  if (!GetIntegerAttribute(feature_name, "max", max)) return false;
  gint64 flag_val = 0;
  if (!GetIntegerAttribute(feature_name, "flags", flag_val)) return false;

  static constexpr gint64 kMaxSaneDimension = 32768;
  if ((g_str_has_suffix(feature_name, "Width") ||
       g_str_has_suffix(feature_name, "Height") ||
       g_strcmp0(feature_name, "Width") == 0 ||
       g_strcmp0(feature_name, "Height") == 0) &&
      (max > kMaxSaneDimension || min < 0 || min > max)) {
    GST_WARNING("Ignoring invalid cache entry for %s (min=%" G_GINT64_FORMAT
                " max=%" G_GINT64_FORMAT ")",
                feature_name, min, max);
    return false;
  }

  flags = static_cast<GParamFlags>(flag_val);

  return true;
}

bool GstPylonCache::GetDoubleProps(const char* feature_name, gdouble& min,
                                   gdouble& max, GParamFlags& flags) {
  if (!GetDoubleAttribute(feature_name, "min", min)) return false;
  if (!GetDoubleAttribute(feature_name, "max", max)) return false;

  gint64 flag_val = 0;
  if (!GetIntegerAttribute(feature_name, "flags", flag_val)) return false;

  flags = static_cast<GParamFlags>(flag_val);

  return true;
}

bool GstPylonCache::GetFlags(const gchar* feature_name, GParamFlags& flags) {
  gint64 flag_val = 0;
  if (!GetIntegerAttribute(feature_name, "flags", flag_val)) return false;
  flags = static_cast<GParamFlags>(flag_val);
  return true;
}

void GstPylonCache::SetFlags(const gchar* feature_name,
                             const GParamFlags flags) {
  SetIntegerAttribute(feature_name, "flags", static_cast<gint64>(flags));
}
