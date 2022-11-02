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

/* Set flag for features with selectors */
#define GST_PYLON_PARAM_IS_SELECTOR (1 << (G_PARAM_USER_SHIFT + 1))
#define GST_PYLON_PARAM_FLAG_IS_SET(pspec, flag) ((pspec)->flags & (flag))

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

/**
 * GstPylonParamSelectorBoolean:
 *
 * A fundamental type that describes a #GParamSpec Pylon
 * features controlled by a Selector
 */

#define GST_PYLON_TYPE_PARAM_SELECTOR_BOOLEAN\
  (gst_pylon_param_spec_selector_boolean_get_type())
#define GST_PYLON_IS_PARAM_SPEC_SELECTOR_BOOLEAN(pspec) \
  (G_TYPE_CHECK_INSTANCE_TYPE((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_BOOLEAN))
#define GST_PYLON_PARAM_SPEC_SELECTOR_BOOLEAN(pspec)                          \
  (G_TYPE_CHECK_INSTANCE_CAST((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_BOOLEAN, \
                              GstPylonParamSpecSelectorBoolean))

/**
 * GstPylonParamSelectorFloat:
 *
 * A fundamental type that describes a #GParamSpec Pylon
 * features controlled by a Selector
 */

#define GST_PYLON_TYPE_PARAM_SELECTOR_FLOAT \
  (gst_pylon_param_spec_selector_float_get_type())
#define GST_PYLON_IS_PARAM_SPEC_SELECTOR_FLOAT(pspec) \
  (G_TYPE_CHECK_INSTANCE_TYPE((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_FLOAT))
#define GST_PYLON_PARAM_SPEC_SELECTOR_FLOAT(pspec)                          \
  (G_TYPE_CHECK_INSTANCE_CAST((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_FLOAT, \
                              GstPylonParamSpecSelectorFloat))

/**
 * GstPylonParamSelectorString:
 *
 * A fundamental type that describes a #GParamSpec Pylon
 * features controlled by a Selector
 */

#define GST_PYLON_TYPE_PARAM_SELECTOR_STRING \
  (gst_pylon_param_spec_selector_string_get_type())
#define GST_PYLON_IS_PARAM_SPEC_SELECTOR_STRING(pspec) \
  (G_TYPE_CHECK_INSTANCE_TYPE((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_STRING))
#define GST_PYLON_PARAM_SPEC_SELECTOR_STRING(pspec)                          \
  (G_TYPE_CHECK_INSTANCE_CAST((pspec), GST_PYLON_TYPE_PARAM_SELECTOR_STRING, \
                              GstPylonParamSpecSelectorString))

/* --- get_type functions --- */

GType gst_pylon_param_spec_selector_int64_get_type(void);
GType gst_pylon_param_spec_selector_boolean_get_type(void);
GType gst_pylon_param_spec_selector_float_get_type(void);
GType gst_pylon_param_spec_selector_string_get_type(void);
GType gst_pylon_param_spec_selector_enum_register(GenApi::INodeMap& nodemap,
                                                  const gchar* feature_name,
                                                  GType enum_feature_type,
                                                  const gchar* device_fullname);

/* --- typedefs & structures --- */

typedef struct _GstPylonParamSpecSelectorInt64 GstPylonParamSpecSelectorInt64;
typedef struct _GstPylonParamSpecSelectorBoolean GstPylonParamSpecSelectorBoolean;
typedef struct _GstPylonParamSpecSelectorFloat GstPylonParamSpecSelectorFloat;
typedef struct _GstPylonParamSpecSelectorString GstPylonParamSpecSelectorString;
typedef struct _GstPylonParamSpecSelectorEnum GstPylonParamSpecSelectorEnum;

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
  gchar* feature;
  gchar* selector;
  guint64 selector_value;
};

/**
 * GstPylonParamSpecSelectorBoolean:
 * @parent_instance: super class
 * @base: an existing boolean param spec to rely on
 * @selector_name: the name of the selector that controls the feature
 * @feature_name: the name of the feature controlled by the selector
 *
 * A GParamSpec derived structure that contains the meta data for Pylon
 * boolean features controlled by a selector.
 */
struct _GstPylonParamSpecSelectorBoolean {
  GParamSpec parent_instance;
  GParamSpec* base;
  gchar* feature;
  gchar* selector;
  guint64 selector_value;
};

/**
 * GstPylonParamSpecSelectorFloat:
 * @parent_instance: super class
 * @base: an existing int64 param spec to rely on
 * @selector_name: the name of the selector that controls the feature
 * @feature_name: the name of the feature controlled by the selector
 *
 * A GParamSpec derived structure that contains the meta data for Pylon
 * float features controlled by a selector.
 */
struct _GstPylonParamSpecSelectorFloat {
  GParamSpec parent_instance;
  GParamSpec* base;
  gchar* feature;
  gchar* selector;
  guint64 selector_value;
};

/**
 * GstPylonParamSpecSelectorString:
 * @parent_instance: super class
 * @base: an existing string param spec to rely on
 * @selector_name: the name of the selector that controls the feature
 * @feature_name: the name of the feature controlled by the selector
 *
 * A GParamSpec derived structure that contains the meta data for Pylon
 * string features controlled by a selector.
 */
struct _GstPylonParamSpecSelectorString {
  GParamSpec parent_instance;
  GParamSpec* base;
  gchar* feature;
  gchar* selector;
  guint64 selector_value;
};

/**
 * GstPylonParamSpecSelectorEnum:
 * @parent_instance: super class
 * @base: an existing enumaration param spec to rely on
 * @selector_name: the name of the selector that controls the feature
 * @feature_name: the name of the feature controlled by the selector
 *
 * A GParamSpec derived structure that contains the meta data for Pylon
 * string features controlled by a selector.
 */
struct _GstPylonParamSpecSelectorEnum {
  GParamSpec parent_instance;
  GParamSpec* base;
  gchar* feature;
  gchar* selector;
  guint64 selector_value;
};

/* --- GParamSpec prototypes --- */

GParamSpec* gst_pylon_param_spec_selector_int64(
    GenApi::INodeMap& nodemap, const gchar* selector_name,
    const gchar* feature_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, gint64 min, gint64 max, gint64 def,
    GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_boolean(
    GenApi::INodeMap& nodemap, const gchar* selector_name,
    const gchar* feature_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, gboolean def, GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_float(
    GenApi::INodeMap& nodemap, const gchar* feature_name,
    const gchar* selector_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, gdouble min, gdouble max, gdouble def,
    GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_string(
    GenApi::INodeMap& nodemap, const gchar* feature_name,
    const gchar* selector_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, const gchar* def, GParamFlags flags) G_GNUC_MALLOC;
GParamSpec* gst_pylon_param_spec_selector_enum(
    GenApi::INodeMap& nodemap, const gchar* feature_name,
    const gchar* selector_name, guint64 selector_value, const gchar* nick,
    const gchar* blurb, GType type, gint64 def, GParamFlags flags,
    const gchar* device_fullname) G_GNUC_MALLOC;

/* --- Utility prototypes --- */

gchar* gst_pylon_param_spec_sanitize_name(const gchar* name);

G_END_DECLS

#endif /* __GST_PYLON_PARAM_SPECS_H__ */
