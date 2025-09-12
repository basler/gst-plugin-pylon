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

#include "gstpylonfastparamfactory.h"

#include "gstpylonparamspecs.h"

#include <unordered_map>

GParamFlags GstPylonFastParamFactory::gst_pylon_query_fast_access(
    GenApi::INode *node) {
  g_return_val_if_fail(node, G_PARAM_READABLE);

  GParamFlags flags = G_PARAM_READABLE;

  try {
    if (GenApi::IsWritable(node)) {
      flags = static_cast<GParamFlags>(flags | G_PARAM_WRITABLE);
    }
  } catch (const Pylon::GenericException &e) {
    /* If we can't determine writability, assume read-only */
  }

  return flags;
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_int64(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CIntegerParameter param(node);
  gint64 default_value = 0;

  try {
    default_value = param.GetValue();
  } catch (const Pylon::GenericException &e) {
    /* Use 0 as default if we can't read current value */
    default_value = 0;
  }

  return g_param_spec_int64(node->GetName(), node->GetDisplayName(),
                            node->GetToolTip(), G_MININT64, G_MAXINT64,
                            default_value, gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_selector_int64(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  /* In fast mode, don't try to read the actual default value for selector
   * properties as this would require setting the selector first, which is
   * expensive. Use a safe default value instead. */
  gint64 default_value = 0;

  return gst_pylon_param_spec_selector_int64(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), G_MININT64, G_MAXINT64,
      default_value, gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_bool(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CBooleanParameter param(node);
  gboolean default_value = FALSE;

  try {
    default_value = param.GetValue();
  } catch (const Pylon::GenericException &e) {
    default_value = FALSE;
  }

  return g_param_spec_boolean(node->GetName(), node->GetDisplayName(),
                              node->GetToolTip(), default_value,
                              gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_selector_bool(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  /* In fast mode, don't try to read the actual default value for selector
   * properties as this would require setting the selector first, which is
   * expensive. Use a safe default value instead. */
  gboolean default_value = FALSE;

  return gst_pylon_param_spec_selector_boolean(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), default_value,
      gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_double(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CFloatParameter param(node);
  gdouble default_value = 0.0;

  try {
    default_value = param.GetValue();
    /* Clamp default value to our reasonable range */
    if (default_value < -1e15) {
      default_value = -1e15;
    } else if (default_value > 1e15) {
      default_value = 1e15;
    }
  } catch (const Pylon::GenericException &e) {
    default_value = 0.0;
  }

  return g_param_spec_double(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), -1e15, 1e15, default_value,
                             gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_selector_double(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  /* In fast mode, don't try to read the actual default value for selector
   * properties as this would require setting the selector first, which is
   * expensive. Use a safe default value instead. */
  gdouble default_value = 0.0;

  return gst_pylon_param_spec_selector_double(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), -1e15, 1e15, default_value,
      gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_str(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CStringParameter param(node);
  const gchar *default_value = "";

  try {
    default_value = param.GetValue();
  } catch (const Pylon::GenericException &e) {
    default_value = "";
  }

  return g_param_spec_string(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), default_value,
                             gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_selector_str(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  /* In fast mode, don't try to read the actual default value for selector
   * properties as this would require setting the selector first, which is
   * expensive. Use a safe default value instead. */
  const gchar *default_value = "";

  return gst_pylon_param_spec_selector_string(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), default_value,
      gst_pylon_query_fast_access(node));
}

GType GstPylonFastParamFactory::gst_pylon_make_fast_enum_type(
    GenApi::INode *node) {
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

    try {
      /* In fast mode, enumerate all IsImplemented values for completeness */
      GenApi::NodeList_t enum_entries;
      dynamic_cast<GenApi::IEnumeration *>(node)->GetEntries(enum_entries);

      for (auto const &e : enum_entries) {
        if (!GenApi::IsImplemented(e)) {
          continue;
        }

        auto entry = dynamic_cast<GenApi::IEnumEntry *>(e);
        if (!entry) {
          continue;
        }

        auto value = static_cast<gint>(entry->GetValue());
        auto value_name = entry->GetSymbolic();
        auto tooltip = entry->GetNode()->GetToolTip();

        /* Strip enum prefix for cleaner names */
        std::string clean_name = value_name.c_str();
        const auto prefix_str = std::string("EnumEntry_") +
                                node->GetName().c_str() + std::string("_");
        if (clean_name.length() > prefix_str.length() &&
            clean_name.substr(0, prefix_str.length()) == prefix_str) {
          clean_name = clean_name.substr(prefix_str.length());
        }

        GEnumValue ev = {value, g_strdup(clean_name.c_str()),
                         g_strdup(tooltip.c_str())};
        enumvalues.push_back(ev);
      }
    } catch (const Pylon::GenericException &e) {
      /* If we can't enumerate, create a minimal enum with safe default */
      GST_DEBUG("Unable to enumerate values for %s, using minimal enum: %s",
                node->GetName().c_str(), e.GetDescription());
      GEnumValue ev = {0, g_strdup("unknown"), g_strdup("Unknown value")};
      enumvalues.push_back(ev);
    }

    GEnumValue sentinel = {0};
    enumvalues.push_back(sentinel);

    type = g_enum_register_static(name.c_str(), enumvalues.data());
    persistent_values.insert({type, std::move(enumvalues)});
  }

  return type;
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_enum(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_fast_enum_type(node);
  gint default_value = 0;

  try {
    default_value = param.GetIntValue();
  } catch (const Pylon::GenericException &e) {
    default_value = 0;
  }

  return g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                           node->GetToolTip(), type, default_value,
                           gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::gst_pylon_make_fast_spec_selector_enum(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  GType type = gst_pylon_make_fast_enum_type(node);
  /* In fast mode, don't try to read the actual default value for selector
   * properties as this would require setting the selector first, which is
   * expensive. Use a safe default value instead. */
  gint default_value = 0;

  return gst_pylon_param_spec_selector_enum(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), type, default_value,
      gst_pylon_query_fast_access(node));
}

GParamSpec *GstPylonFastParamFactory::make_param(GenApi::INode *node,
                                                 GenApi::INode *selector,
                                                 guint64 selector_value) {
  g_return_val_if_fail(node, NULL);

  GParamSpec *spec = NULL;
  GenApi::EInterfaceType iface = node->GetPrincipalInterfaceType();

  switch (iface) {
    case GenApi::intfIInteger:
      if (!selector) {
        spec = gst_pylon_make_fast_spec_int64(node);
      } else {
        spec = gst_pylon_make_fast_spec_selector_int64(node, selector,
                                                       selector_value);
      }
      break;
    case GenApi::intfIBoolean:
      if (!selector) {
        spec = gst_pylon_make_fast_spec_bool(node);
      } else {
        spec = gst_pylon_make_fast_spec_selector_bool(node, selector,
                                                      selector_value);
      }
      break;
    case GenApi::intfIFloat:
      if (!selector) {
        spec = gst_pylon_make_fast_spec_double(node);
      } else {
        spec = gst_pylon_make_fast_spec_selector_double(node, selector,
                                                        selector_value);
      }
      break;
    case GenApi::intfIString:
      if (!selector) {
        spec = gst_pylon_make_fast_spec_str(node);
      } else {
        spec = gst_pylon_make_fast_spec_selector_str(node, selector,
                                                     selector_value);
      }
      break;
    case GenApi::intfIEnumeration:
      if (!selector) {
        spec = gst_pylon_make_fast_spec_enum(node);
      } else {
        spec = gst_pylon_make_fast_spec_selector_enum(node, selector,
                                                      selector_value);
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
