#include "gstpylonparamfactory.h"

#include "gstpylonintrospection.h"
#include "gstpylonparamspecs.h"

#include <unordered_map>

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_int64(
    GenApi::INode *node) {
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

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_selector_int64(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
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

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_bool(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CBooleanParameter param(node);

  return g_param_spec_boolean(node->GetName(), node->GetDisplayName(),
                              node->GetToolTip(), param.GetValue(),
                              gst_pylon_query_access(nodemap, node));
}

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_selector_bool(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CBooleanParameter param(node);

  return gst_pylon_param_spec_selector_boolean(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(nodemap, node));
}

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_double(
    GenApi::INode *node) {
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

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_selector_double(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
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

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_str(GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CStringParameter param(node);

  return g_param_spec_string(node->GetName(), node->GetDisplayName(),
                             node->GetToolTip(), param.GetValue(),
                             gst_pylon_query_access(nodemap, node));
}

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_selector_str(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CStringParameter param(node);

  return gst_pylon_param_spec_selector_string(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), param.GetValue(),
      gst_pylon_query_access(nodemap, node));
}

GType GstPylonParamFactory::gst_pylon_make_enum_type(GenApi::INode *node) {
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

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_enum(
    GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(node);

  return g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                           node->GetToolTip(), type, param.GetIntValue(),
                           gst_pylon_query_access(nodemap, node));
}

GParamSpec *GstPylonParamFactory::gst_pylon_make_spec_selector_enum(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);
  g_return_val_if_fail(selector, NULL);

  Pylon::CEnumParameter param(node);
  GType type = gst_pylon_make_enum_type(node);

  return gst_pylon_param_spec_selector_enum(
      nodemap, node->GetName(), selector->GetName(), selector_value,
      node->GetDisplayName(), node->GetToolTip(), type, param.GetIntValue(),
      gst_pylon_query_access(nodemap, node));
}

GParamSpec *GstPylonParamFactory::GstPylonParamFactory::make_param(
    GenApi::INode *node, GenApi::INode *selector, guint64 selector_value) {
  g_return_val_if_fail(node, NULL);

  GParamSpec *spec = NULL;
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

  return spec;
}
