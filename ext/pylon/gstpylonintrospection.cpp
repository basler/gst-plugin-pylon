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

#include "gstpylonintrospection.h"

#include "gstpylonparamspecs.h"

#include <unordered_map>

/* prototypes */
static GParamSpec *gst_pylon_make_spec_int64(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_int64(GenApi::INodeMap &nodemap,
                                                      GenApi::INode *node,
                                                      GenApi::INode *selector,
                                                      guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_bool(GenApi::INodeMap &nodemap,
                                            GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_bool(GenApi::INodeMap &nodemap,
                                                     GenApi::INode *node,
                                                     GenApi::INode *selector,
                                                     guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_float(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_float(GenApi::INodeMap &nodemap,
                                                      GenApi::INode *node,
                                                      GenApi::INode *selector,
                                                      guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_str(GenApi::INodeMap &nodemap,
                                           GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_str(GenApi::INodeMap &nodemap,
                                                    GenApi::INode *node,
                                                    GenApi::INode *selector,
                                                    guint64 selector_value);
static GType gst_pylon_make_enum_type(GenApi::INodeMap &nodemap,
                                      GenApi::INode *node,
                                      const gchar *device_fullname);
static GParamSpec *gst_pylon_make_spec_enum(GenApi::INodeMap &nodemap,
                                            GenApi::INode *node,
                                            const gchar *device_fullname);
static GParamSpec *gst_pylon_make_spec_selector_enum(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, const gchar *device_fullname);
static GenApi::INode *gst_pylon_find_limit_node(GenApi::INode *feature_node,
                                                const GenICam::gcstring limit);
static std::vector<GenApi::INode *> gst_pylon_find_parent_features(
    GenApi::INode *feature_node);
static void gst_pylon_add_all_property_values(
    GenApi::INode *feature_node, std::string value,
    std::unordered_map<std::string, GenApi::INode *> &invalidators);
static std::vector<GenApi::INode *> gst_pylon_get_available_features(
    std::vector<GenApi::INode *> feature_list);
template <class Type>
static std::vector<std::vector<Type>> gst_pylon_cartesian_product(
    std::vector<std::vector<Type>> &v);
static gboolean gst_pylon_can_feature_later_be_writable(GenApi::INode *node);
static GParamFlags gst_pylon_query_access(GenApi::INodeMap &nodemap,
                                          GenApi::INode *node);

static gboolean gst_pylon_can_feature_later_be_writable(GenApi::INode *node) {
  GenICam::gcstring value;
  GenICam::gcstring attribute;
  if (node->GetProperty("pIsLocked", value, attribute)) {
    return true;
  } else {
    GenApi::NodeList_t node_children;
    node->GetChildren(node_children);
    if (!node_children.empty()) {
      for (auto &node_child : node_children) {
        return gst_pylon_can_feature_later_be_writable(node_child);
      }
    }
    return FALSE;
  }
}

static GParamFlags gst_pylon_query_access(GenApi::INodeMap &nodemap,
                                          GenApi::INode *node) {
  gint flags = 0;

  g_return_val_if_fail(node, static_cast<GParamFlags>(flags));

  Pylon::CParameter param(node);
  gboolean is_writable = FALSE;

  if (param.IsReadable()) {
    flags |= G_PARAM_READABLE;
  }
  if (param.IsWritable()) {
    flags |= G_PARAM_WRITABLE;
    is_writable = TRUE;
  }
  if (!param.IsWritable() && gst_pylon_can_feature_later_be_writable(node)) {
    flags |= G_PARAM_WRITABLE;
  }

  gboolean is_read_write = param.IsReadable() && param.IsWritable();
  gboolean is_write_only = !param.IsReadable() && param.IsWritable();

  /* Check if feature is writable in PLAYING state */
  if (is_read_write || is_write_only) {
    GenICam::gcstring value;
    GenICam::gcstring attribute;
    if (node->GetProperty("TLParamsLocked", value, attribute)) {
      Pylon::CIntegerParameter tl_params_locked(nodemap, "TLParamsLocked");

      /* Check if feature is writable at runtime by checking if it is still
       * writable after setting TLParamsLocked to 1. */
      tl_params_locked.SetValue(1);
      if (is_writable && param.IsWritable()) {
        flags |= GST_PARAM_MUTABLE_PLAYING;
      } else {
        flags |= GST_PARAM_MUTABLE_READY;
      }
      tl_params_locked.SetValue(0);

    } else {
      flags |= GST_PARAM_MUTABLE_READY;
    }
  }

  return static_cast<GParamFlags>(flags);
}

static GenApi::INode *gst_pylon_find_limit_node(GenApi::INode *node,
                                                const GenICam::gcstring limit) {
  GenApi::INode *limit_node = NULL;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  g_return_val_if_fail(node, NULL);

  if (node->GetProperty(limit, value, attribute)) {
    limit_node = node->GetNodeMap()->GetNode(value);
  } else if (node->GetProperty("pValue", value, attribute)) {
    limit_node =
        gst_pylon_find_limit_node(node->GetNodeMap()->GetNode(value), limit);
  }
  return limit_node;
}

static std::vector<GenApi::INode *> gst_pylon_find_parent_features(
    GenApi::INode *node) {
  std::vector<GenApi::INode *> parent_features;

  g_return_val_if_fail(node, parent_features);

  if (node->IsFeature()) {
    parent_features.push_back(node);
  } else {
    GenApi::NodeList_t parents;
    node->GetParents(parents);
    for (const auto &parent : parents) {
      std::vector<GenApi::INode *> grandparents =
          gst_pylon_find_parent_features(parent);
      parent_features.insert(parent_features.end(), grandparents.begin(),
                             grandparents.end());
    }
  }
  return parent_features;
}

static void gst_pylon_add_all_property_values(
    GenApi::INode *node, std::string value,
    std::unordered_map<std::string, GenApi::INode *> &invalidators) {
  std::string delimiter = "\t";
  gsize pos = 0;
  std::string token;

  g_return_if_fail(node);

  while ((pos = value.find(delimiter)) != std::string::npos) {
    token = value.substr(0, pos);
    if (invalidators.find(token) == invalidators.end()) {
      invalidators[token] = node->GetNodeMap()->GetNode(token.c_str());
    }
    value.erase(0, pos + delimiter.length());
  }

  if (invalidators.find(value) == invalidators.end()) {
    invalidators[value] = node->GetNodeMap()->GetNode(value.c_str());
  }
}

static std::vector<GenApi::INode *> gst_pylon_get_available_features(
    std::vector<GenApi::INode *> feature_list) {
  std::vector<GenApi::INode *> available_features;
  for (const auto &feature : feature_list) {
    if (GenApi::IsAvailable(feature)) {
      available_features.push_back(feature);
    }
  }
  return available_features;
}

template <class Type>
static std::vector<std::vector<Type>> gst_pylon_cartesian_product(
    std::vector<std::vector<Type>> &v) {
  std::vector<std::vector<Type>> result;
  auto product = [](long long a, std::vector<Type> &b) { return a * b.size(); };
  const long long N = accumulate(v.begin(), v.end(), 1LL, product);
  std::vector<Type> u(v.size());
  for (long long n = 0; n < N; ++n) {
    lldiv_t q{n, 0};
    for (long long i = v.size() - 1; 0 <= i; --i) {
      q = div(q.quot, v[i].size());
      u[i] = v[i][q.rem];
    }
    result.push_back(u);
  }
  return result;
}

static GParamSpec *gst_pylon_make_spec_int64(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CIntegerParameter param(node);

  return g_param_spec_int64(node->GetName(), node->GetDisplayName(),
                            node->GetToolTip(), param.GetMin(), param.GetMax(),
                            param.GetValue(),
                            gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_selector_int64(GenApi::INodeMap &nodemap,
                                                      GenApi::INode *node,
                                                      GenApi::INode *selector,
                                                      guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CIntegerParameter param(node);

  return gst_pylon_param_spec_selector_int64(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetMin(),
      param.GetMax(), param.GetValue(), gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_bool(GenApi::INodeMap &nodemap,
                                            GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CBooleanParameter param(node);

  return g_param_spec_boolean(node->GetName(), node->GetDisplayName(),
                              node->GetToolTip(), param.GetValue(),
                              gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_selector_bool(GenApi::INodeMap &nodemap,
                                                     GenApi::INode *node,
                                                     GenApi::INode *selector,
                                                     guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CBooleanParameter param(node);

  return gst_pylon_param_spec_selector_bool(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_float(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CFloatParameter param(node);

  return g_param_spec_float(node->GetName(), node->GetDisplayName(),
                            node->GetToolTip(), param.GetMin(), param.GetMax(),
                            param.GetValue(),
                            gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_selector_float(GenApi::INodeMap &nodemap,
                                                      GenApi::INode *node,
                                                      GenApi::INode *selector,
                                                      guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CFloatParameter param(node);

  return gst_pylon_param_spec_selector_float(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetMin(),
      param.GetMax(), param.GetValue(), gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_str(GenApi::INodeMap &nodemap,
                                           GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CStringParameter param(node);

  return g_param_spec_string(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), param.GetValue(),
                             gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_selector_str(GenApi::INodeMap &nodemap,
                                                    GenApi::INode *node,
                                                    GenApi::INode *selector,
                                                    guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CStringParameter param(node);

  return gst_pylon_param_spec_selector_str(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(nodemap, node));
}

static GType gst_pylon_make_enum_type(GenApi::INodeMap &nodemap,
                                      GenApi::INode *node,
                                      const gchar *device_fullname) {
  /* When registering enums to the GType system, their string pointers
     must remain valid throughout the application lifespan. To achieve this
     we are saving all found enums into a static hash table
  */
  static std::unordered_map<GType, std::vector<GEnumValue>> persistent_values;

  g_return_val_if_fail(node, G_TYPE_INVALID);

  Pylon::CEnumParameter param(node);

  gchar *full_name =
      g_strdup_printf("%s_%s", device_fullname, node->GetName().c_str());
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

static GParamSpec *gst_pylon_make_spec_enum(GenApi::INodeMap &nodemap,
                                            GenApi::INode *node,
                                            const gchar *device_fullname) {
  g_return_val_if_fail(node, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(nodemap, node, device_fullname);

  return g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                           node->GetToolTip(), type, param.GetIntValue(),
                           gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_selector_enum(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, const gchar *device_fullname) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(nodemap, node, device_fullname);

  return gst_pylon_param_spec_selector_enum(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), type, param.GetIntValue(),
      gst_pylon_query_access(nodemap, node), device_fullname);
}

GParamSpec *GstPylonParamFactory::make_param(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node,
                                             GenApi::INode *selector,
                                             guint64 selector_value,
                                             const gchar *device_fullname) {
  g_return_val_if_fail(node, NULL);

  GParamSpec *spec = NULL;
  GenApi::EInterfaceType iface = node->GetPrincipalInterfaceType();

  switch (iface) {
    case GenApi::intfIInteger:
      if (!selector) {
        spec = gst_pylon_make_spec_int64(nodemap, node);
      } else {
        spec = gst_pylon_make_spec_selector_int64(nodemap, node, selector,
                                                  selector_value);
      }
      break;
    case GenApi::intfIBoolean:
      if (!selector) {
        spec = gst_pylon_make_spec_bool(nodemap, node);
      } else {
        spec = gst_pylon_make_spec_selector_bool(nodemap, node, selector,
                                                 selector_value);
      }
      break;
    case GenApi::intfIFloat:
      if (!selector) {
        spec = gst_pylon_make_spec_float(nodemap, node);
      } else {
        spec = gst_pylon_make_spec_selector_float(nodemap, node, selector,
                                                  selector_value);
      }
      break;
    case GenApi::intfIString:
      if (!selector) {
        spec = gst_pylon_make_spec_str(nodemap, node);
      } else {
        spec = gst_pylon_make_spec_selector_str(nodemap, node, selector,
                                                selector_value);
      }
      break;
    case GenApi::intfIEnumeration:
      if (!selector) {
        spec = gst_pylon_make_spec_enum(nodemap, node, device_fullname);
      } else {
        spec = gst_pylon_make_spec_selector_enum(
            nodemap, node, selector, selector_value, device_fullname);
      }
      break;
    default:
      std::string msg = "Unsupported node of type " +
                        std::to_string(node->GetPrincipalInterfaceType());
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }

  return spec;
}
