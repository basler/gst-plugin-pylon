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
#include "gstpylonlimitprobe.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::size_t MAX_LIMIT_PROBE_PERMUTATIONS = 256;

class GstPylonActions {
 public:
  virtual void set_value() = 0;
  virtual ~GstPylonActions() = default;
};

template <class P, class V>
class GstPylonTypeAction : public GstPylonActions {
 public:
  GstPylonTypeAction(P param, V value)
      : param_(std::move(param)), value_(std::move(value)) {}
  void set_value() override { param_.SetValue(value_); }

 private:
  P param_;
  V value_;
};

class TimeLogger {
 public:
  explicit TimeLogger(const std::string& message)
      : t0_(std::chrono::system_clock::now()), message_(message) {
    GST_DEBUG("Start Checking: %s ", message_.c_str());
  }

  ~TimeLogger() {
    using namespace std::literals;
    auto t1 = std::chrono::system_clock::now();
    GST_DEBUG("TIMELOGGER %s %ld msec -> ", message_.c_str(),
              static_cast<long>((t1 - t0_) / 1ms));
    for (const auto& info : info_list_) {
      GST_DEBUG("%s ", info.c_str());
    }
  }

  void add_info(const std::string& info) { info_list_.push_back(info); }

 private:
  const std::chrono::time_point<std::chrono::system_clock> t0_;
  std::string message_;
  std::vector<std::string> info_list_;
};

GenApi::INode* find_limit_node(GenApi::INode* node,
                               const GenICam::gcstring& limit) {
  GenApi::INode* limit_node = nullptr;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  g_return_val_if_fail(node, nullptr);

  if (node->GetProperty(limit, value, attribute)) {
    limit_node = node->GetNodeMap()->GetNode(value);
  } else if (node->GetProperty("pValue", value, attribute)) {
    limit_node = find_limit_node(node->GetNodeMap()->GetNode(value), limit);
  } else if (node->GetProperty("pValueDefault", value, attribute)) {
    limit_node = find_limit_node(node->GetNodeMap()->GetNode(value), limit);
  }
  return limit_node;
}

std::string find_node_category(GenApi::INode* node) {
  std::string category;
  g_return_val_if_fail(node, category);

  GenApi::INode* curr_node = node;
  while (true) {
    if (curr_node->IsFeature() &&
        curr_node->GetPrincipalInterfaceType() == GenApi::intfICategory) {
      category = curr_node->GetName();
      break;
    }
    GenApi::NodeList_t parents;
    curr_node->GetParents(parents);
    if (parents.empty()) {
      break;
    }
    curr_node = parents[0];
  }
  return category;
}

std::vector<GenApi::INode*> find_parent_features(GenApi::INode* node) {
  std::vector<GenApi::INode*> parent_features;
  g_return_val_if_fail(node, parent_features);

  if (node->IsFeature()) {
    parent_features.push_back(node);
  } else {
    GenApi::NodeList_t parents;
    node->GetParents(parents);
    for (const auto& parent : parents) {
      auto grandparents = find_parent_features(parent);
      parent_features.insert(parent_features.end(), grandparents.begin(),
                             grandparents.end());
    }
  }
  return parent_features;
}

void add_all_property_values(
    GenApi::INode* node, std::string value,
    std::unordered_map<std::string, GenApi::INode*>& invalidators) {
  const std::string delimiter = "\t";
  gsize pos = 0;

  g_return_if_fail(node);

  while ((pos = value.find(delimiter)) != std::string::npos) {
    std::string token = value.substr(0, pos);
    if (invalidators.find(token) == invalidators.end()) {
      invalidators[token] = node->GetNodeMap()->GetNode(token.c_str());
    }
    value.erase(0, pos + delimiter.length());
  }

  if (invalidators.find(value) == invalidators.end()) {
    invalidators[value] = node->GetNodeMap()->GetNode(value.c_str());
  }
}

std::vector<GenApi::INode*> get_available_features(
    const std::set<GenApi::INode*>& feature_list) {
  std::vector<GenApi::INode*> available_features;
  for (const auto& feature : feature_list) {
    if (GenApi::IsImplemented(feature)) {
      available_features.push_back(feature);
    }
  }
  return available_features;
}

std::vector<GenApi::INode*> get_valid_categories(
    const std::vector<GenApi::INode*>& feature_list) {
  std::vector<GenApi::INode*> valid_features;
  for (const auto& feature : feature_list) {
    if (!is_unsupported_category(find_node_category(feature))) {
      valid_features.push_back(feature);
    }
  }
  return valid_features;
}

std::vector<GenApi::INode*> filter_gev_ctrl(
    const std::vector<GenApi::INode*>& feature_list) {
  std::vector<GenApi::INode*> valid_features;
  const std::vector<std::string> gst_feature_list(
      {"GevSCPSPacketSize", "GevSCPD", "GevSCFTD", "GevSCBWR", "GevSCBWRA",
       "GevGVSPExtendedIDMode"});
  for (const auto& feature : feature_list) {
    if (std::find(gst_feature_list.begin(), gst_feature_list.end(),
                  feature->GetName().c_str()) == gst_feature_list.end()) {
      valid_features.push_back(feature);
    }
  }
  return valid_features;
}

template <class Type>
std::vector<std::vector<Type>> cartesian_product_capped(
    const std::vector<std::vector<Type>>& values, const char* feature_name) {
  std::vector<std::vector<Type>> result;
  if (values.empty()) {
    return result;
  }

  std::size_t total = 1;
  bool capped = false;
  for (const auto& group : values) {
    if (group.empty()) {
      return result;
    }
    if (group.size() > MAX_LIMIT_PROBE_PERMUTATIONS / total) {
      total = MAX_LIMIT_PROBE_PERMUTATIONS;
      capped = true;
      break;
    }
    total *= group.size();
  }

  if (capped) {
    GST_DEBUG("Capping limit-probe permutations for '%s' at %zu",
              feature_name ? feature_name : "(unknown)",
              MAX_LIMIT_PROBE_PERMUTATIONS);
  }

  result.reserve(total);
  std::vector<Type> combination(values.size());
  for (std::size_t n = 0; n < total; ++n) {
    std::size_t rem = n;
    for (std::size_t idx = values.size(); idx > 0; --idx) {
      const std::size_t i = idx - 1;
      combination[i] = values[i][rem % values[i].size()];
      rem /= values[i].size();
    }
    result.push_back(combination);
  }
  return result;
}

template <class P, class T>
T query_feature_limit(GenApi::INode* node, const std::string& limit) {
  g_return_val_if_fail(node, 0);

  P param(node);
  return ("max" == limit) ? param.GetMax() : param.GetMin();
}

template <class P, class T>
T check_for_feature_invalidators(
    GenApi::INode* node, GenApi::INode* limit_node, const std::string& limit,
    std::unordered_map<std::string, GenApi::INode*>& invalidators) {
  T limit_under_all_settings = 0;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  g_return_val_if_fail(node, 0);

  if (!limit_node ||
      !limit_node->GetProperty("pInvalidator", value, attribute)) {
    limit_under_all_settings = query_feature_limit<P, T>(node, limit);
  } else {
    limit_node->GetProperty("pInvalidator", value, attribute);
    add_all_property_values(node, std::string(value), invalidators);
  }

  return limit_under_all_settings;
}

std::vector<std::vector<std::unique_ptr<GstPylonActions>>>
create_set_value_actions(const std::vector<GenApi::INode*>& node_list) {
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
      case GenApi::intfICommand:
        continue;
      default:
        GST_DEBUG("No test for node %s", node->GetName().c_str());
        continue;
    }
    if (!values.empty()) {
      actions_list.push_back(std::move(values));
    }
  }

  return actions_list;
}

std::vector<std::unique_ptr<GstPylonActions>> create_reset_value_actions(
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
      case GenApi::intfICommand:
        continue;
      default:
        GST_DEBUG("No test for node %s", node->GetName().c_str());
        continue;
    }
  }

  return actions_list;
}

template <class P, class T>
void find_limits(GenApi::INode* node, T& minimum_under_all_settings,
                 T& maximum_under_all_settings,
                 GstPylonDeviceProfile device_profile) {
  std::unordered_map<std::string, GenApi::INode*> invalidators;
  maximum_under_all_settings = 0;
  minimum_under_all_settings = 0;
  g_return_if_fail(node);

  auto tl = TimeLogger(node->GetName().c_str());

  if (device_profile == GstPylonDeviceProfile::kBaslerGen2 &&
      gst_pylon_try_prior_limits(device_profile, node,
                                 minimum_under_all_settings,
                                 maximum_under_all_settings)) {
    return;
  }

  GenApi::INode* pmax_node = find_limit_node(node, "pMax");
  maximum_under_all_settings = check_for_feature_invalidators<P, T>(
      node, pmax_node, "max", invalidators);

  GenApi::INode* pmin_node = find_limit_node(node, "pMin");
  minimum_under_all_settings = check_for_feature_invalidators<P, T>(
      node, pmin_node, "min", invalidators);

  if (invalidators.empty()) {
    return;
  }

  if (device_profile != GstPylonDeviceProfile::kBaslerGen2 &&
      gst_pylon_try_prior_limits(device_profile, node,
                                 minimum_under_all_settings,
                                 maximum_under_all_settings)) {
    return;
  }

  std::set<GenApi::INode*> parent_invalidators;
  for (const auto& inv : invalidators) {
    auto parent_features = find_parent_features(inv.second);
    parent_invalidators.insert(parent_features.begin(), parent_features.end());
  }

  auto available_parent_inv = get_available_features(parent_invalidators);
  available_parent_inv = get_valid_categories(available_parent_inv);
  available_parent_inv = filter_gev_ctrl(available_parent_inv);

  for (auto& inv_node : available_parent_inv) {
    tl.add_info(inv_node->GetName().c_str());
  }

  auto reset_list = create_reset_value_actions(available_parent_inv);
  auto actions_list = create_set_value_actions(available_parent_inv);

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

  auto reg_streaming_start = Pylon::CCommandParameter(
      node->GetNodeMap()->GetNode("DeviceRegistersStreamingStart"));
  auto reg_streaming_end = Pylon::CCommandParameter(
      node->GetNodeMap()->GetNode("DeviceRegistersStreamingEnd"));

  auto action_list_permutations =
      cartesian_product_capped(raw_actions_list, node->GetName().c_str());
  std::vector<T> min_values;
  std::vector<T> max_values;
  for (const auto& actions : action_list_permutations) {
    reg_streaming_start.TryExecute();
    for (const auto& action : actions) {
      try {
        action->set_value();
      } catch (const GenICam::GenericException&) {
        GST_DEBUG("failed to set action");
        continue;
      }
    }

    reg_streaming_end.TryExecute();

    min_values.push_back(query_feature_limit<P, T>(node, "min"));
    max_values.push_back(query_feature_limit<P, T>(node, "max"));
  }

  if (!min_values.empty() && !max_values.empty()) {
    minimum_under_all_settings =
        *std::min_element(min_values.begin(), min_values.end());
    maximum_under_all_settings =
        *std::max_element(max_values.begin(), max_values.end());
  } else {
    minimum_under_all_settings = query_feature_limit<P, T>(node, "min");
    maximum_under_all_settings = query_feature_limit<P, T>(node, "max");
  }

  for (const auto& action : reset_list) {
    try {
      action->set_value();
    } catch (const Pylon::GenericException&) {
      continue;
    }
  }
}

}  // namespace

gboolean gst_pylon_try_prior_limits(GstPylonDeviceProfile device_profile,
                                    GenApi::INode* node,
                                    gint64& minimum_under_all_settings,
                                    gint64& maximum_under_all_settings) {
  return gst_pylon_prior_resolve_int_limits(device_profile, node,
                                            minimum_under_all_settings,
                                            maximum_under_all_settings);
}

gboolean gst_pylon_try_prior_limits(GstPylonDeviceProfile device_profile,
                                    GenApi::INode* node,
                                    gdouble& minimum_under_all_settings,
                                    gdouble& maximum_under_all_settings) {
  return gst_pylon_prior_resolve_double_limits(device_profile, node,
                                               minimum_under_all_settings,
                                               maximum_under_all_settings);
}

gint64 gst_pylon_query_integer_limit(GenApi::INode* node,
                                     const std::string& limit) {
  return query_feature_limit<Pylon::CIntegerParameter, gint64>(node, limit);
}

gdouble gst_pylon_query_double_limit(GenApi::INode* node,
                                     const std::string& limit) {
  return query_feature_limit<Pylon::CFloatParameter, gdouble>(node, limit);
}

void gst_pylon_probe_integer_limits(GenApi::INode* node,
                                    gint64& minimum_under_all_settings,
                                    gint64& maximum_under_all_settings,
                                    GstPylonDeviceProfile device_profile) {
  find_limits<Pylon::CIntegerParameter, gint64>(
      node, minimum_under_all_settings, maximum_under_all_settings,
      device_profile);
}

void gst_pylon_probe_double_limits(GenApi::INode* node,
                                   gdouble& minimum_under_all_settings,
                                   gdouble& maximum_under_all_settings,
                                   GstPylonDeviceProfile device_profile) {
  find_limits<Pylon::CFloatParameter, gdouble>(node, minimum_under_all_settings,
                                               maximum_under_all_settings,
                                               device_profile);
}
