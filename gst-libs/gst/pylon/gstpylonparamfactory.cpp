/* Copyright (C) 2023 Basler AG
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

#include "gstpylonparamfactory.h"

#include "gstpylonintrospection.h"
#include "gstpylonobject.h"
#include "gstpylonparamspecs.h"

#include <mutex>
#include <unordered_map>

static gint64 gst_pylon_clamp_int64_default(Pylon::CIntegerParameter& param,
                                            gint64 min_val, gint64 max_val) {
  gint64 def = min_val;
  if (param.IsReadable()) {
    try {
      def = param.GetValue();
    } catch (const GenICam::GenericException&) {
      def = min_val;
    }
  }
  if (def < min_val) {
    def = min_val;
  }
  if (def > max_val) {
    def = max_val;
  }
  return def;
}

static gdouble gst_pylon_clamp_double_default(Pylon::CFloatParameter& param,
                                              gdouble min_val,
                                              gdouble max_val) {
  gdouble def = min_val;
  if (param.IsReadable()) {
    try {
      def = param.GetValue();
    } catch (const GenICam::GenericException&) {
      def = min_val;
    }
  }
  if (def < min_val) {
    def = min_val;
  }
  if (def > max_val) {
    def = max_val;
  }
  return def;
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_int64(
    GenApi::INode* node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CIntegerParameter param(node);
  gint64 max_value = 0;
  gint64 min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_integer(
      schema.node_map(), node, schema.cache(), flags, min_value, max_value,
      NULL, 0, device_profile);

  return g_param_spec_int64(
      node->GetName(), node->GetDisplayName(), node->GetToolTip(), min_value,
      max_value, gst_pylon_clamp_int64_default(param, min_value, max_value),
      flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_selector_int64(
    GenApi::INode* node, GenApi::INode* selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CIntegerParameter param(node);
  gint64 max_value = 0;
  gint64 min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_integer(
      schema.node_map(), node, schema.cache(), flags, min_value, max_value,
      selector, selector_value, device_profile);

  return gst_pylon_param_spec_selector_int64(
      schema.node_map(), node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), min_value, max_value,
      gst_pylon_clamp_int64_default(param, min_value, max_value), flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_bool(
    GenApi::INode* node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CBooleanParameter param(node);
  GParamFlags flags = G_PARAM_READABLE;
  gst_pylon_query_feature_properties_flags(
      schema.node_map(), node, schema.cache(), flags, NULL, 0, device_profile);
  gboolean def = param.IsReadable() ? param.GetValue() : FALSE;

  return g_param_spec_boolean(node->GetName(), node->GetDisplayName(),
                              node->GetToolTip(), def, flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_selector_bool(
    GenApi::INode* node, GenApi::INode* selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CBooleanParameter param(node);
  GParamFlags flags = G_PARAM_READABLE;
  gst_pylon_query_feature_properties_flags(schema.node_map(), node,
                                           schema.cache(), flags, selector,
                                           selector_value, device_profile);
  gboolean def = param.IsReadable() ? param.GetValue() : FALSE;

  return gst_pylon_param_spec_selector_boolean(
      schema.node_map(), node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), def, flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_double(
    GenApi::INode* node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CFloatParameter param(node);
  gdouble max_value = 0;
  gdouble min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_double(schema.node_map(), node,
                                            schema.cache(), flags, min_value,
                                            max_value, NULL, 0, device_profile);

  return g_param_spec_double(
      node->GetName(), node->GetDisplayName(), node->GetToolTip(), min_value,
      max_value, gst_pylon_clamp_double_default(param, min_value, max_value),
      flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_selector_double(
    GenApi::INode* node, GenApi::INode* selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CFloatParameter param(node);
  gdouble max_value = 0;
  gdouble min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_double(
      schema.node_map(), node, schema.cache(), flags, min_value, max_value,
      selector, selector_value, device_profile);

  return gst_pylon_param_spec_selector_double(
      schema.node_map(), node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), min_value, max_value,
      gst_pylon_clamp_double_default(param, min_value, max_value), flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_str(GenApi::INode* node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CStringParameter param(node);
  GParamFlags flags = G_PARAM_READABLE;
  gst_pylon_query_feature_properties_flags(
      schema.node_map(), node, schema.cache(), flags, NULL, 0, device_profile);
  std::string def = param.IsReadable() ? param.GetValue().c_str() : "";

  return g_param_spec_string(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), def.c_str(), flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_selector_str(
    GenApi::INode* node, GenApi::INode* selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CStringParameter param(node);
  GParamFlags flags = G_PARAM_READABLE;
  gst_pylon_query_feature_properties_flags(schema.node_map(), node,
                                           schema.cache(), flags, selector,
                                           selector_value, device_profile);
  std::string def = param.IsReadable() ? param.GetValue().c_str() : "";

  return gst_pylon_param_spec_selector_string(
      schema.node_map(), node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), def.c_str(), flags);
}

GType GstPylonParamFactory::gst_pylon_make_enum_type(GenApi::INode* node) {
  /* When registering enums to the GType system, their string pointers
     must remain valid throughout the application lifespan. Protect both
     lookup and insertion with a mutex for multi-camera thread safety.
  */
  static std::mutex enum_registry_mutex;
  static std::unordered_map<GType, std::vector<GEnumValue>> persistent_values;

  g_return_val_if_fail(node, G_TYPE_INVALID);

  Pylon::CEnumParameter param(node);

  gchar* full_name = g_strdup_printf("%s_%s", schema.device_full_name.c_str(),
                                     node->GetName().c_str());
  std::string name = gst_pylon_param_spec_sanitize_name(full_name);
  g_free(full_name);

  std::lock_guard<std::mutex> lock(enum_registry_mutex);
  GType type = g_type_from_name(name.c_str());

  if (!type) {
    std::vector<GEnumValue> enumvalues;
    GenApi::NodeList_t entries;
    node->GetNodeMap();
    auto* enum_iface = dynamic_cast<GenApi::IEnumeration*>(node);
    if (enum_iface) {
      enum_iface->GetEntries(entries);
    }

    /* Include all implemented entries (not just currently settable ones on
     * whichever selector happens to be active right now), so every valid enum
     * value across selector states has a registered GEnumValue. */
    for (auto* entry_node : entries) {
      if (!GenApi::IsImplemented(entry_node)) {
        continue;
      }
      auto* enum_entry = dynamic_cast<GenApi::IEnumEntry*>(entry_node);
      if (!enum_entry) {
        continue;
      }
      auto value = static_cast<gint>(enum_entry->GetValue());
      std::string symbolic(enum_entry->GetSymbolic().c_str());
      auto tooltip = entry_node->GetToolTip();

      /* We need a copy of the strings so that they are persistent
         throughout the application lifespan */
      GEnumValue ev = {value, g_strdup(symbolic.c_str()),
                       g_strdup(tooltip.c_str())};
      enumvalues.push_back(ev);
    }

    if (enumvalues.empty()) {
      std::string msg = "Enumeration " + std::string(node->GetName().c_str()) +
                        " has no implemented entries";
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    GEnumValue sentinel = {0, nullptr, nullptr};
    enumvalues.push_back(sentinel);

    type = g_enum_register_static(name.c_str(), enumvalues.data());
    persistent_values.insert({type, std::move(enumvalues)});
  }

  return type;
}

static gint gst_pylon_resolve_enum_default(GType enum_type,
                                           Pylon::CEnumParameter& param) {
  gint def_val = 0;
  bool has_cur = false;
  if (param.IsReadable()) {
    try {
      def_val = static_cast<gint>(param.GetIntValue());
      has_cur = true;
    } catch (const GenICam::GenericException&) {
      has_cur = false;
    }
  }

  GEnumClass* eclass = static_cast<GEnumClass*>(g_type_class_ref(enum_type));
  if (eclass) {
    if (!has_cur || g_enum_get_value(eclass, def_val) == NULL) {
      if (eclass->n_values > 0) {
        def_val = eclass->values[0].value;
      }
    }
    g_type_class_unref(eclass);
  }
  return def_val;
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_enum(
    GenApi::INode* node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(node);
  GParamFlags flags = G_PARAM_READABLE;
  gst_pylon_query_feature_properties_flags(
      schema.node_map(), node, schema.cache(), flags, NULL, 0, device_profile);
  gint def_val = gst_pylon_resolve_enum_default(type, param);

  return g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                           node->GetToolTip(), type, def_val, flags);
}

GParamSpec* GstPylonParamFactory::gst_pylon_make_spec_selector_enum(
    GenApi::INode* node, GenApi::INode* selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(node);
  GParamFlags flags = G_PARAM_READABLE;
  gst_pylon_query_feature_properties_flags(schema.node_map(), node,
                                           schema.cache(), flags, selector,
                                           selector_value, device_profile);
  gint def_val = gst_pylon_resolve_enum_default(type, param);

  return gst_pylon_param_spec_selector_enum(
      schema.node_map(), node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), type, def_val, flags);
}

GParamSpec* GstPylonParamFactory::make_param(GenApi::INode* node,
                                             GenApi::INode* selector,
                                             guint64 selector_value) {
  g_return_val_if_fail(node, NULL);

  GParamSpec* spec = NULL;
  GenApi::EInterfaceType iface = node->GetPrincipalInterfaceType();

  switch (iface) {
    case GenApi::intfIInteger:
      if (!selector) {
        spec = gst_pylon_make_spec_int64(node);
      } else {
        spec =
            gst_pylon_make_spec_selector_int64(node, selector, selector_value);
      }
      break;
    case GenApi::intfIBoolean:
      if (!selector) {
        spec = gst_pylon_make_spec_bool(node);
      } else {
        spec =
            gst_pylon_make_spec_selector_bool(node, selector, selector_value);
      }
      break;
    case GenApi::intfIFloat:
      if (!selector) {
        spec = gst_pylon_make_spec_double(node);
      } else {
        spec =
            gst_pylon_make_spec_selector_double(node, selector, selector_value);
      }
      break;
    case GenApi::intfIString:
      if (!selector) {
        spec = gst_pylon_make_spec_str(node);
      } else {
        spec = gst_pylon_make_spec_selector_str(node, selector, selector_value);
      }
      break;
    case GenApi::intfIEnumeration:
      if (!selector) {
        spec = gst_pylon_make_spec_enum(node);
      } else {
        spec =
            gst_pylon_make_spec_selector_enum(node, selector, selector_value);
      }
      break;
    default:
      Pylon::String_t msg =
          "Unsupported node of type " + GenApi::GetInterfaceName(node);
      throw Pylon::GenericException(msg, __FILE__, __LINE__);
  }

  if (!spec) {
    Pylon::String_t msg = "Property creation failed for " + node->GetName();
    throw Pylon::GenericException(msg, __FILE__, __LINE__);
  }

  return spec;
}
