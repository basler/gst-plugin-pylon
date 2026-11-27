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
#include "gstpylonparamfactory.h"

#include <string.h>

#include <queue>
#include <unordered_set>

#define MAX_INT_SELECTOR_ENTRIES 16

/* prototypes */
std::vector<std::string> gst_pylon_get_enum_entries(
    GenApi::IEnumeration* enum_node);
std::vector<std::string> gst_pylon_get_int_entries(GenApi::IInteger* int_node);
void gst_pylon_camera_install_specs(const std::vector<GParamSpec*>& specs_list,
                                    GObjectClass* oclass, gint& nprop);
std::vector<GParamSpec*> gst_pylon_camera_handle_node(
    GenApi::INode* node, GstPylonParamFactory& param_factory);

static const std::unordered_set<std::string> propfilter_set = {
    "Width",
    "Height",
    "PixelFormat",
    "AcquisitionStart",
    "AcquisitionStop",
    "UserSetLoad",
    "UserSetSave",
    "TriggerSoftware",
    "DeviceReset",
    "DeviceFeaturePersistenceStart",
    "DeviceFeaturePersistenceEnd",
    "DeviceRegistersStreamingStart",
    "DeviceRegistersStreamingEnd",
};

static const std::unordered_set<std::string> categoryfilter_set = {
    "ChunkData",
    "FileAccessControl", /* has to be implemented in access library */
    "EventControl",      /* disable full event section until mapped to gst
                            events/msgs */
    "SequenceControl",   /* sequencer control relies on cmd feature */
    "SequencerControl",  /* sequencer control relies on cmd feature */
    "MultipleROI",       /* FIXME: workaround skip because feature walker
                            currently supports only one selector per node;
                            MultipleROI features typically have more. */
};

/* filter for selector nodes */
std::unordered_set<std::string> selectorfilter_set = {
    "DeviceLinkSelector",
};

/* filter for features that are not supported */
bool is_unsupported_feature(const std::string& feature_name) {
  /* check on exception list */
  return propfilter_set.find(feature_name) != propfilter_set.end();
}

/* filter for categories that are not supported */
bool is_unsupported_category(const std::string& category_name) {
  bool res = categoryfilter_set.find(category_name) != categoryfilter_set.end();

  if (!res) {
    /* check on end pattern to cover events */
    std::string event_suffix = "EventData";
    if (category_name.length() >= event_suffix.length()) {
      res |= 0 == category_name.compare(
                      category_name.length() - event_suffix.length(),
                      event_suffix.length(), event_suffix);
    }
  }

  return res;
}

/* filter for selectors and categories that are supported */
bool is_unsupported_selector(const std::string& feature_name) {
  return selectorfilter_set.find(feature_name) != selectorfilter_set.end();
}

std::vector<std::string> gst_pylon_get_enum_entries(
    GenApi::IEnumeration* enum_node) {
  GenApi::NodeList_t enum_entries;
  std::vector<std::string> entry_names;

  g_return_val_if_fail(enum_node, entry_names);

  /* Calculate prefix length to strip */
  const auto prefix_str = std::string("EnumEntry_") +
                          enum_node->GetNode()->GetName().c_str() +
                          std::string("_");
  auto prefix_len = prefix_str.length();

  /* Add enum values */
  enum_node->GetEntries(enum_entries);

  for (auto const& e : enum_entries) {
    if (!GenApi::IsImplemented(e)) {
      /* Skip all entries that don't exist on this device */
      continue;
    }
    auto enum_name = std::string(e->GetName());
    entry_names.push_back(enum_name.substr(prefix_len));
  }

  return entry_names;
}

std::vector<std::string> gst_pylon_get_int_entries(GenApi::IInteger* int_node) {
  std::vector<std::string> entry_names;

  g_return_val_if_fail(int_node, entry_names);

  const int64_t min_val = int_node->GetMin();
  const int64_t max_val = int_node->GetMax();
  const int64_t inc_val = int_node->GetInc();
  if (inc_val <= 0 || min_val > max_val) {
    return entry_names;
  }

  for (int64_t i = min_val; i <= max_val; i += inc_val) {
    /* Limit integer based selectors to MAX_INT_SELECTOR_ENTRIES entries */
    if (entry_names.size() >= static_cast<size_t>(MAX_INT_SELECTOR_ENTRIES)) {
      break;
    }
    entry_names.push_back(std::to_string(i));
    if (i > max_val - inc_val) {
      break;
    }
  }

  return entry_names;
}

std::vector<std::string> GstPylonFeatureWalker::process_selector_features(
    GenApi::INode* node, GenApi::INode** selector_node) {
  std::vector<std::string> enum_values;
  std::string error_msg;

  g_return_val_if_fail(node, enum_values);
  g_return_val_if_fail(selector_node, enum_values);

  auto sel_node = dynamic_cast<GenApi::ISelector*>(node);
  if (!sel_node) {
    std::string msg = std::string(node->GetName()) + " is an invalid node";
    throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }

  GenApi::FeatureList_t selectors;
  bool is_direct_feature = false;

  sel_node->GetSelectingFeatures(selectors);

  /* At the time being features with multiple selectors are not supported */
  guint max_selectors = 1;
  if (selectors.size() > max_selectors) {
    error_msg = "\"" + std::string(node->GetDisplayName()) + "\"" +
                " has more than " + std::to_string(max_selectors) +
                " selectors, ignoring!";
    throw Pylon::GenericException(error_msg.c_str(), __FILE__, __LINE__);
  }

  /* If the feature has no selectors then it is a "direct" feature, it does
   * not depend on any other selector
   */
  if (selectors.empty()) {
    is_direct_feature = true;
  } else {
    /* If the selector is in negative list it is a "direct" feature and
     * selector is ignored  */
    auto selector = selectors.at(0);
    is_direct_feature |=
        is_unsupported_selector(std::string(selector->GetNode()->GetName()));
  }

  if (is_direct_feature) {
    enum_values.push_back("direct-feature");
    return enum_values;
  }

  auto selector = selectors.at(0);
  *selector_node = selector->GetNode();

  gint selector_type = (*selector_node)->GetPrincipalInterfaceType();
  switch (selector_type) {
    case GenApi::intfIEnumeration:
      enum_values = gst_pylon_get_enum_entries(
          dynamic_cast<GenApi::IEnumeration*>(selector));
      break;
    case GenApi::intfIInteger:
      enum_values =
          gst_pylon_get_int_entries(dynamic_cast<GenApi::IInteger*>(selector));
      break;
    default:
      error_msg = "Selector \"" + std::string((*selector_node)->GetName()) +
                  "\"" + " is of invalid type " +
                  std::to_string(selector_type) + ", ignoring feature " + "\"" +
                  std::string(node->GetDisplayName()) + "\"";
      throw Pylon::GenericException(error_msg.c_str(), __FILE__, __LINE__);
  }

  return enum_values;
}

namespace {

class SelectorStateGuard {
 public:
  explicit SelectorStateGuard(GenApi::INode* selector_node)
      : selector_node_(selector_node) {
    if (!selector_node_ || !GenApi::IsReadable(selector_node_)) {
      return;
    }
    try {
      switch (selector_node_->GetPrincipalInterfaceType()) {
        case GenApi::intfIEnumeration:
          saved_value_ = Pylon::CEnumParameter(selector_node_).GetIntValue();
          has_saved_ = true;
          break;
        case GenApi::intfIInteger:
          saved_value_ = Pylon::CIntegerParameter(selector_node_).GetValue();
          has_saved_ = true;
          break;
        default:
          break;
      }
    } catch (const GenICam::GenericException&) {
      has_saved_ = false;
    }
  }

  ~SelectorStateGuard() {
    if (!has_saved_ || !selector_node_ || !GenApi::IsWritable(selector_node_)) {
      return;
    }
    try {
      switch (selector_node_->GetPrincipalInterfaceType()) {
        case GenApi::intfIEnumeration: {
          Pylon::CEnumParameter p(selector_node_);
          if (p.GetIntValue() != saved_value_) {
            p.SetIntValue(saved_value_);
          }
          break;
        }
        case GenApi::intfIInteger: {
          Pylon::CIntegerParameter p(selector_node_);
          if (p.GetValue() != saved_value_) {
            p.SetValue(saved_value_);
          }
          break;
        }
        default:
          break;
      }
    } catch (const GenICam::GenericException& e) {
      GST_DEBUG("Could not restore selector '%s': %s",
                selector_node_->GetName().c_str(), e.GetDescription());
    }
  }

 private:
  GenApi::INode* selector_node_ = nullptr;
  gint64 saved_value_ = 0;
  bool has_saved_ = false;
};

}  // namespace

std::vector<GParamSpec*> gst_pylon_camera_handle_node(
    GenApi::INode* node, GstPylonParamFactory& param_factory) {
  GenApi::INode* selector_node = NULL;
  gint64 selector_value = 0;
  std::vector<GParamSpec*> specs_list;
  Pylon::CEnumParameter param;

  g_return_val_if_fail(node, specs_list);

  std::vector<std::string> enum_values =
      GstPylonFeatureWalker::process_selector_features(node, &selector_node);

  /* If the number of selector values (stored in enum_values) is 1, leave
   * selector_node NULL, hence treating the feature as a "direct" one. */
  if (1 == enum_values.size()) {
    selector_node = NULL;
  }

  /* Restore any selector modified during property introspection (e.g.
   * Stream.TransmissionType, TriggerSelector, CounterSelector) back to its
   * original hardware/driver state before returning. */
  SelectorStateGuard selector_guard(selector_node);

  for (auto& enum_value : enum_values) {
    try {
      if (NULL != selector_node) {
        switch (selector_node->GetPrincipalInterfaceType()) {
          case GenApi::intfIEnumeration:
            param.Attach(selector_node);
            selector_value =
                param.GetEntryByName(enum_value.c_str())->GetValue();
            break;
          case GenApi::intfIInteger: {
            gint64 parsed = 0;
            GError* parse_err = NULL;
            if (!g_ascii_string_to_signed(enum_value.c_str(), 10, G_MININT64,
                                          G_MAXINT64, &parsed, &parse_err)) {
              std::string msg =
                  "Invalid integer selector entry '" + enum_value +
                  "': " + (parse_err ? parse_err->message : "parse error");
              g_clear_error(&parse_err);
              throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
            }
            selector_value = parsed;
            break;
          }
          default:; /* do nothing */
        }
      }

      GParamSpec* spec =
          param_factory.make_param(node, selector_node, selector_value);
      if (spec) {
        if (spec->flags & (G_PARAM_READABLE | G_PARAM_WRITABLE)) {
          specs_list.push_back(spec);
        } else {
          g_param_spec_sink(spec);
        }
      }
    } catch (const Pylon::GenericException& e) {
      GST_DEBUG("Unable to fully install property '%s-%s' : %s",
                node->GetName().c_str(), enum_value.c_str(),
                e.GetDescription());
    }
  }

  return specs_list;
}

void gst_pylon_camera_install_specs(const std::vector<GParamSpec*>& specs_list,
                                    GObjectClass* oclass, gint& nprop) {
  g_return_if_fail(oclass);

  if (!specs_list.empty()) {
    for (const auto& pspec : specs_list) {
      if (!g_object_class_find_property(oclass, pspec->name)) {
        g_object_class_install_property(oclass, nprop, pspec);
        nprop++;
      } else {
        g_param_spec_unref(pspec);
      }
    }
  } else {
    throw Pylon::GenericException(
        "Could not install GParamSpecs, no GParamSpecs were created", __FILE__,
        __LINE__);
  }
}

void GstPylonFeatureWalker::install_properties(
    GObjectClass* oclass, const GstPylonObjectSchema& schema) {
  g_return_if_fail(oclass);

  /* handle filter for debugging */
  const char* single_feature = NULL;
  if (const char* env_p = std::getenv("PYLONSRC_SINGLE_FEATURE")) {
    GST_DEBUG("LIMIT to use only feature %s\n", env_p);
    single_feature = env_p;
  }

  const std::string& device_fullname = schema.device_full_name;
  GstPylonCache& feature_cache = schema.cache();
  auto param_factory = GstPylonParamFactory(schema);

  gint nprop = 1;
  GenApi::INode* root_node = schema.node_map().GetNode("Root");
  auto worklist = std::queue<GenApi::INode*>();

  worklist.push(root_node);

  while (!worklist.empty()) {
    auto node = worklist.front();
    worklist.pop();

    /* Only handle real features that are not in the filter set, are not
     * selectors and are available */
    auto sel_node = dynamic_cast<GenApi::ISelector*>(node);
    auto category_node = dynamic_cast<GenApi::ICategory*>(node);
    if (!category_node && node->IsFeature() &&
        (node->GetVisibility() != GenApi::Invisible) &&
        GenApi::IsImplemented(node) &&
        !is_unsupported_feature(std::string(node->GetName())) &&
        node->GetPrincipalInterfaceType() != GenApi::intfICategory &&
        node->GetPrincipalInterfaceType() != GenApi::intfICommand &&
        node->GetPrincipalInterfaceType() != GenApi::intfIRegister &&
        sel_node && !sel_node->IsSelector()) {
      GenICam::gcstring value;
      GenICam::gcstring attrib;

      try {
        if (!single_feature ||
            (single_feature && std::string(node->GetName().c_str()) ==
                                   std::string(single_feature))) {
          GST_DEBUG("Install node %s", node->GetName().c_str());
          std::vector<GParamSpec*> specs_list =
              gst_pylon_camera_handle_node(node, param_factory);

          gst_pylon_camera_install_specs(specs_list, oclass, nprop);
        }
      } catch (const Pylon::GenericException& e) {
        GST_DEBUG("Unable to install property \"%s\" on device \"%s\": %s",
                  node->GetName().c_str(), device_fullname.c_str(),
                  e.GetDescription());
      }
    }

    /* Walk down all categories */
    if (category_node &&
        !is_unsupported_category(std::string(node->GetName()))) {
      GenApi::FeatureList_t features;
      category_node->GetFeatures(features);
      for (auto const& f : features) {
        worklist.push(f->GetNode());
      }
    }
  }

  if (feature_cache.HasNewSettings()) {
    try {
      feature_cache.CreateCacheFile();
    } catch (const Pylon::GenericException& e) {
      GST_WARNING("Feature cache could not be generated. %s",
                  e.GetDescription());
    }
  }
}
