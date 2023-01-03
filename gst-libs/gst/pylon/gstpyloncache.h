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

#ifndef _GST_PYLON_CACHE_H_
#define _GST_PYLON_CACHE_H_

#include <gst/gst.h>

#include <string>

#define MIN_VALUE_INDEX 0
#define MAX_VALUE_INDEX 1
#define FLAGS_VALUE_INDEX 2
#define NUMERATOR_INDEX 0
#define DENOMINATOR_INDEX 1

class GstPylonCache {
 public:
  GstPylonCache(const std::string &name);
  ~GstPylonCache();
  gboolean IsCacheValid();
  gboolean IsCacheNew();
  void SetCacheValue(const std::string &key, const std::string &value);
  void CreateCacheFile();

  template <class T>
  void GetKeyValues(std::string key, T &min, T &max, GParamFlags &flags);

 private:
  std::string filepath;
  GKeyFile *feature_cache_dict;
  std::string keyfile_groupname;
  gboolean is_cache_new;
};

template <class T>
void GstPylonCache::GetKeyValues(std::string key, T &min, T &max,
                                 GParamFlags &flags) {
  GError *err;

  gchar *values_str =
      g_key_file_get_string(this->feature_cache_dict,
                            this->keyfile_groupname.c_str(), key.c_str(), &err);
  if (!values_str) {
    GST_ERROR("Could not read values for feature %s from file %s: %s",
              key.c_str(), this->filepath.c_str(), err->message);
    g_error_free(err);
    return;
  }

  gchar **values_list = g_strsplit(values_str, " ", -1);
  gchar **min_limit = g_strsplit(values_list[MIN_VALUE_INDEX], ",", -1);
  gchar **max_limit = g_strsplit(values_list[MAX_VALUE_INDEX], ",", -1);

  min = (std::stod(min_limit[NUMERATOR_INDEX]) /
         std::stod(min_limit[DENOMINATOR_INDEX]));
  max = (std::stod(max_limit[NUMERATOR_INDEX]) /
         std::stod(max_limit[DENOMINATOR_INDEX]));
  flags = static_cast<GParamFlags>(std::stoi(values_list[FLAGS_VALUE_INDEX]));

  g_strfreev(min_limit);
  g_strfreev(max_limit);
  g_strfreev(values_list);
  g_free(values_str);
}

#endif
