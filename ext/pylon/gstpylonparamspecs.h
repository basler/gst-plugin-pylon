/* Copyright (C) 2022 Basler AG
 *
 * Created by RidgeRun, LLC <support@ridgerun.com>
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

#ifndef __GST_PYLON_PARAM_SPECS_H__
#define __GST_PYLON_PARAM_SPECS_H__

#include <gst/gst.h>

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

G_BEGIN_DECLS

/**
 * GstPylonParamSelectorInt64:
 *
 * A fundamental type that describes a #GParamSpec Pylon
 * features controlled by a Selector
 */

#define GST_PYLON_TYPE_PARAM_SELECTOR_INT64 \
  (gst_pylon_param_spec_selector_int64_get_type())
#define GST_PYLON_IS_PARAM_SPEC_SELECTOR_INT64(pspec) \
  (G_TYPE_CHECK_INSTANCE_TYPE((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_INT64))
#define GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec)                          \
  (G_TYPE_CHECK_INSTANCE_CAST((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_INT64, \
                              GstPylonParamSpecSelectorInt64))

/* --- get_type functions --- */

GST_API
GType gst_pylon_param_spec_selector_int64_get_type(void);

/* --- typedefs & structures --- */

typedef struct _GstPylonParamSpecSelectorInt64 GstPylonParamSpecSelectorInt64;

/**
 * GstPylonParamSpecSelectorInt64:
 * @parent_instance: super class
 * @base: an existing int64 param spec to rely on
 * @selector_name: the name of the selector that controls the feature
 * @feature_name: the name of the feature controlled by the selector
 *
 * A GParamSpec derived structure that contains the meta data for Pylon
 * int64eger features controlled by a selector.
 */
struct _GstPylonParamSpecSelectorInt64 {
  GParamSpec parent_instance;
  GParamSpec* base;
  GenApi::INode* feature;
  GenApi::INode* selector;
  gint64 selector_value;
};

/* --- GParamSpec prototypes --- */

GST_API
GParamSpec* gst_pylon_param_spec_selector_int64(
    const gchar* selector_name, const gchar* feature_name, const gchar* nick,
    const gchar* blurb, gint64 min, gint64 max, gint64 def,
    GParamFlags flags) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __GST_PYLON_PARAM_SPECS_H__ */
