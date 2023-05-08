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
#  include "config.h"
#endif

#include "gstpylondebug.h"
#include "gstpylonfeaturewalker.h"
#include "gstpylonobject.h"
#include "gstpylonparamspecs.h"

#include <utility>

/************************************************************
 * Start of GObject definition
 ***********************************************************/

static void gst_pylon_object_init(GstPylonObject* self);
static void gst_pylon_object_class_init(
    GstPylonObjectClass* klass, GstPylonObjectDeviceMembers* device_members);
static gpointer gst_pylon_object_parent_class = NULL;
static gint GstPylonObject_private_offset;
static void gst_pylon_object_class_intern_init(
    gpointer klass, GstPylonObjectDeviceMembers* device_members) {
  gst_pylon_object_parent_class = g_type_class_peek_parent(klass);
  if (GstPylonObject_private_offset != 0)
    g_type_class_adjust_private_offset(klass, &GstPylonObject_private_offset);
  gst_pylon_object_class_init((GstPylonObjectClass*)klass, device_members);
}

GType gst_pylon_object_register(const std::string& device_name,
                                GstPylonCache& feature_cache,
                                GenApi::INodeMap& exemplar) {
  GstPylonObjectDeviceMembers* device_members =
      new GstPylonObjectDeviceMembers({device_name, feature_cache, exemplar});

  GTypeInfo typeinfo = {
      sizeof(GstPylonObjectClass),
      NULL,
      NULL,
      (GClassInitFunc)gst_pylon_object_class_intern_init,
      NULL,
      device_members,
      sizeof(GstPylonObject),
      0,
      (GInstanceInitFunc)gst_pylon_object_init,
  };

  /* Convert camera name to a valid string */
  std::string type_name =
      gst_pylon_param_spec_sanitize_name(device_name.c_str());

  GType type = g_type_from_name(type_name.c_str());
  if (!type) {
    type = g_type_register_static(GST_TYPE_OBJECT, type_name.c_str(), &typeinfo,
                                  static_cast<GTypeFlags>(0));
  }

  GstPylonObject_private_offset =
      g_type_add_instance_private(type, sizeof(GstPylonObjectPrivate));

  return type;
}

/************************************************************
 * End of GObject definition
 ***********************************************************/

gpointer gst_pylon_object_get_instance_private(GstPylonObject* self) {
  return (G_STRUCT_MEMBER_P(self, GstPylonObject_private_offset));
}

/* prototypes */
static void gst_pylon_object_install_properties(GstPylonObjectClass* klass,
                                                GenApi::INodeMap& nodemap,
                                                const std::string& device_name,
                                                GstPylonCache& feature_cache);

/* Set a pylon feature from a gstreamer gst property */
template <typename F, typename P>
static void gst_pylon_object_set_pylon_feature(GstPylonObjectPrivate* priv,
                                               F get_value, const GValue* value,
                                               const gchar* name);

template <typename F, typename P>
static void gst_pylon_object_set_pylon_selected_feature(
    GenApi::INodeMap& nodemap, F get_value, const GValue* value,
    const gchar* feature, const gchar* selector, guint64& selector_value);

template <typename T, typename P>
static T gst_pylon_object_get_pylon_property(GenApi::INodeMap& nodemap,
                                             const gchar* name);

template <typename F, typename P>
static void gst_pylon_object_feature_set_value(
    GParamSpec* pspec, GstPylonObjectPrivate* priv,
    GstPylonParamSpecSelectorData* selector_data, F get_value,
    const GValue* value);

template <typename F, typename P>
static void gst_pylon_object_feature_get_value(
    GParamSpec* pspec, GstPylonObjectPrivate* priv,
    GstPylonParamSpecSelectorData* selector_data, F get_value,
    const GValue* value);

static void gst_pylon_object_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec);
static void gst_pylon_object_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec);
static void gst_pylon_object_finalize(GObject* self);

/* GValue get fptr */
typedef gint64 (*GGetInt64)(const GValue*);
typedef gboolean (*GGetBool)(const GValue*);
typedef gdouble (*GGetDouble)(const GValue*);
typedef const gchar* (*GGetString)(const GValue*);
typedef gint (*GGetEnum)(const GValue*);

/* GValue set fptr */
typedef void (*GSetInt64)(GValue*, gint64);
typedef void (*GSetBool)(GValue*, gboolean);
typedef void (*GSetDouble)(GValue*, gdouble);
typedef void (*GSetString)(GValue*, const gchar*);
typedef void (*GSetEnum)(GValue*, gint);

/* implementations */

static void gst_pylon_object_install_properties(GstPylonObjectClass* klass,
                                                GenApi::INodeMap& nodemap,
                                                const std::string& device_name,
                                                GstPylonCache& feature_cache) {
  g_return_if_fail(klass);

  GObjectClass* oclass = G_OBJECT_CLASS(klass);

  GstPylonFeatureWalker::install_properties(oclass, nodemap, device_name,
                                            feature_cache);
}

static void gst_pylon_object_class_init(
    GstPylonObjectClass* klass, GstPylonObjectDeviceMembers* device_members) {
  GObjectClass* oclass = G_OBJECT_CLASS(klass);

  oclass->set_property = gst_pylon_object_set_property;
  oclass->get_property = gst_pylon_object_get_property;
  oclass->finalize = gst_pylon_object_finalize;

  gst_pylon_object_install_properties(klass, device_members->nodemap,
                                      device_members->device_name,
                                      device_members->feature_cache);

  delete (device_members);
}

static void gst_pylon_object_init(GstPylonObject* self) {}

/* Set pylon feature from gst property */
template <class F, typename P>
static void gst_pylon_object_set_pylon_feature(GstPylonObjectPrivate* priv,
                                               F get_value, const GValue* value,
                                               const gchar* name) {
  P param(*priv->nodemap, name);
  param.SetValue(get_value(value));
  GST_INFO("Set Feature %s: %s", name, param.ToString().c_str());
}

template <>
void gst_pylon_object_set_pylon_feature<GGetInt64, Pylon::CIntegerParameter>(
    GstPylonObjectPrivate* priv, GGetInt64 get_value, const GValue* value,
    const gchar* name) {
  Pylon::CIntegerParameter param(*priv->nodemap, name);
  int64_t gst_val = get_value(value);
  bool value_corrected = false;

  if (priv->enable_correction) {
    /* the rules to check an integer are complex.
     * leave decision to correct the value to genicam
     */
    try {
      param.SetValue(gst_val);
    } catch (GenICam::OutOfRangeException&) {
      value_corrected = true;
      param.SetValue(
          gst_val,
          Pylon::EIntegerValueCorrection::IntegerValueCorrection_Nearest);
    }
  } else {
    param.SetValue(get_value(value));
  }
  GST_INFO("Set Feature %s: %s%s", name, param.ToString().c_str(),
           value_corrected ? " [corrected]" : "");
}

template <>
void gst_pylon_object_set_pylon_feature<GGetDouble, Pylon::CFloatParameter>(
    GstPylonObjectPrivate* priv, GGetDouble get_value, const GValue* value,
    const gchar* name) {
  Pylon::CFloatParameter param(*priv->nodemap, name);
  int64_t gst_val = get_value(value);
  bool value_corrected = false;
  if (priv->enable_correction &&
      (gst_val > param.GetMax() || gst_val < param.GetMin())) {
    value_corrected = true;
    param.SetValue(
        gst_val,
        Pylon::EFloatValueCorrection::FloatValueCorrection_ClipToRange);
  } else {
    param.SetValue(gst_val);
  }

  GST_INFO("Set Feature %s: %s%s", name, param.ToString().c_str(),
           value_corrected ? " [corrected]" : "");
}

template <>
void gst_pylon_object_set_pylon_feature<GGetEnum, Pylon::CEnumParameter>(
    GstPylonObjectPrivate* priv, GGetEnum get_value, const GValue* value,
    const gchar* name) {
  Pylon::CEnumParameter param(*priv->nodemap, name);
  param.SetIntValue(get_value(value));
  GST_INFO("Set Feature %s: %s", name, param.ToString().c_str());
}

/* Get gst property from pylon feature */
template <class F, typename P>
static void gst_pylon_object_get_pylon_feature(GenApi::INodeMap& nodemap,
                                               F set_value, GValue* value,
                                               const gchar* name) {
  P param(nodemap, name);
  set_value(value, param.GetValue());
  GST_DEBUG("Get Feature %s: %s", name, param.ToString().c_str());
}

template <>
void gst_pylon_object_get_pylon_feature<GSetEnum, Pylon::CEnumParameter>(
    GenApi::INodeMap& nodemap, GSetEnum set_value, GValue* value,
    const gchar* name) {
  Pylon::CEnumParameter param(nodemap, name);
  set_value(value, param.GetIntValue());
  GST_DEBUG("Get Feature %s: %s", name, param.ToString().c_str());
}

template <>
void gst_pylon_object_get_pylon_feature<GSetString, Pylon::CStringParameter>(
    GenApi::INodeMap& nodemap, GSetString set_value, GValue* value,
    const gchar* name) {
  Pylon::CStringParameter param(nodemap, name);
  set_value(value, param.GetValue().c_str());
  GST_DEBUG("Get Feature %s: %s", name, param.ToString().c_str());
}

void gst_pylon_object_set_pylon_selector(GenApi::INodeMap& nodemap,
                                         const gchar* selector_name,
                                         gint64& selector_value) {
  gint selector_type =
      nodemap.GetNode(selector_name)->GetPrincipalInterfaceType();
  switch (selector_type) {
    case GenApi::intfIEnumeration:
      Pylon::CEnumParameter(nodemap, selector_name).SetIntValue(selector_value);
      GST_INFO(
          "Set Selector-Feature %s: %s", selector_name,
          Pylon::CEnumParameter(nodemap, selector_name).ToString().c_str());
      break;
    case GenApi::intfIInteger:
      Pylon::CIntegerParameter(nodemap, selector_name).SetValue(selector_value);
      GST_INFO(
          "Set Selector-Feature %s: %s", selector_name,
          Pylon::CIntegerParameter(nodemap, selector_name).ToString().c_str());
      break;
    default:
      std::string error_msg = "Selector \"" + std::string(selector_name) +
                              "\"" + " is of invalid type " +
                              std::to_string(selector_type);
      g_warning("%s", error_msg.c_str());
      throw Pylon::GenericException(error_msg.c_str(), __FILE__, __LINE__);
  }
}

template <typename T, typename P>
static T gst_pylon_object_get_pylon_property(GenApi::INodeMap& nodemap,
                                             const gchar* name) {
  P param(nodemap, name);
  T val = param.GetValue();
  GST_DEBUG("Get Feature %s: %s", name, param.ToString().c_str());
  return val;
}

template <typename F, typename P>
static void gst_pylon_object_feature_set_value(
    GParamSpec* pspec, GstPylonObjectPrivate* priv,
    GstPylonParamSpecSelectorData* selector_data, F get_value,
    const GValue* value) {
  /* The value accepted by the pspec can be a direct feature or a feature that
   * has a selector. */

  if (GST_PYLON_PARAM_FLAG_IS_SET(pspec, GST_PYLON_PARAM_IS_SELECTOR)) {
    gst_pylon_object_set_pylon_selector(*priv->nodemap, selector_data->selector,
                                        selector_data->selector_value);
    gst_pylon_object_set_pylon_feature<F, P>(priv, get_value, value,
                                             selector_data->feature);
  } else {
    /* Decanonicalize gst to pylon name */
    gchar** split = g_strsplit(pspec->name, "-", -1);
    gchar* name_pylon = g_strjoinv("_", split);
    g_strfreev(split);
    gst_pylon_object_set_pylon_feature<F, P>(priv, get_value, value,
                                             name_pylon);
    g_free(name_pylon);
  }
}

template <typename F, typename P>
static void gst_pylon_object_feature_get_value(
    GParamSpec* pspec, GstPylonObjectPrivate* priv,
    GstPylonParamSpecSelectorData* selector_data, F set_value, GValue* value) {
  /* The value accepted by the pspec can be a direct feature or a feature that
   * has a selector. */
  if (GST_PYLON_PARAM_FLAG_IS_SET(pspec, GST_PYLON_PARAM_IS_SELECTOR)) {
    gst_pylon_object_set_pylon_selector(*priv->nodemap, selector_data->selector,
                                        selector_data->selector_value);
    gst_pylon_object_get_pylon_feature<F, P>(*priv->nodemap, set_value, value,
                                             selector_data->feature);
  } else {
    /* Decanonicalize gst to pylon name */
    gchar** split = g_strsplit(pspec->name, "-", -1);
    gchar* name_pylon = g_strjoinv("_", split);
    g_strfreev(split);
    gst_pylon_object_get_pylon_feature<F, P>(*priv->nodemap, set_value, value,
                                             name_pylon);
    g_free(name_pylon);
  }
}

static void gst_pylon_object_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec) {
  GstPylonObject* self = (GstPylonObject*)object;
  GstPylonObjectPrivate* priv =
      (GstPylonObjectPrivate*)gst_pylon_object_get_instance_private(self);
  GType value_type = g_type_fundamental(G_VALUE_TYPE(value));
  GstPylonParamSpecSelectorData* selector_data = NULL;

  if (GST_PYLON_PARAM_FLAG_IS_SET(pspec, GST_PYLON_PARAM_IS_SELECTOR)) {
    selector_data = gst_pylon_param_spec_selector_get_data(pspec);
  }

  /* check if property is from dimension list
   * and set before streaming
   */
  if (!priv->camera->IsGrabbing()) {
    bool is_cached = false;
    if (std::string(pspec->name) == "OffsetX") {
      priv->dimension_cache.offsetx = g_value_get_int64(value);
      is_cached = true;
    } else if (std::string(pspec->name) == "OffsetY") {
      priv->dimension_cache.offsety = g_value_get_int64(value);
      is_cached = true;
    } else if (std::string(pspec->name) == "Width") {
      priv->dimension_cache.width = g_value_get_int64(value);
      is_cached = true;
    } else if (std::string(pspec->name) == "Height") {
      priv->dimension_cache.height = g_value_get_int64(value);
      is_cached = true;
    }
    if (is_cached) {
      GST_INFO("Caching property \"%s\". Value is checked during caps fixation",
               pspec->name);

      /* skip to set the camera property value
       * any value in the gst property range of this feature is accepted in this
       * phase
       */
      return;
    }
  }

  try {
    switch (value_type) {
      case G_TYPE_INT64:
        gst_pylon_object_feature_set_value<GGetInt64, Pylon::CIntegerParameter>(
            pspec, priv, selector_data, g_value_get_int64, value);
        break;
      case G_TYPE_BOOLEAN:
        gst_pylon_object_feature_set_value<GGetBool, Pylon::CBooleanParameter>(
            pspec, priv, selector_data, g_value_get_boolean, value);
        break;
      case G_TYPE_DOUBLE:
        gst_pylon_object_feature_set_value<GGetDouble, Pylon::CFloatParameter>(
            pspec, priv, selector_data, g_value_get_double, value);
        break;
      case G_TYPE_STRING:
        gst_pylon_object_feature_set_value<GGetString, Pylon::CStringParameter>(
            pspec, priv, selector_data, g_value_get_string, value);
        break;
      case G_TYPE_ENUM:
        gst_pylon_object_feature_set_value<GGetEnum, Pylon::CEnumParameter>(
            pspec, priv, selector_data, g_value_get_enum, value);
        break;

      default:
        g_warning("Unsupported GType: %s", g_type_name(pspec->value_type));
        std::string msg =
            "Unsupported GType: " + std::string(g_type_name(pspec->value_type));
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } catch (const Pylon::GenericException& e) {
    GST_ERROR("Unable to set pylon property \"%s\" on \"%s\": %s", pspec->name,
              priv->camera->GetDeviceInfo().GetFriendlyName().c_str(),
              e.GetDescription());
  }
}

static void gst_pylon_object_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec) {
  GstPylonObject* self = (GstPylonObject*)object;
  GstPylonObjectPrivate* priv =
      (GstPylonObjectPrivate*)gst_pylon_object_get_instance_private(self);
  GstPylonParamSpecSelectorData* selector_data = NULL;

  if (GST_PYLON_PARAM_FLAG_IS_SET(pspec, GST_PYLON_PARAM_IS_SELECTOR)) {
    selector_data = gst_pylon_param_spec_selector_get_data(pspec);
  }

  /* check if property is from dimension list
   * and get from cache if not streaming
   */
  if (!priv->camera->IsGrabbing()) {
    bool is_cached = false;
    if (std::string(pspec->name) == "OffsetX" &&
        priv->dimension_cache.offsetx >= 0) {
      is_cached = true;
      g_value_set_int64(value, priv->dimension_cache.offsetx);
    } else if (std::string(pspec->name) == "OffsetY" &&
               priv->dimension_cache.offsety >= 0) {
      is_cached = true;
      g_value_set_int64(value, priv->dimension_cache.offsety);
    } else if (std::string(pspec->name) == "Width" &&
               priv->dimension_cache.width >= 0) {
      is_cached = true;
      g_value_set_int64(value, priv->dimension_cache.width);
    } else if (std::string(pspec->name) == "Height" &&
               priv->dimension_cache.height >= 0) {
      is_cached = true;
      g_value_set_int64(value, priv->dimension_cache.height);
    }

    if (is_cached) {
      GST_INFO(
          "Read cached property \"%s\". Value might be adjusted during caps "
          "fixation",
          pspec->name);

      return;
    }
  }

  try {
    switch (g_type_fundamental(pspec->value_type)) {
      case G_TYPE_INT64:
        gst_pylon_object_feature_get_value<GSetInt64, Pylon::CIntegerParameter>(
            pspec, priv, selector_data, g_value_set_int64, value);
        break;
      case G_TYPE_BOOLEAN:
        gst_pylon_object_feature_get_value<GSetBool, Pylon::CBooleanParameter>(
            pspec, priv, selector_data, g_value_set_boolean, value);
        break;
      case G_TYPE_DOUBLE:
        gst_pylon_object_feature_get_value<GSetDouble, Pylon::CFloatParameter>(
            pspec, priv, selector_data, g_value_set_double, value);
        break;
      case G_TYPE_STRING:
        gst_pylon_object_feature_get_value<GSetString, Pylon::CStringParameter>(
            pspec, priv, selector_data, g_value_set_string, value);
        break;
      case G_TYPE_ENUM:
        gst_pylon_object_feature_get_value<GSetEnum, Pylon::CEnumParameter>(
            pspec, priv, selector_data, g_value_set_enum, value);
        break;
      default:
        g_warning("Unsupported GType: %s", g_type_name(pspec->value_type));
        std::string msg =
            "Unsupported GType: " + std::string(g_type_name(pspec->value_type));
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } catch (const Pylon::GenericException& e) {
    GST_ERROR("Unable to get pylon property \"%s\" on \"%s\": %s", pspec->name,
              priv->camera->GetDeviceInfo().GetFriendlyName().c_str(),
              e.GetDescription());
  }
}

GObject* gst_pylon_object_new(
    std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera,
    const std::string& device_name, GenApi::INodeMap* nodemap,
    gboolean enable_correction) {
  std::string type_name =
      gst_pylon_param_spec_sanitize_name(device_name.c_str());

  GType type = g_type_from_name(type_name.c_str());

  std::unique_ptr<GstPylonCache> feature_cache;

  if (!type) {
    std::string cache_filename =
        std::string(camera->GetDeviceInfo().GetModelName() + "_" +
                    Pylon::VersionInfo::getVersionString() + "_" + VERSION);
    feature_cache = std::make_unique<GstPylonCache>(cache_filename);
    type = gst_pylon_object_register(device_name, *feature_cache, *nodemap);
  }

  GObject* obj = G_OBJECT(g_object_new(type, "name", type_name.c_str(), NULL));
  GstPylonObject* self = (GstPylonObject*)obj;
  GstPylonObjectPrivate* priv =
      (GstPylonObjectPrivate*)gst_pylon_object_get_instance_private(self);

  priv->camera = std::move(camera);
  priv->nodemap = nodemap;
  priv->enable_correction = enable_correction;

  /* setup dimension cache
   * -1 -> not activly set
   */
  priv->dimension_cache = {-1, -1, -1, -1};

  return obj;
}

static void gst_pylon_object_finalize(GObject* object) {
  GstPylonObject* self = (GstPylonObject*)object;
  GstPylonObjectPrivate* priv =
      (GstPylonObjectPrivate*)gst_pylon_object_get_instance_private(self);

  priv->camera = NULL;

  G_OBJECT_CLASS(gst_pylon_object_parent_class)->finalize(object);
}
