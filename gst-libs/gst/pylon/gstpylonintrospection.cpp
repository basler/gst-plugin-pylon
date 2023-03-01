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
#include "gstpylonintrospection.h"
#include "gstpylonobject.h"
#include "gstpylonparamspecs.h"

#include <pylon/PixelType.h>

#include <algorithm>
#include <numeric>
#include <set>
#include <unordered_map>
#include <vector>

class GstPylonActions {
 public:
  void virtual set_value() = 0;
  virtual ~GstPylonActions() = default;
};

template <class P, class V>
class GstPylonTypeAction : public GstPylonActions {
 public:
  GstPylonTypeAction(P param, V value) {
    this->param = param;
    this->value = value;
  }
  void set_value() override { this->param.SetValue(this->value); }

 private:
  P param;
  V value;
};

/* prototypes */
static GParamSpec *gst_pylon_make_spec_int64(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node,
                                             GstPylonCache *feature_cache);
static GParamSpec *gst_pylon_make_spec_selector_int64(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, GstPylonCache *feature_cache);
static GParamSpec *gst_pylon_make_spec_bool(GenApi::INodeMap &nodemap,
                                            GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_bool(GenApi::INodeMap &nodemap,
                                                     GenApi::INode *node,
                                                     GenApi::INode *selector,
                                                     guint64 selector_value);
static GParamSpec *gst_pylon_make_spec_double(GenApi::INodeMap &nodemap,
                                              GenApi::INode *node,
                                              GstPylonCache *feature_cache);
static GParamSpec *gst_pylon_make_spec_selector_double(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, GstPylonCache *feature_cache);
static GParamSpec *gst_pylon_make_spec_str(GenApi::INodeMap &nodemap,
                                           GenApi::INode *node);
static GParamSpec *gst_pylon_make_spec_selector_str(GenApi::INodeMap &nodemap,
                                                    GenApi::INode *node,
                                                    GenApi::INode *selector,
                                                    guint64 selector_value);
static GType gst_pylon_make_enum_type(GenApi::INodeMap &nodemap,
                                      GenApi::INode *node,
                                      const std::string &device_fullname);
static GParamSpec *gst_pylon_make_spec_enum(GenApi::INodeMap &nodemap,
                                            GenApi::INode *node,
                                            const std::string &device_fullname);
static GParamSpec *gst_pylon_make_spec_selector_enum(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, const std::string &device_fullname);
static GenApi::INode *gst_pylon_find_limit_node(GenApi::INode *feature_node,
                                                const GenICam::gcstring &limit);
static std::vector<GenApi::INode *> gst_pylon_find_parent_features(
    GenApi::INode *feature_node);
static void gst_pylon_add_all_property_values(
    GenApi::INode *feature_node, std::string value,
    std::unordered_map<std::string, GenApi::INode *> &invalidators);
static std::vector<GenApi::INode *> gst_pylon_get_available_features(
    const std::set<GenApi::INode *> &feature_list);
template <class Type>
static std::vector<std::vector<Type>> gst_pylon_cartesian_product(
    std::vector<std::vector<Type>> &v);
template <class P, class T>
static T gst_pylon_check_for_feature_invalidators(
    GenApi::INode *feature_node, GenApi::INode *limit_node, std::string limit,
    std::unordered_map<std::string, GenApi::INode *> &invalidators);
template <class P, class T>
static T gst_pylon_query_feature_limits(GenApi::INode *feature_node,
                                        const std::string &limit);
static std::vector<std::vector<GstPylonActions *>>
gst_pylon_create_set_value_actions(
    const std::vector<GenApi::INode *> &node_list);
template <class P, class T>
static void gst_pylon_find_limits(
    GenApi::INode *node, double &minimum_under_all_settings,
    double &maximum_under_all_settings,
    std::vector<GenApi::INode *> &invalidators_result);
template <class T>
static std::string gst_pylon_build_cache_value_string(
    GParamFlags flags, T minimum_under_all_settings,
    T maximum_under_all_settings);

static void gst_pylon_query_feature_properties_double(
    GenApi::INodeMap &nodemap, GenApi::INode *node,
    GstPylonCache *feature_cache, GParamFlags &flags,
    gdouble &minimum_under_all_settings, gdouble &maximum_under_all_settings,
    GenApi::INode *selector = NULL, gint64 selector_value = 0);

static void gst_pylon_query_feature_properties_integer(
    GenApi::INodeMap &nodemap, GenApi::INode *node,
    GstPylonCache *feature_cache, GParamFlags &flags,
    gint64 &minimum_under_all_settings, gint64 &maximum_under_all_settings,
    GenApi::INode *selector = NULL, gint64 selector_value = 0);

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
    Pylon::CIntegerParameter tl_params_locked(nodemap, "TLParamsLocked");
    if (tl_params_locked.IsValid()) {
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

static GenApi::INode *gst_pylon_find_limit_node(
    GenApi::INode *node, const GenICam::gcstring &limit) {
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
    const std::set<GenApi::INode *> &feature_list) {
  std::vector<GenApi::INode *> available_features;
  for (const auto &feature : feature_list) {
    if (GenApi::IsImplemented(feature)) {
      available_features.push_back(feature);
    }
  }
  return available_features;
}

template <class Type>
static std::vector<std::vector<Type>> gst_pylon_cartesian_product(
    std::vector<std::vector<Type>> &values) {
  std::vector<std::vector<Type>> result;
  auto product = [](long long a, std::vector<Type> &b) { return a * b.size(); };
  const long long N = accumulate(values.begin(), values.end(), 1LL, product);
  std::vector<Type> result_combination(values.size());
  for (long long n = 0; n < N; ++n) {
    lldiv_t div_result{n, 0};
    for (long long i = values.size() - 1; 0 <= i; --i) {
      div_result = div(div_result.quot, values[i].size());
      result_combination[i] = values[i][div_result.rem];
    }
    result.push_back(result_combination);
  }
  return result;
}

template <class P, class T>
static T gst_pylon_query_feature_limits(GenApi::INode *node,
                                        const std::string &limit) {
  g_return_val_if_fail(node, 0);

  P param(node);

  if ("max" == limit) {
    return param.GetMax();
  } else {
    return param.GetMin();
  }
}

template <class P, class T>
static T gst_pylon_check_for_feature_invalidators(
    GenApi::INode *node, GenApi::INode *limit_node, std::string limit,
    std::unordered_map<std::string, GenApi::INode *> &invalidators) {
  T limit_under_all_settings = 0;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  g_return_val_if_fail(node, 0);

  if (!limit_node ||
      !limit_node->GetProperty("pInvalidator", value, attribute)) {
    limit_under_all_settings =
        gst_pylon_query_feature_limits<P, T>(node, limit);
  } else {
    limit_node->GetProperty("pInvalidator", value, attribute);
    gst_pylon_add_all_property_values(node, std::string(value), invalidators);
  }

  return limit_under_all_settings;
}

static std::vector<std::vector<GstPylonActions *>>
gst_pylon_create_set_value_actions(
    const std::vector<GenApi::INode *> &node_list) {
  std::vector<std::vector<GstPylonActions *>> actions_list;

  for (const auto &node : node_list) {
    std::vector<GstPylonActions *> values;
    switch (node->GetPrincipalInterfaceType()) {
      case GenApi::intfIBoolean: {
        Pylon::CBooleanParameter param(node);
        values.push_back(
            new GstPylonTypeAction<Pylon::CBooleanParameter, gboolean>(param,
                                                                       TRUE));
        values.push_back(
            new GstPylonTypeAction<Pylon::CBooleanParameter, gboolean>(param,
                                                                       FALSE));
        break;
      }
      case GenApi::intfIFloat: {
        Pylon::CFloatParameter param(node);
        values.push_back(
            new GstPylonTypeAction<Pylon::CFloatParameter, gdouble>(
                param, param.GetMin()));
        values.push_back(
            new GstPylonTypeAction<Pylon::CFloatParameter, gdouble>(
                param, param.GetMax()));
        break;
      }
      case GenApi::intfIInteger: {
        Pylon::CIntegerParameter param(node);
        values.push_back(
            new GstPylonTypeAction<Pylon::CIntegerParameter, gint64>(
                param, param.GetMin()));
        values.push_back(
            new GstPylonTypeAction<Pylon::CIntegerParameter, gint64>(
                param, param.GetMax()));
        break;
      }
      case GenApi::intfIEnumeration: {
        Pylon::CEnumParameter param(node);
        GenApi::StringList_t settable_values;
        param.GetSettableValues(settable_values);
        for (const auto &value : settable_values) {
          /* Skip unsupported packed mono and bayer formats */
          if (node->GetName() == "PixelFormat" &&
              (Pylon::IsMonoPacked(static_cast<Pylon::EPixelType>(
                   param.GetEntryByName(value)->GetValue())) ||
               Pylon::IsBayerPacked(static_cast<Pylon::EPixelType>(
                   param.GetEntryByName(value)->GetValue()))))
            continue;
          values.push_back(
              new GstPylonTypeAction<Pylon::CEnumParameter, Pylon::String_t>(
                  param, value));
        }
        break;
      }
      default:
        std::string msg = "Action in unsupported for node of type " +
                          std::to_string(node->GetPrincipalInterfaceType());
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
    actions_list.push_back(values);
  }

  return actions_list;
}

static std::vector<GstPylonActions *> gst_pylon_create_reset_value_actions(
    const std::vector<GenApi::INode *> &node_list) {
  std::vector<GstPylonActions *> actions_list;

  for (const auto &node : node_list) {
    std::vector<GstPylonActions *> values;
    switch (node->GetPrincipalInterfaceType()) {
      case GenApi::intfIBoolean: {
        Pylon::CBooleanParameter param(node);
        actions_list.push_back(
            new GstPylonTypeAction<Pylon::CBooleanParameter, gboolean>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfIFloat: {
        Pylon::CFloatParameter param(node);
        actions_list.push_back(
            new GstPylonTypeAction<Pylon::CFloatParameter, gdouble>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfIInteger: {
        Pylon::CIntegerParameter param(node);
        actions_list.push_back(
            new GstPylonTypeAction<Pylon::CIntegerParameter, gint64>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfIEnumeration: {
        Pylon::CEnumParameter param(node);
        actions_list.push_back(
            new GstPylonTypeAction<Pylon::CEnumParameter, Pylon::String_t>(
                param, param.GetValue()));
        break;
      }
      default:
        std::string msg = "Action in unsupported for node of type " +
                          std::to_string(node->GetPrincipalInterfaceType());
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  }

  return actions_list;
}

template <class P, class T>
static void gst_pylon_find_limits(GenApi::INode *node,
                                  T &minimum_under_all_settings,
                                  T &maximum_under_all_settings) {
  std::unordered_map<std::string, GenApi::INode *> invalidators;
  maximum_under_all_settings = 0;
  minimum_under_all_settings = 0;

  g_return_if_fail(node);

  /* Find the maximum value of a feature under the influence of other elements
   * of the nodemap */
  GenApi::INode *pmax_node = gst_pylon_find_limit_node(node, "pMax");
  maximum_under_all_settings = gst_pylon_check_for_feature_invalidators<P, T>(
      node, pmax_node, "max", invalidators);

  /* Find the minimum value of a feature under the influence of other elements
   * of the nodemap */
  GenApi::INode *pmin_node = gst_pylon_find_limit_node(node, "pMin");
  minimum_under_all_settings = gst_pylon_check_for_feature_invalidators<P, T>(
      node, pmin_node, "min", invalidators);

  /* Return if no invalidator nodes found */
  if (invalidators.empty()) {
    return;
  }

  /* Find all features that control the node and
   * store results in a set to remove duplicates*/
  std::set<GenApi::INode *> parent_invalidators;
  for (const auto &inv : invalidators) {
    std::vector<GenApi::INode *> parent_features =
        gst_pylon_find_parent_features(inv.second);
    for (const auto &p_feat : parent_features) {
      parent_invalidators.insert(p_feat);
    }
  }

  /* Filter parent invalidators to only available ones */
  std::vector<GenApi::INode *> available_parent_inv =
      gst_pylon_get_available_features(parent_invalidators);

  /* Save current set of values */
  std::vector<GstPylonActions *> reset_list =
      gst_pylon_create_reset_value_actions(available_parent_inv);

  /* Create list of extreme value settings per invalidator */
  std::vector<std::vector<GstPylonActions *>> actions_list =
      gst_pylon_create_set_value_actions(available_parent_inv);

  /* Create list of all possible setting permutations and execute them all */
  auto action_list_permutations = gst_pylon_cartesian_product(actions_list);
  std::vector<T> min_values;
  std::vector<T> max_values;
  for (const auto &actions : action_list_permutations) {
    for (const auto &action : actions) {
      /* Some states might not be valid, so just skip them */
      try {
        action->set_value();
      } catch (const Pylon::GenericException &) {
        continue;
      }
    }
    /* Capture min and max values after all setting are applied*/
    min_values.push_back(gst_pylon_query_feature_limits<P, T>(node, "min"));
    max_values.push_back(gst_pylon_query_feature_limits<P, T>(node, "max"));
  }

  /* Get the max and min values under all settings executed*/
  minimum_under_all_settings =
      *std::min_element(min_values.begin(), min_values.end());
  maximum_under_all_settings =
      *std::max_element(max_values.begin(), max_values.end());

  /* Reset to old values */
  for (const auto &action : reset_list) {
    try {
      action->set_value();
    } catch (const Pylon::GenericException &) {
      continue;
    }
  }

  /* Clean up */
  for (const auto &actions : actions_list) {
    for (const auto &action : actions) {
      delete action;
    }
  }
  for (const auto &action : reset_list) {
    delete action;
  }
}

void gst_pylon_query_feature_properties_double(
    GenApi::INodeMap &nodemap, GenApi::INode *node,
    GstPylonCache *feature_cache, GParamFlags &flags,
    gdouble &minimum_under_all_settings, gdouble &maximum_under_all_settings,
    GenApi::INode *selector, gint64 selector_value) {
  g_return_if_fail(node);
  g_return_if_fail(feature_cache);

  gchar *feature_cache_name = NULL;
  if (selector) {
    /* Set selector value value */
    gst_pylon_object_set_pylon_selector(nodemap, selector->GetName().c_str(),
                                        selector_value);

    feature_cache_name = gst_pylon_create_selected_name(
        nodemap, node->GetName().c_str(), selector->GetName().c_str(),
        selector_value);
  } else {
    feature_cache_name = g_strdup(node->GetName().c_str());
  }

  /* If access to a feature cache entry fails, create new props dynamically */
  if (!feature_cache->GetDoubleProps(feature_cache_name,
                                     minimum_under_all_settings,
                                     maximum_under_all_settings, flags)) {
    flags = gst_pylon_query_access(nodemap, node);
    gst_pylon_find_limits<Pylon::CFloatParameter, gdouble>(
        node, minimum_under_all_settings, maximum_under_all_settings);

    feature_cache->SetDoubleProps(feature_cache_name,
                                  minimum_under_all_settings,
                                  maximum_under_all_settings, flags);
  }

  g_free(feature_cache_name);
}

void gst_pylon_query_feature_properties_integer(
    GenApi::INodeMap &nodemap, GenApi::INode *node,
    GstPylonCache *feature_cache, GParamFlags &flags,
    gint64 &minimum_under_all_settings, gint64 &maximum_under_all_settings,
    GenApi::INode *selector, gint64 selector_value) {
  g_return_if_fail(node);
  g_return_if_fail(feature_cache);

  gchar *feature_cache_name = NULL;
  if (selector) {
    /* Set selector value value */
    gst_pylon_object_set_pylon_selector(nodemap, selector->GetName().c_str(),
                                        selector_value);

    feature_cache_name = gst_pylon_create_selected_name(
        nodemap, node->GetName().c_str(), selector->GetName().c_str(),
        selector_value);

  } else {
    feature_cache_name = g_strdup(node->GetName().c_str());
  }

  /* If access to a feature cache entry fails, create new props dynamically */
  if (!feature_cache->GetIntProps(node->GetName().c_str(),
                                  minimum_under_all_settings,
                                  maximum_under_all_settings, flags)) {
    flags = gst_pylon_query_access(nodemap, node);
    gst_pylon_find_limits<Pylon::CIntegerParameter, gint64>(
        node, minimum_under_all_settings, maximum_under_all_settings);

    feature_cache->SetIntProps(node->GetName().c_str(),
                               minimum_under_all_settings,
                               maximum_under_all_settings, flags);
  }

  g_free(feature_cache_name);
}

static GParamSpec *gst_pylon_make_spec_int64(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node,
                                             GstPylonCache *feature_cache) {
  g_return_val_if_fail(node, NULL);

  Pylon::CIntegerParameter param(node);
  gint64 max_value = 0;
  gint64 min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_integer(nodemap, node, feature_cache,
                                             flags, min_value, max_value);

  return g_param_spec_int64(node->GetName(), node->GetDisplayName(),
                            node->GetToolTip(), min_value, max_value,
                            param.GetValue(), flags);
}

static GParamSpec *gst_pylon_make_spec_selector_int64(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, GstPylonCache *feature_cache) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CIntegerParameter param(node);
  gint64 max_value = 0;
  gint64 min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_integer(nodemap, node, feature_cache,
                                             flags, min_value, max_value,
                                             selector, selector_value);

  return gst_pylon_param_spec_selector_int64(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), min_value, max_value,
      param.GetValue(), flags);
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

  return gst_pylon_param_spec_selector_boolean(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_double(GenApi::INodeMap &nodemap,
                                              GenApi::INode *node,
                                              GstPylonCache *feature_cache) {
  g_return_val_if_fail(node, NULL);

  Pylon::CFloatParameter param(node);
  gdouble max_value = 0;
  gdouble min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_double(nodemap, node, feature_cache, flags,
                                            min_value, max_value);

  return g_param_spec_double(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), min_value, max_value,
                             param.GetValue(), flags);
}

static GParamSpec *gst_pylon_make_spec_selector_double(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, GstPylonCache *feature_cache) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CFloatParameter param(node);
  gdouble max_value = 0;
  gdouble min_value = 0;
  GParamFlags flags = G_PARAM_READABLE;

  gst_pylon_query_feature_properties_double(nodemap, node, feature_cache, flags,
                                            min_value, max_value, selector,
                                            selector_value);

  return gst_pylon_param_spec_selector_double(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), min_value, max_value,
      param.GetValue(), flags);
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

  return gst_pylon_param_spec_selector_string(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(nodemap, node));
}

static GType gst_pylon_make_enum_type(GenApi::INodeMap &nodemap,
                                      GenApi::INode *node,
                                      const std::string &device_fullname) {
  /* When registering enums to the GType system, their string pointers
     must remain valid throughout the application lifespan. To achieve this
     we are saving all found enums into a static hash table
  */
  static std::unordered_map<GType, std::vector<GEnumValue>> persistent_values;

  g_return_val_if_fail(node, G_TYPE_INVALID);

  Pylon::CEnumParameter param(node);

  gchar *full_name = g_strdup_printf("%s_%s", device_fullname.c_str(),
                                     node->GetName().c_str());
  std::string name = gst_pylon_param_spec_sanitize_name(full_name);
  g_free(full_name);

  GType type = g_type_from_name(name.c_str());

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

    type = g_enum_register_static(name.c_str(), enumvalues.data());
    persistent_values.insert({type, std::move(enumvalues)});
  }

  return type;
}

static GParamSpec *gst_pylon_make_spec_enum(
    GenApi::INodeMap &nodemap, GenApi::INode *node,
    const std::string &device_fullname) {
  g_return_val_if_fail(node, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(nodemap, node, device_fullname.c_str());

  return g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                           node->GetToolTip(), type, param.GetIntValue(),
                           gst_pylon_query_access(nodemap, node));
}

static GParamSpec *gst_pylon_make_spec_selector_enum(
    GenApi::INodeMap &nodemap, GenApi::INode *node, GenApi::INode *selector,
    guint64 selector_value, const std::string &device_fullname) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(nodemap, node, device_fullname.c_str());

  return gst_pylon_param_spec_selector_enum(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), type, param.GetIntValue(),
      gst_pylon_query_access(nodemap, node));
}

GParamSpec *GstPylonParamFactory::make_param(GenApi::INodeMap &nodemap,
                                             GenApi::INode *node,
                                             GenApi::INode *selector,
                                             guint64 selector_value,
                                             const std::string &device_fullname,
                                             GstPylonCache *feature_cache) {
  g_return_val_if_fail(node, NULL);

  GParamSpec *spec = NULL;
  GenApi::EInterfaceType iface = node->GetPrincipalInterfaceType();

  switch (iface) {
    case GenApi::intfIInteger:
      if (!selector) {
        spec = gst_pylon_make_spec_int64(nodemap, node, feature_cache);
      } else {
        spec = gst_pylon_make_spec_selector_int64(
            nodemap, node, selector, selector_value, feature_cache);
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
        spec = gst_pylon_make_spec_double(nodemap, node, feature_cache);
      } else {
        spec = gst_pylon_make_spec_selector_double(
            nodemap, node, selector, selector_value, feature_cache);
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
      Pylon::String_t msg =
          "Unsupported node of type " + GenApi::GetInterfaceName(node);
      throw Pylon::GenericException(msg, __FILE__, __LINE__);
  }

  return spec;
}
