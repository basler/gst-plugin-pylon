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

class GstPylonCache {
 public:
  GstPylonCache(const std::string &name);
  ~GstPylonCache();
  gboolean IsCacheValid();
  gboolean IsEmpty();

  void SetIntProps(const std::string &key, const gint64 min,const gint64 max,const GParamFlags flags);
  void SetDoubleProps(const std::string &key, const gdouble min,const gdouble max,const GParamFlags flags);

  bool GetIntProps(const std::string &key, gint64 &min,gint64 &max,GParamFlags &flags);
  bool GetDoubleProps(const std::string &key, gdouble &min,gdouble &max,GParamFlags &flags);

  /* persist cache to filesystem */
  void CreateCacheFile();

 private:

  void SetIntegerAttribute(const char *feature, const char* attribute, const gint64 val);
  void SetDoubleAttribute(const char *feature, const char* attribute, gdouble val);

  bool GetIntegerAttribute(const char *feature, const char* attribute, gint64 &val);
  bool GetDoubleAttribute(const char *feature, const char* attribute, gdouble &val);


  std::string filepath;
  GKeyFile *feature_cache_dict;
  gboolean is_empty;
};

#endif
