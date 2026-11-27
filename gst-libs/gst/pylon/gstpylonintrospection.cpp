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
#include "gstpylonintrospection.h"
#include "gstpylonlimitprobe.h"
#include "gstpylonobject.h"
#include "gstpylonparamspecs.h"

gboolean gst_pylon_can_feature_later_be_writable(GenApi::INode* node) {
  if (!node) {
    return FALSE;
  }
  GenICam::gcstring value;
  GenICam::gcstring attribute;
  if (node->GetProperty("pIsLocked", value, attribute)) {
    return TRUE;
  }
  GenApi::NodeList_t node_children;
  node->GetChildren(node_children);
  for (auto& node_child : node_children) {
    if (gst_pylon_can_feature_later_be_writable(node_child)) {
      return TRUE;
    }
  }
  return FALSE;
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
        gst_pylon_probe_double_limits(node, minimum_under_all_settings,
                                      maximum_under_all_settings,
                                      device_profile);
      } else if (!gst_pylon_try_prior_limits(device_profile, node,
                                             minimum_under_all_settings,
                                             maximum_under_all_settings)) {
        minimum_under_all_settings = gst_pylon_query_double_limit(node, "min");
        maximum_under_all_settings = gst_pylon_query_double_limit(node, "max");
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
        gst_pylon_probe_integer_limits(node, minimum_under_all_settings,
                                       maximum_under_all_settings,
                                       device_profile);
      } else if (!gst_pylon_try_prior_limits(device_profile, node,
                                             minimum_under_all_settings,
                                             maximum_under_all_settings)) {
        minimum_under_all_settings = gst_pylon_query_integer_limit(node, "min");
        maximum_under_all_settings = gst_pylon_query_integer_limit(node, "max");
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
