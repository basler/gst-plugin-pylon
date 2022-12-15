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
#include "config.h"
#endif

#include "gstpylondebug.h"
#include "gstpylonfeaturewalker.h"
#include "gstpylonintrospection.h"

#include <string.h>

#include <queue>
#include <unordered_set>

#define MAX_INT_SELECTOR_ENTRIES 16

/* prototypes */
static std::vector<std::string> gst_pylon_get_enum_entries(
    GenApi::IEnumeration* enum_node);
static std::vector<std::string> gst_pylon_get_int_entries(
    GenApi::IInteger* int_node);
static std::vector<GParamSpec*> gst_pylon_camera_handle_node(
    GenApi::INode* node, GenApi::INodeMap& nodemap,
    const std::string device_fullname, GKeyFile* feature_cache);
static void gst_pylon_camera_install_specs(
    const std::vector<GParamSpec*>& specs_list, GObjectClass* oclass,
    gint& nprop);
static gchar* gst_pylon_check_for_feature_cache(
    const std::string cache_filename);
static void gst_pylon_create_cache_file(GKeyFile* feature_cache,
                                        const std::string cache_filename);

static std::unordered_set<std::string> propfilter_set = {
    "Width",
    "Height",
    "PixelFormat",
    "AcquisitionFrameRateEnable",
    "AcquisitionFrameRate",
    "AcquisitionFrameRateAbs"};

static std::vector<std::string> gst_pylon_get_enum_entries(
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

static std::vector<std::string> gst_pylon_get_int_entries(
    GenApi::IInteger* int_node) {
  std::vector<std::string> entry_names;

  g_return_val_if_fail(int_node, entry_names);

  for (gint i = int_node->GetMin(); i <= int_node->GetMax();
       i += int_node->GetInc()) {
    /* Limit integer based selectors to MAX_INT_SELECTOR_ENTRIES */
    if (i > MAX_INT_SELECTOR_ENTRIES) {
      break;
    }
    entry_names.push_back(std::to_string(i));
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

  /* If the feature has no selectors then it is a "direct" feature, it does not
   * depend on any other selector */
  GenApi::FeatureList_t selectors;
  sel_node->GetSelectingFeatures(selectors);
  if (selectors.empty()) {
    enum_values.push_back("direct-feature");
    return enum_values;
  }

  /* At the time being features with multiple selectors are not supported */
  guint max_selectors = 1;
  if (selectors.size() > max_selectors) {
    error_msg = "\"" + std::string(node->GetDisplayName()) + "\"" +
                " has more than " + std::to_string(max_selectors) +
                " selectors, ignoring!";
    throw Pylon::GenericException(error_msg.c_str(), __FILE__, __LINE__);
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

static std::vector<GParamSpec*> gst_pylon_camera_handle_node(
    GenApi::INode* node, GenApi::INodeMap& nodemap,
    const std::string device_fullname, GKeyFile* feature_cache) {
  GenApi::INode* selector_node = NULL;
  guint64 selector_value = 0;
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

  for (guint i = 0; i < enum_values.size(); i++) {
    if (NULL != selector_node) {
      switch (selector_node->GetPrincipalInterfaceType()) {
        case GenApi::intfIEnumeration:
          param.Attach(selector_node);
          selector_value =
              param.GetEntryByName(enum_values[i].c_str())->GetValue();
          break;
        case GenApi::intfIInteger:
          selector_value = std::stoi(enum_values[i]);
          break;
        default:; /* do nothing */
      }
    }
    specs_list.push_back(GstPylonParamFactory::make_param(
        nodemap, node, selector_node, selector_value, device_fullname,
        feature_cache));
  }

  return specs_list;
}

static void gst_pylon_camera_install_specs(
    const std::vector<GParamSpec*>& specs_list, GObjectClass* oclass,
    gint& nprop) {
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

static gchar* gst_pylon_check_for_feature_cache(
    const std::string cache_filename) {
  gchar* filename_hash =
      g_compute_checksum_for_string(G_CHECKSUM_SHA256, cache_filename.c_str(),
                                    strlen(cache_filename.c_str()));
  gchar* dirpath = g_strdup_printf("%s/%s", g_get_user_cache_dir(), "gstpylon");

  /* Create gstpylon directory */
  gint dir_permissions = 0775;
  g_mkdir_with_parents(dirpath, dir_permissions);

  gchar* filepath = g_strdup_printf("%s/%s", dirpath, filename_hash);

  g_free(dirpath);
  g_free(filename_hash);

  return filepath;
}

static void gst_pylon_create_cache_file(GKeyFile* feature_cache,
                                        const std::string cache_filename) {
  g_return_if_fail(feature_cache);

  gsize len = 0;
  gchar* feature_cache_str = g_key_file_to_data(feature_cache, &len, NULL);
  gchar* filepath = gst_pylon_check_for_feature_cache(cache_filename.c_str());

  GError* file_err = NULL;
  gboolean ret =
      g_file_set_contents(filepath, feature_cache_str, len, &file_err);
  if (!ret) {
    GST_WARNING("Feature cache could not be generated. %s", file_err->message);
    g_error_free(file_err);
  }

  g_free(feature_cache_str);
  g_free(filepath);
}

void GstPylonFeatureWalker::install_properties(
    GObjectClass* oclass, GenApi::INodeMap& nodemap,
    const std::string device_fullname, GstPylonCache& feature_cache) {
  g_return_if_fail(oclass);

  /* Start KeyFile object to hold property cache */
  GKeyFile* feature_cache_dict = g_key_file_new();

  gint nprop = 1;
  GenApi::INode* root_node = nodemap.GetNode("Root");
  auto worklist = std::queue<GenApi::INode*>();

  worklist.push(root_node);

  while (!worklist.empty()) {
    auto node = worklist.front();
    worklist.pop();

    /* Only handle real features that are not in the filter set, are not
     * selectors and are available */
    auto sel_node = dynamic_cast<GenApi::ISelector*>(node);
    if (node->IsFeature() && (node->GetVisibility() != GenApi::Invisible) &&
        sel_node && GenApi::IsAvailable(node) && !sel_node->IsSelector() &&
        propfilter_set.find(std::string(node->GetName())) ==
            propfilter_set.end() &&
        node->GetPrincipalInterfaceType() != GenApi::intfICategory) {
      GenICam::gcstring value;
      GenICam::gcstring attrib;

      try {
        std::vector<GParamSpec*> specs_list = gst_pylon_camera_handle_node(
            node, nodemap, device_fullname, feature_cache_dict);
        gst_pylon_camera_install_specs(specs_list, oclass, nprop);
      } catch (const Pylon::GenericException& e) {
        GST_FIXME("Unable to install property \"%s\" on device \"%s\": %s",
                  node->GetDisplayName().c_str(), device_fullname.c_str(),
                  e.GetDescription());
      }
    }

    /* Walk down all categories */
    auto category_node = dynamic_cast<GenApi::ICategory*>(node);
    if (category_node) {
      GenApi::FeatureList_t features;
      category_node->GetFeatures(features);
      for (auto const& f : features) {
        worklist.push(f->GetNode());
      }
    }
  }

  g_key_file_free(feature_cache_dict);
}
