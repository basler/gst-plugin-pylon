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
#include "gstpylonformatmapping.h"
#include "gstpylonintrospection.h"
#include "gstpylonobject.h"
#include "gstpylonparamspecs.h"

#include <pylon/PixelType.h>

#include <algorithm>
#include <chrono>
#include <memory>
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
GenApi::INode* gst_pylon_find_limit_node(GenApi::INode* feature_node,
                                         const GenICam::gcstring& limit);
static std::vector<GenApi::INode*> gst_pylon_find_parent_features(
    GenApi::INode* feature_node);
void gst_pylon_add_all_property_values(
    GenApi::INode* feature_node, std::string value,
    std::unordered_map<std::string, GenApi::INode*>& invalidators);
std::vector<GenApi::INode*> gst_pylon_get_available_features(
    const std::set<GenApi::INode*>& feature_list);
template <class Type>
std::vector<std::vector<Type>> gst_pylon_cartesian_product(
    std::vector<std::vector<Type>>& v);
template <class P, class T>
T gst_pylon_check_for_feature_invalidators(
    GenApi::INode* feature_node, GenApi::INode* limit_node, std::string limit,
    std::unordered_map<std::string, GenApi::INode*>& invalidators);
template <class P, class T>
T gst_pylon_query_feature_limits(GenApi::INode* feature_node,
                                 const std::string& limit);
std::vector<std::vector<std::unique_ptr<GstPylonActions>>>
gst_pylon_create_set_value_actions(
    const std::vector<GenApi::INode*>& node_list);
template <class P, class T>
void gst_pylon_find_limits(GenApi::INode* node, T& minimum_under_all_settings,
                           T& maximum_under_all_settings,
                           GstPylonDeviceProfile device_profile);
template <class T>
std::string gst_pylon_build_cache_value_string(GParamFlags flags,
                                               T minimum_under_all_settings,
                                               T maximum_under_all_settings);

gboolean gst_pylon_can_feature_later_be_writable(GenApi::INode* node);

std::vector<std::unique_ptr<GstPylonActions>>
gst_pylon_create_reset_value_actions(
    const std::vector<GenApi::INode*>& node_list);

gboolean gst_pylon_can_feature_later_be_writable(GenApi::INode* node) {
  GenICam::gcstring value;
  GenICam::gcstring attribute;
  if (node->GetProperty("pIsLocked", value, attribute)) {
    return true;
  } else {
    GenApi::NodeList_t node_children;
    node->GetChildren(node_children);
    if (!node_children.empty()) {
      for (auto& node_child : node_children) {
        return gst_pylon_can_feature_later_be_writable(node_child);
      }
    }
    return FALSE;
  }
}

GParamFlags gst_pylon_query_access(GenApi::INodeMap& nodemap,
                                   GenApi::INode* node,
                                   GstPylonDeviceProfile device_profile) {
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
    const GstPylonTlLockBehavior lock_behavior =
        gst_pylon_prior_tl_lock_behavior(device_profile,
                                         node->GetName().c_str());
    if (lock_behavior == GstPylonTlLockBehavior::kUnlockedPlaying) {
      flags |=
          is_writable ? GST_PARAM_MUTABLE_PLAYING : GST_PARAM_MUTABLE_READY;
    } else if (lock_behavior == GstPylonTlLockBehavior::kLockedPlaying) {
      flags |= GST_PARAM_MUTABLE_READY;
    } else {
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
  }

  return static_cast<GParamFlags>(flags);
}

GenApi::INode* gst_pylon_find_limit_node(GenApi::INode* node,
                                         const GenICam::gcstring& limit) {
  GenApi::INode* limit_node = NULL;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  g_return_val_if_fail(node, NULL);

  if (node->GetProperty(limit, value, attribute)) {
    limit_node = node->GetNodeMap()->GetNode(value);
  } else if (node->GetProperty("pValue", value, attribute)) {
    limit_node =
        gst_pylon_find_limit_node(node->GetNodeMap()->GetNode(value), limit);
  } else if (node->GetProperty("pValueDefault", value, attribute)) {
    /* FIXME: pValueDefault might not cover the limits of all selector entries
     * properly
     */
    limit_node =
        gst_pylon_find_limit_node(node->GetNodeMap()->GetNode(value), limit);
  }
  return limit_node;
}

/* identify the first category a node belongs to */
static const std::string gst_pylon_find_node_category(GenApi::INode* node) {
  std::string category = "";
  g_return_val_if_fail(node, category);

  GenApi::INode* curr_node = node;

  while (true) {
    if (curr_node->IsFeature() &&
        curr_node->GetPrincipalInterfaceType() == GenApi::intfICategory) {
      category = curr_node->GetName();
      break;
    } else {
      GenApi::NodeList_t parents;
      curr_node->GetParents(parents);
      if (parents.size() == 0) {
        /* no parents and still no Category found, abort */
        break;
      } else if (parents.size() >= 1) {
        /* yes there are genicam nodes that belong to multiple categories ...
         * but we ignore this here */
        curr_node = parents[0];
      }
    }
  }

  return category;
}

static std::vector<GenApi::INode*> gst_pylon_find_parent_features(
    GenApi::INode* node) {
  std::vector<GenApi::INode*> parent_features;

  g_return_val_if_fail(node, parent_features);

  if (node->IsFeature()) {
    parent_features.push_back(node);
  } else {
    GenApi::NodeList_t parents;
    node->GetParents(parents);
    for (const auto& parent : parents) {
      std::vector<GenApi::INode*> grandparents =
          gst_pylon_find_parent_features(parent);
      parent_features.insert(parent_features.end(), grandparents.begin(),
                             grandparents.end());
    }
  }
  return parent_features;
}

void gst_pylon_add_all_property_values(
    GenApi::INode* node, std::string value,
    std::unordered_map<std::string, GenApi::INode*>& invalidators) {
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

std::vector<GenApi::INode*> gst_pylon_get_available_features(
    const std::set<GenApi::INode*>& feature_list) {
  std::vector<GenApi::INode*> available_features;
  for (const auto& feature : feature_list) {
    if (GenApi::IsImplemented(feature)) {
      available_features.push_back(feature);
    }
  }
  return available_features;
}

static std::vector<GenApi::INode*> gst_pylon_get_valid_categories(
    const std::vector<GenApi::INode*>& feature_list) {
  std::vector<GenApi::INode*> valid_features;
  for (const auto& feature : feature_list) {
    const std::string feature_category = gst_pylon_find_node_category(feature);
    if (!is_unsupported_category(feature_category)) {
      valid_features.push_back(feature);
    }
  }
  return valid_features;
}

static std::vector<GenApi::INode*> gst_pylon_filter_gev_ctrl(
    const std::vector<GenApi::INode*>& feature_list) {
  std::vector<GenApi::INode*> valid_features;
  /* feature to filter */
  const std::vector<std::string> gst_feature_list(
      {"GevSCPSPacketSize", "GevSCPD", "GevSCFTD", "GevSCBWR", "GevSCBWRA",
       "GevGVSPExtendedIDMode"});
  /* filter out gev control features from feature_list */
  for (const auto& feature : feature_list) {
    if (std::find(gst_feature_list.begin(), gst_feature_list.end(),
                  feature->GetName().c_str()) == gst_feature_list.end()) {
      valid_features.push_back(feature);
    }
  }
  return valid_features;
}

template <class Type>
std::vector<std::vector<Type>> gst_pylon_cartesian_product(
    std::vector<std::vector<Type>>& values) {
  std::vector<std::vector<Type>> result;
  auto product = [](long long a, std::vector<Type>& b) { return a * b.size(); };
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
T gst_pylon_query_feature_limits(GenApi::INode* node,
                                 const std::string& limit) {
  g_return_val_if_fail(node, 0);

  P param(node);

  if ("max" == limit) {
    return param.GetMax();
  } else {
    return param.GetMin();
  }
}

template <class P, class T>
T gst_pylon_check_for_feature_invalidators(
    GenApi::INode* node, GenApi::INode* limit_node, std::string limit,
    std::unordered_map<std::string, GenApi::INode*>& invalidators) {
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

std::vector<std::vector<std::unique_ptr<GstPylonActions>>>
gst_pylon_create_set_value_actions(
    const std::vector<GenApi::INode*>& node_list) {
  std::vector<std::vector<std::unique_ptr<GstPylonActions>>> actions_list;

  for (const auto& node : node_list) {
    std::vector<std::unique_ptr<GstPylonActions>> values;
    switch (node->GetPrincipalInterfaceType()) {
      case GenApi::intfIBoolean: {
        Pylon::CBooleanParameter param(node);
        values.push_back(
            std::make_unique<
                GstPylonTypeAction<Pylon::CBooleanParameter, gboolean>>(param,
                                                                        TRUE));
        values.push_back(
            std::make_unique<
                GstPylonTypeAction<Pylon::CBooleanParameter, gboolean>>(param,
                                                                        FALSE));
        break;
      }
      case GenApi::intfIFloat: {
        Pylon::CFloatParameter param(node);
        values.push_back(std::make_unique<
                         GstPylonTypeAction<Pylon::CFloatParameter, gdouble>>(
            param, param.GetMin()));
        values.push_back(std::make_unique<
                         GstPylonTypeAction<Pylon::CFloatParameter, gdouble>>(
            param, param.GetMax()));
        break;
      }
      case GenApi::intfIInteger: {
        Pylon::CIntegerParameter param(node);
        values.push_back(std::make_unique<
                         GstPylonTypeAction<Pylon::CIntegerParameter, gint64>>(
            param, param.GetMin()));
        values.push_back(std::make_unique<
                         GstPylonTypeAction<Pylon::CIntegerParameter, gint64>>(
            param, param.GetMax()));
        break;
      }
      case GenApi::intfIEnumeration: {
        Pylon::CEnumParameter param(node);
        GenApi::StringList_t settable_values;
        param.GetSettableValues(settable_values);
        for (const auto& value : settable_values) {
          /* Skip only check plugin supported formats */
          if (node->GetName() == "PixelFormat" &&
              !isSupportedPylonFormat(value.c_str())) {
            continue;
          }
          values.push_back(
              std::make_unique<
                  GstPylonTypeAction<Pylon::CEnumParameter, Pylon::String_t>>(
                  param, value));
        }
        break;
      }
      case GenApi::intfICommand: {
        /* command node feature modification is ignored */
        continue;
      }
      default:
        std::string msg =
            "No test for node " + std::string(node->GetName().c_str());
        GST_DEBUG("%s", msg.c_str());
        continue;
    }
    if (!values.empty()) {
      actions_list.push_back(std::move(values));
    }
  }

  return actions_list;
}

std::vector<std::unique_ptr<GstPylonActions>>
gst_pylon_create_reset_value_actions(
    const std::vector<GenApi::INode*>& node_list) {
  std::vector<std::unique_ptr<GstPylonActions>> actions_list;

  for (const auto& node : node_list) {
    switch (node->GetPrincipalInterfaceType()) {
      case GenApi::intfIBoolean: {
        Pylon::CBooleanParameter param(node);
        actions_list.push_back(
            std::make_unique<
                GstPylonTypeAction<Pylon::CBooleanParameter, gboolean>>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfIFloat: {
        Pylon::CFloatParameter param(node);
        actions_list.push_back(
            std::make_unique<
                GstPylonTypeAction<Pylon::CFloatParameter, gdouble>>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfIInteger: {
        Pylon::CIntegerParameter param(node);
        actions_list.push_back(
            std::make_unique<
                GstPylonTypeAction<Pylon::CIntegerParameter, gint64>>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfIEnumeration: {
        Pylon::CEnumParameter param(node);
        actions_list.push_back(
            std::make_unique<
                GstPylonTypeAction<Pylon::CEnumParameter, Pylon::String_t>>(
                param, param.GetValue()));
        break;
      }
      case GenApi::intfICommand: {
        /* command node feature modification is ignored */
        continue;
      }
      default:
        std::string msg =
            "No test for node " + std::string(node->GetName().c_str());
        GST_DEBUG("%s", msg.c_str());
        continue;
    }
  }

  return actions_list;
}
using namespace std ::literals;
class TimeLogger {
 public:
  TimeLogger(const std::string& message)
      : t0(std::chrono::system_clock::now()), message(message) {
    GST_DEBUG("Start Checking: %s ", message.c_str());
  };

  ~TimeLogger() {
    auto t1 = std::chrono::system_clock::now();
    GST_DEBUG("TIMELOGGER %s %ld msec -> ", message.c_str(), (t1 - t0) / 1ms);
    for (auto& info : info_list) {
      GST_DEBUG("%s ", info.c_str());
    }
  }

  void add_info(const std::string& info) { info_list.push_back(info); }

 private:
  const std::chrono::time_point<std::chrono::system_clock> t0;
  std::string message;
  std::vector<std::string> info_list;
};

static gboolean gst_pylon_try_prior_limits(GstPylonDeviceProfile device_profile,
                                           GenApi::INode* node,
                                           gint64& minimum_under_all_settings,
                                           gint64& maximum_under_all_settings) {
  return gst_pylon_prior_resolve_int_limits(device_profile, node,
                                            minimum_under_all_settings,
                                            maximum_under_all_settings);
}

static gboolean gst_pylon_try_prior_limits(
    GstPylonDeviceProfile device_profile, GenApi::INode* node,
    gdouble& minimum_under_all_settings, gdouble& maximum_under_all_settings) {
  return gst_pylon_prior_resolve_double_limits(device_profile, node,
                                               minimum_under_all_settings,
                                               maximum_under_all_settings);
}

template <class P, class T>
void gst_pylon_find_limits(GenApi::INode* node, T& minimum_under_all_settings,
                           T& maximum_under_all_settings,
                           GstPylonDeviceProfile device_profile) {
  std::unordered_map<std::string, GenApi::INode*> invalidators;
  maximum_under_all_settings = 0;
  minimum_under_all_settings = 0;
  g_return_if_fail(node);

  auto tl = TimeLogger(node->GetName().c_str());

  /* For profiled second-generation Basler cameras (ace 2, dart 2, boost,
   * racer 2 / 2S), apply prior knowledge directly before walking GenApi
   * invalidators. */
  if (device_profile == GstPylonDeviceProfile::kBaslerGen2 &&
      gst_pylon_try_prior_limits(device_profile, node,
                                 minimum_under_all_settings,
                                 maximum_under_all_settings)) {
    return;
  }

  /* Find the maximum value of a feature under the influence of other elements
   * of the nodemap */
  GenApi::INode* pmax_node = gst_pylon_find_limit_node(node, "pMax");
  maximum_under_all_settings = gst_pylon_check_for_feature_invalidators<P, T>(
      node, pmax_node, "max", invalidators);

  /* Find the minimum value of a feature under the influence of other elements
   * of the nodemap */
  GenApi::INode* pmin_node = gst_pylon_find_limit_node(node, "pMin");
  minimum_under_all_settings = gst_pylon_check_for_feature_invalidators<P, T>(
      node, pmin_node, "min", invalidators);

  /* Return if no invalidator nodes found */
  if (invalidators.empty()) {
    return;
  }

  /* Apply generic workarounds for non-profiled cameras with high invalidator
   * fanout before resorting to Cartesian permutation probing. */
  if (device_profile != GstPylonDeviceProfile::kBaslerGen2 &&
      gst_pylon_try_prior_limits(device_profile, node,
                                 minimum_under_all_settings,
                                 maximum_under_all_settings)) {
    return;
  }

  /* Find all features that control the node and
   * store results in a set to remove duplicates*/
  std::set<GenApi::INode*> parent_invalidators;
  for (const auto& inv : invalidators) {
    std::vector<GenApi::INode*> parent_features =
        gst_pylon_find_parent_features(inv.second);
    for (const auto& p_feat : parent_features) {
      parent_invalidators.insert(p_feat);
    }
  }

  /* Filter parent invalidators to only available ones */
  std::vector<GenApi::INode*> available_parent_inv =
      gst_pylon_get_available_features(parent_invalidators);

  /* remove any feature from the list that belongs to an unsupported
   * category */
  available_parent_inv = gst_pylon_get_valid_categories(available_parent_inv);

  /* workaround for camera features that have an irrelevant dependency on
   * the gige setup parameters */
  available_parent_inv = gst_pylon_filter_gev_ctrl(available_parent_inv);

  for (auto& inv_node : available_parent_inv) {
    tl.add_info(inv_node->GetName().c_str());
  }

  /* Save current set of values (RAII) */
  auto reset_list = gst_pylon_create_reset_value_actions(available_parent_inv);

  /* Create list of extreme value settings per invalidator (RAII) */
  auto actions_list = gst_pylon_create_set_value_actions(available_parent_inv);

  std::vector<std::vector<GstPylonActions*>> raw_actions_list;
  raw_actions_list.reserve(actions_list.size());
  for (const auto& group : actions_list) {
    std::vector<GstPylonActions*> ptrs;
    ptrs.reserve(group.size());
    for (const auto& act : group) {
      ptrs.push_back(act.get());
    }
    raw_actions_list.push_back(std::move(ptrs));
  }

  /* try to get support for optimized bulk feature settings */
  auto reg_streaming_start = Pylon::CCommandParameter(
      node->GetNodeMap()->GetNode("DeviceRegistersStreamingStart"));
  auto reg_streaming_end = Pylon::CCommandParameter(
      node->GetNodeMap()->GetNode("DeviceRegistersStreamingEnd"));

  /* Create list of all possible setting permutations and execute them all */
  auto action_list_permutations = gst_pylon_cartesian_product(raw_actions_list);
  std::vector<T> min_values;
  std::vector<T> max_values;
  for (const auto& actions : action_list_permutations) {
    reg_streaming_start.TryExecute();
    for (const auto& action : actions) {
      /* Some states might not be valid, so just skip them */
      try {
        action->set_value();
      } catch (const GenICam::GenericException&) {
        GST_DEBUG("failed to set action");
        continue;
      }
    }

    reg_streaming_end.TryExecute();

    /* Capture min and max values after all setting are applied*/
    min_values.push_back(gst_pylon_query_feature_limits<P, T>(node, "min"));
    max_values.push_back(gst_pylon_query_feature_limits<P, T>(node, "max"));
  }

  if (!min_values.empty() && !max_values.empty()) {
    minimum_under_all_settings =
        *std::min_element(min_values.begin(), min_values.end());
    maximum_under_all_settings =
        *std::max_element(max_values.begin(), max_values.end());
  } else {
    minimum_under_all_settings =
        gst_pylon_query_feature_limits<P, T>(node, "min");
    maximum_under_all_settings =
        gst_pylon_query_feature_limits<P, T>(node, "max");
  }

  /* Reset to old values */
  for (const auto& action : reset_list) {
    try {
      action->set_value();
    } catch (const Pylon::GenericException&) {
      continue;
    }
  }
}

void gst_pylon_query_feature_properties_double(
    GenApi::INodeMap& nodemap, GenApi::INode* node,
    GstPylonCache& feature_cache, GParamFlags& flags,
    gdouble& minimum_under_all_settings, gdouble& maximum_under_all_settings,
    GenApi::INode* selector, gint64 selector_value,
    GstPylonDeviceProfile device_profile) {
  g_return_if_fail(node);

  gchar* feature_cache_name = NULL;
  if (selector) {
    feature_cache_name = gst_pylon_create_selected_name(
        nodemap, node->GetName().c_str(), selector->GetName().c_str(),
        selector_value);
  } else {
    feature_cache_name = g_strdup(node->GetName().c_str());
  }

  /* If access to a feature cache entry fails, create new props dynamically */
  if (!feature_cache.GetDoubleProps(feature_cache_name,
                                    minimum_under_all_settings,
                                    maximum_under_all_settings, flags)) {
    try {
      if (selector) {
        gst_pylon_object_set_pylon_selector(
            nodemap, selector->GetName().c_str(), selector_value);
      }
      flags = gst_pylon_query_access(nodemap, node, device_profile);
      if (!(flags & (G_PARAM_READABLE | G_PARAM_WRITABLE))) {
        minimum_under_all_settings = 0.0;
        maximum_under_all_settings = 0.0;
      } else if (feature_cache.IsLimitProbeEnabled()) {
        gst_pylon_find_limits<Pylon::CFloatParameter, gdouble>(
            node, minimum_under_all_settings, maximum_under_all_settings,
            device_profile);
      } else if (!gst_pylon_try_prior_limits(device_profile, node,
                                             minimum_under_all_settings,
                                             maximum_under_all_settings)) {
        minimum_under_all_settings =
            gst_pylon_query_feature_limits<Pylon::CFloatParameter, gdouble>(
                node, "min");
        maximum_under_all_settings =
            gst_pylon_query_feature_limits<Pylon::CFloatParameter, gdouble>(
                node, "max");
      }
    } catch (const GenICam::GenericException& e) {
      GST_DEBUG("Caching unavailable float feature '%s': %s",
                feature_cache_name, e.GetDescription());
      flags = static_cast<GParamFlags>(0);
      minimum_under_all_settings = 0.0;
      maximum_under_all_settings = 0.0;
    }

    feature_cache.SetDoubleProps(feature_cache_name, minimum_under_all_settings,
                                 maximum_under_all_settings, flags);
  }

  g_free(feature_cache_name);
}

void gst_pylon_query_feature_properties_flags(
    GenApi::INodeMap& nodemap, GenApi::INode* node,
    GstPylonCache& feature_cache, GParamFlags& flags, GenApi::INode* selector,
    gint64 selector_value, GstPylonDeviceProfile device_profile) {
  g_return_if_fail(node);

  gchar* feature_cache_name = NULL;
  if (selector) {
    feature_cache_name = gst_pylon_create_selected_name(
        nodemap, node->GetName().c_str(), selector->GetName().c_str(),
        selector_value);
  } else {
    feature_cache_name = g_strdup(node->GetName().c_str());
  }

  if (!feature_cache.GetFlags(feature_cache_name, flags)) {
    try {
      if (selector) {
        gst_pylon_object_set_pylon_selector(
            nodemap, selector->GetName().c_str(), selector_value);
      }
      flags = gst_pylon_query_access(nodemap, node, device_profile);
    } catch (const GenICam::GenericException& e) {
      GST_DEBUG("Caching unavailable feature '%s': %s", feature_cache_name,
                e.GetDescription());
      flags = static_cast<GParamFlags>(0);
    }
    feature_cache.SetFlags(feature_cache_name, flags);
  }

  g_free(feature_cache_name);
}

void gst_pylon_query_feature_properties_integer(
    GenApi::INodeMap& nodemap, GenApi::INode* node,
    GstPylonCache& feature_cache, GParamFlags& flags,
    gint64& minimum_under_all_settings, gint64& maximum_under_all_settings,
    GenApi::INode* selector, gint64 selector_value,
    GstPylonDeviceProfile device_profile) {
  g_return_if_fail(node);

  gchar* feature_cache_name = NULL;
  if (selector) {
    feature_cache_name = gst_pylon_create_selected_name(
        nodemap, node->GetName().c_str(), selector->GetName().c_str(),
        selector_value);
  } else {
    feature_cache_name = g_strdup(node->GetName().c_str());
  }

  /* If access to a feature cache entry fails, create new props dynamically */
  if (!feature_cache.GetIntProps(feature_cache_name, minimum_under_all_settings,
                                 maximum_under_all_settings, flags)) {
    try {
      if (selector) {
        gst_pylon_object_set_pylon_selector(
            nodemap, selector->GetName().c_str(), selector_value);
      }
      flags = gst_pylon_query_access(nodemap, node, device_profile);
      if (!(flags & (G_PARAM_READABLE | G_PARAM_WRITABLE))) {
        minimum_under_all_settings = 0;
        maximum_under_all_settings = 0;
      } else if (feature_cache.IsLimitProbeEnabled()) {
        gst_pylon_find_limits<Pylon::CIntegerParameter, gint64>(
            node, minimum_under_all_settings, maximum_under_all_settings,
            device_profile);
      } else if (!gst_pylon_try_prior_limits(device_profile, node,
                                             minimum_under_all_settings,
                                             maximum_under_all_settings)) {
        minimum_under_all_settings =
            gst_pylon_query_feature_limits<Pylon::CIntegerParameter, gint64>(
                node, "min");
        maximum_under_all_settings =
            gst_pylon_query_feature_limits<Pylon::CIntegerParameter, gint64>(
                node, "max");
      }
    } catch (const GenICam::GenericException& e) {
      GST_DEBUG("Caching unavailable integer feature '%s': %s",
                feature_cache_name, e.GetDescription());
      flags = static_cast<GParamFlags>(0);
      minimum_under_all_settings = 0;
      maximum_under_all_settings = 0;
    }

    feature_cache.SetIntProps(feature_cache_name, minimum_under_all_settings,
                              maximum_under_all_settings, flags);
  }

  g_free(feature_cache_name);
}
