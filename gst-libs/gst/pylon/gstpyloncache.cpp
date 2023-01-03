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

#ifdef _MSC_VER  // MSVC
#pragma warning(push)
#pragma warning(disable : 4265)
#elif __GNUC__  // GCC, CLANG, MinGW
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <pylon/PylonIncludes.h>

#ifdef _MSC_VER  // MSVC
#pragma warning(pop)
#elif __GNUC__  // GCC, CLANG, MinWG
#pragma GCC diagnostic pop
#endif

#define DIRERR -1

/* prototypes */
static std::string gst_pylon_cache_create_filepath(
    const std::string& cache_filename);

static std::string gst_pylon_cache_create_filepath(
    const std::string& cache_filename) {
  gchar* filename_hash =
      g_compute_checksum_for_string(G_CHECKSUM_SHA256, cache_filename.c_str(),
                                    strlen(cache_filename.c_str()));
  std::string filename_hash_str = std::string(filename_hash);
  g_free(filename_hash);

  std::string dirpath = std::string(g_get_user_cache_dir()) + "/" + "gstpylon";

  /* Create gstpylon directory */
  gint dir_permissions = 0775;
  gint ret = g_mkdir_with_parents(dirpath.c_str(), dir_permissions);
  if (DIRERR == ret) {
    std::string msg = "Failed to create " + std::string(dirpath) + ": " +
                      std::string(strerror(errno));
    throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }

  std::string filepath = dirpath + "/" + filename_hash_str;

  return filepath;
}

GstPylonCache::GstPylonCache(const std::string& name)
    : cache_file_name(name), feature_cache_dict(g_key_file_new()) {}

GstPylonCache::~GstPylonCache() { g_key_file_free(this->feature_cache_dict); }

void GstPylonCache::SetCacheValue(const std::string& key,
                                  const std::string& value) {
  g_key_file_set_string(this->feature_cache_dict, "gstpylon", key.c_str(),
                        value.c_str());
}

void GstPylonCache::CreateCacheFile() {
  gsize len = 0;
  gchar* feature_cache =
      g_key_file_to_data(this->feature_cache_dict, &len, NULL);
  std::string feature_cache_str = std::string(feature_cache);
  g_free(feature_cache);

  std::string filepath =
      gst_pylon_cache_create_filepath(this->cache_file_name.c_str());

  GError* file_err = NULL;
  gboolean ret = g_file_set_contents(filepath.c_str(),
                                     feature_cache_str.c_str(), len, &file_err);
  if (!ret) {
    std::string file_err_str = std::string(file_err->message);
    g_error_free(file_err);
    throw Pylon::GenericException(file_err_str.c_str(), __FILE__, __LINE__);
  }
}
