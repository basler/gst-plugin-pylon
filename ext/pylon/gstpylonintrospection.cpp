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

#include "gstpylonintrospection.h"

#include "gstpylonparamspecs.h"

#include <unordered_map>

/* prototypes */
static GParamSpec *gst_pylon_make_spec_int64(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_int64(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_bool(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_bool(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_float(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_float(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_str(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_str(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value);
static GType gst_pylon_make_enum_type(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_enum(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_enum(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value);
static GParamFlags gst_pylon_query_access(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node);

static GParamFlags gst_pylon_query_access(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  gint flags = 0;

  g_return_val_if_fail(node, static_cast<GParamFlags>(flags));

  Pylon::CParameter param(node);

  if (param.IsReadable()) {
    flags |= G_PARAM_READABLE;
  }
  if (param.IsWritable()) {
    flags |= G_PARAM_WRITABLE;
  }

  GenICam::gcstring value;
  GenICam::gcstring attribute;
  if (node->GetProperty("pIsLocked", value, attribute)) {
    flags |= GST_PARAM_MUTABLE_READY;
  } else {
    flags |= GST_PARAM_MUTABLE_PLAYING;
  }

  return static_cast<GParamFlags>(flags);
}

static GParamSpec *gst_pylon_make_spec_int64(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CIntegerParameter param(node);

  return g_param_spec_int64(node->GetName(), node->GetDisplayName(),
                            node->GetToolTip(), param.GetMin(), param.GetMax(),
                            param.GetValue(),
                            gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_selector_int64(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(camera, NULL);
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CIntegerParameter param(node);

  return gst_pylon_param_spec_selector_int64(
      camera, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetMin(),
      param.GetMax(), param.GetValue(), gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_bool(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CBooleanParameter param(node);

  return g_param_spec_boolean(node->GetName(), node->GetDisplayName(),
                              node->GetToolTip(), param.GetValue(),
                              gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_selector_bool(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(camera, NULL);
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CBooleanParameter param(node);

  return gst_pylon_param_spec_selector_bool(
      camera, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_float(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CFloatParameter param(node);

  return g_param_spec_float(node->GetName(), node->GetDisplayName(),
                            node->GetToolTip(), param.GetMin(), param.GetMax(),
                            param.GetValue(),
                            gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_selector_float(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(camera, NULL);
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CFloatParameter param(node);

  return gst_pylon_param_spec_selector_float(
      camera, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetMin(),
      param.GetMax(), param.GetValue(), gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_str(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CStringParameter param(node);

  return g_param_spec_string(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), param.GetValue(),
                             gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_selector_str(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(camera, NULL);
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CStringParameter param(node);

  return gst_pylon_param_spec_selector_str(
      camera, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(camera, node));
}

static GType gst_pylon_make_enum_type(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  /* When registering enums to the GType system, their string pointers
     must remain valid throughout the application lifespan. To achieve this
     we are saving all found enums into a static hash table
  */
  static std::unordered_map<GType, std::vector<GEnumValue>> persistent_values;

  g_return_val_if_fail(node, G_TYPE_INVALID);
  g_return_val_if_fail(camera, G_TYPE_INVALID);

  Pylon::CEnumParameter param(node);

  gchar *full_name =
      g_strdup_printf("%s_%s", camera->GetDeviceInfo().GetFullName().c_str(),
                      node->GetName().c_str());
  gchar *name = gst_pylon_param_spec_sanitize_name(full_name);
  g_free(full_name);

  GType type = g_type_from_name(name);

  if (!type) {
    std::vector<GEnumValue> enumvalues;
    GenApi::StringList_t values;

    param.GetSettableValues(values);
    for (const auto &value_name : values) {
      auto entry = param.GetEntryByName(value_name);
      auto value = static_cast<gint>(entry->GetValue());
      auto tooltip = entry->GetNode()->GetToolTip();

      /* We need a copy of the strings so that they are persistent
         throughout the application lifespan */
      GEnumValue ev = {value, g_strdup(value_name.c_str()),
                       g_strdup(tooltip.c_str())};
      enumvalues.push_back(ev);
    }

    GEnumValue sentinel = {0};
    enumvalues.push_back(sentinel);

    type = g_enum_register_static(name, enumvalues.data());
    persistent_values.insert({type, std::move(enumvalues)});
  }

  g_free(name);

  return type;
}

static GParamSpec *gst_pylon_make_spec_enum(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(camera, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(camera, node);

  return g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                           node->GetToolTip(), type, param.GetIntValue(),
                           gst_pylon_query_access(camera, node));
}

static GParamSpec *gst_pylon_make_spec_selector_enum(
    Pylon::CBaslerUniversalInstantCamera *camera, GenApi::INode *node,
    GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(camera, NULL);
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(camera, node);

  return gst_pylon_param_spec_selector_enum(
      camera, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), type, param.GetIntValue(),
      gst_pylon_query_access(camera, node));
}

GParamSpec *GstPylonParamFactory::make_param(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value,
    Pylon::CBaslerUniversalInstantCamera *camera) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(camera, NULL);

  GParamSpec *spec = NULL;
  GenApi::EInterfaceType iface = node->GetPrincipalInterfaceType();

  switch (iface) {
    case GenApi::intfIInteger:
      if (!selector) {
        spec = gst_pylon_make_spec_int64(camera, node);
      } else {
        spec = gst_pylon_make_spec_selector_int64(camera, node, selector,
                                                  selector_value);
      }
      break;
    case GenApi::intfIBoolean:
      if (!selector) {
        spec = gst_pylon_make_spec_bool(camera, node);
      } else {
        spec = gst_pylon_make_spec_selector_bool(camera, node, selector,
                                                 selector_value);
      }
      break;
    case GenApi::intfIFloat:
      if (!selector) {
        spec = gst_pylon_make_spec_float(camera, node);
      } else {
        spec = gst_pylon_make_spec_selector_float(camera, node, selector,
                                                  selector_value);
      }
      break;
    case GenApi::intfIString:
      if (!selector) {
        spec = gst_pylon_make_spec_str(camera, node);
      } else {
        spec = gst_pylon_make_spec_selector_str(camera, node, selector,
                                                selector_value);
      }
      break;
    case GenApi::intfIEnumeration:
      if (!selector) {
        spec = gst_pylon_make_spec_enum(camera, node);
      } else {
        spec = gst_pylon_make_spec_selector_enum(camera, node, selector,
                                                 selector_value);
      }
      break;
    default:
      std::string msg = "Unsupported node of type " +
                        std::to_string(node->GetPrincipalInterfaceType());
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }

  return spec;
}
