/* Copyright (C) 2022 Basler AG
 *
 * Created by RidgeRun, LLC <support@ridgerun.com>
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

#include "gstpyloncamera.h"
#include "gstpylonintrospection.h"
#include "gstpylonparamspecs.h"

#include <numeric>
#include <queue>

typedef struct _GstPylonCameraPrivate GstPylonCameraPrivate;
struct _GstPylonCameraPrivate {
  std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera;
};

/************************************************************
 * Start of GObject definition
 ***********************************************************/

static gchar* gst_pylon_camera_get_sanitized_name(
    const Pylon::CBaslerUniversalInstantCamera& camera) {
  Pylon::String_t cam_name = camera.GetDeviceInfo().GetFullName();

  /* Convert camera name to a valid string */
  return GstPylonParamFactory::sanitize_name(cam_name.c_str());
}

static void gst_pylon_camera_init(GstPylonCamera* self);
static void gst_pylon_camera_class_init(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera);
static gpointer gst_pylon_camera_parent_class = NULL;
static gint GstPylonCamera_private_offset;
static void gst_pylon_camera_class_intern_init(
    gpointer klass, Pylon::CBaslerUniversalInstantCamera* camera) {
  gst_pylon_camera_parent_class = g_type_class_peek_parent(klass);
  if (GstPylonCamera_private_offset != 0)
    g_type_class_adjust_private_offset(klass, &GstPylonCamera_private_offset);
  gst_pylon_camera_class_init((GstPylonCameraClass*)klass, camera);
}
static inline gpointer gst_pylon_camera_get_instance_private(
    GstPylonCamera* self) {
  return (G_STRUCT_MEMBER_P(self, GstPylonCamera_private_offset));
}

GType gst_pylon_camera_register(
    const Pylon::CBaslerUniversalInstantCamera& exemplar) {
  GTypeInfo typeinfo = {
      sizeof(GstPylonCameraClass),
      NULL,
      NULL,
      (GClassInitFunc)gst_pylon_camera_class_intern_init,
      NULL,
      &exemplar,
      sizeof(GstPylonCamera),
      0,
      (GInstanceInitFunc)gst_pylon_camera_init,
  };

  gchar* type_name = gst_pylon_camera_get_sanitized_name(exemplar);

  GType type = g_type_from_name(type_name);
  if (!type) {
    type = g_type_register_static(G_TYPE_OBJECT, type_name, &typeinfo,
                                  static_cast<GTypeFlags>(0));
  }

  g_free(type_name);

  GstPylonCamera_private_offset =
      g_type_add_instance_private(type, sizeof(GstPylonCameraPrivate));

  return type;
}

/************************************************************
 * End of GObject definition
 ***********************************************************/

/* prototypes */
template <class Type>
static std::vector<std::vector<Type>> cartesian(
    std::vector<std::vector<Type>>& v);
static void handle_node(GenApi::INode* node,
                        Pylon::CBaslerUniversalInstantCamera* camera,
                        GObjectClass* oclass, gint& nprop);
static void gst_pylon_camera_install_properties(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera);
template <typename F, typename P>
static void set_pylon_property(GenApi::INodeMap& nodemap, F get_value,
                               const GValue* value, const gchar* name);
static void set_enum_property(GenApi::INodeMap& nodemap, const GValue* value,
                              const gchar* name);
template <typename T, typename P>
static T get_pylon_property(GenApi::INodeMap& nodemap, const gchar* name);
static gint get_enum_property(GenApi::INodeMap& nodemap, const gchar* name);
static void gst_pylon_camera_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec);
static void gst_pylon_camera_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec);
static void gst_pylon_camera_finalize(GObject* self);

template <class Type>
static std::vector<std::vector<Type>> cartesian(
    std::vector<std::vector<Type>>& v) {
  std::vector<std::vector<Type>> result;
  auto product = [](long long a, std::vector<Type>& b) { return a * b.size(); };
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

static void handle_node(GenApi::INode* node,
                        Pylon::CBaslerUniversalInstantCamera* camera,
                        GObjectClass* oclass, gint& nprop) {
  GenApi::INode* selector_node = NULL;
  guint64 selector_value = 0;

  g_return_if_fail(node);
  g_return_if_fail(camera);

  if (auto sel_node = dynamic_cast<GenApi::ISelector*>(node)) {
    GenApi::FeatureList_t selectors;
    sel_node->GetSelectingFeatures(selectors);
    if (selectors.size()) {
      /* This feature has "is selected" */
      std::vector<std::vector<std::string>> selector_values;
      for (auto const& s : selectors) {
        printf("selector: %s\n", s->GetNode()->GetName().c_str());

        /* Add selector enum values */
        std::vector<std::string> enum_values;
        if (auto enum_node = dynamic_cast<GenApi::IEnumeration*>(s)) {
          GenApi::NodeList_t enum_entries;
          enum_node->GetEntries(enum_entries);
          for (auto const& e : enum_entries) {
            auto enum_name = std::string(e->GetName());

            enum_values.push_back(
                enum_name.substr(enum_name.find_last_of("_") + 1));
          }
          /* Found values */
          if (enum_values.size()) {
            selector_values.push_back(enum_values);
          }
        } else {
          GST_DEBUG("%s is not an enumerator selector, ignoring!",
                    node->GetDisplayName().c_str());
        }
      }

      /* Output node for all selector permutations
       *
       * Support for features that have more than one selector
       * is pending.
       */
      auto selector_permutations = cartesian(selector_values);
      for (auto const& sel_pair : selector_permutations) {
        if (sel_pair.size()) {
          selector_node = selectors.at(0)->GetNode();
          Pylon::CEnumParameter param(selector_node);
          selector_value =
              param.GetEntryByName(sel_pair.at(0).c_str())->GetValue();
          GParamSpec* pspec = GstPylonParamFactory::make_param(
              node, selector_node, selector_value, camera);
          g_object_class_install_property(oclass, nprop, pspec);
          nprop++;
        }
      }
    } else {
      // "direct" unselected features
      GParamSpec* pspec = GstPylonParamFactory::make_param(
          node, selector_node, selector_value, camera);
      g_object_class_install_property(oclass, nprop, pspec);
      nprop++;
    }

  } else {
    std::string msg = std::string(node->GetName()) + " is an invalid node";
    throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }
}

static void gst_pylon_camera_install_properties(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera) {
  g_return_if_fail(klass);
  g_return_if_fail(camera);

  GenApi::INodeMap& nodemap = camera->GetNodeMap();
  GObjectClass* oclass = G_OBJECT_CLASS(klass);
  gint nprop = 1;

  GenApi::INode* root_node = nodemap.GetNode("Root");
  auto worklist = std::queue<GenApi::INode*>();

  worklist.push(root_node);

  while (!worklist.empty()) {
    auto node = worklist.front();
    worklist.pop();

    /* Only handle real features, excluding selectors */
    auto sel_node = dynamic_cast<GenApi::ISelector*>(node);
    if (node->IsFeature() && (node->GetVisibility() != GenApi::Invisible) &&
        sel_node && !sel_node->IsSelector()) {
      GenICam::gcstring value;
      GenICam::gcstring attrib;

      /* Handle only features with the 'Streamable' flag */
      if (node->GetProperty("Streamable", value, attrib)) {
        if (GenICam::gcstring("Yes") == value) {
          try {
            /* Follow the feature walker here to unroll the selector
             * with each entry. Selector may be null for non-selected
             * features. For example:
             *  Integer: selector: ROIZoneSelector
             *    ROIZoneSize-Zone0
             *    ROIZoneSize-Zone1
             *    ROIZoneSize-Zone2
             *
             * would make three calls:
             *   INode * node = nodemap ("ROIZoneSize");
             *   INode * selector = nodemap ("ROIZoneSelector");
             *
             *   guint64 selector_value = selector->GetValueByName ("Zone0");
             *   make_param (node, selector, selector_value, camera);
             *
             *   selector_value = selector->GetValueByName ("Zone1");
             *   make_param (node, selector, selector_value, camera);
             *
             *   selector_value = selector->GetValueByName ("Zone2");
             *   make_param (node, selector, selector_value, camera);
             *
             * dynamic_cast<ISelector*>(node) -> necesary for selector node.
             */
            int selector_value = 0;  // temporal
            GenApi::INode* selector = nullptr;
            GParamSpec* pspec = GstPylonParamFactory::make_param(
                node, selector, selector_value, camera);
            g_object_class_install_property(oclass, nprop, pspec);
            nprop++;
          } catch (const Pylon::GenericException& e) {
            GST_DEBUG("Unable to install property \"%s\" on \"%s\": %s",
                      node->GetDisplayName().c_str(),
                      camera->GetDeviceInfo().GetFullName().c_str(),
                      e.GetDescription());
          }
        }
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
}

static void gst_pylon_camera_class_init(
    GstPylonCameraClass* klass,
    Pylon::CBaslerUniversalInstantCamera* exemplar) {
  GObjectClass* oclass = G_OBJECT_CLASS(klass);

  oclass->set_property = gst_pylon_camera_set_property;
  oclass->get_property = gst_pylon_camera_get_property;
  oclass->finalize = gst_pylon_camera_finalize;

  gst_pylon_camera_install_properties(klass, exemplar);
}

static void gst_pylon_camera_init(GstPylonCamera* self) {}

template <typename F, typename P>
static void set_pylon_property(GenApi::INodeMap& nodemap, F get_value,
                               const GValue* value, const gchar* name) {
  P param(nodemap, name);
  param.SetValue(get_value(value));
}

static void set_enum_property(GenApi::INodeMap& nodemap, const GValue* value,
                              const gchar* name) {
  Pylon::CEnumParameter param(nodemap, name);
  param.SetIntValue(g_value_get_enum(value));
}

static void set_selector_int64_property(GenApi::INodeMap& nodemap,
                                        const GValue* value,
                                        GenApi::INode* feature,
                                        GenApi::INode* selector,
                                        guint64 selector_value) {
  Pylon::CEnumParameter selparam(selector);
  Pylon::CIntegerParameter param(feature);

  selparam.SetIntValue(selector_value);
  param.SetValue(g_value_get_int64(value));
}

template <typename T, typename P>
static T get_pylon_property(GenApi::INodeMap& nodemap, const gchar* name) {
  P param(nodemap, name);
  return param.GetValue();
}

static gint get_enum_property(GenApi::INodeMap& nodemap, const gchar* name) {
  Pylon::CEnumParameter param(nodemap, name);
  return param.GetIntValue();
}

static void gst_pylon_camera_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec) {
  GstPylonCamera* self = (GstPylonCamera*)object;
  GstPylonCameraPrivate* priv =
      (GstPylonCameraPrivate*)gst_pylon_camera_get_instance_private(self);
  GType feature_type = g_type_fundamental(G_PARAM_SPEC_TYPE(pspec));

  try {
    GenApi::INodeMap& nodemap = priv->camera->GetNodeMap();
    if (G_TYPE_PARAM_INT64 == feature_type) {
      typedef gint64 (*GGetInt64)(const GValue*);
      set_pylon_property<GGetInt64, Pylon::CIntegerParameter>(
          nodemap, g_value_get_int64, value, pspec->name);
    } else if (GST_PYLON_TYPE_PARAM_SELECTOR_INT64 == feature_type) {
      GstPylonParamSpecSelectorInt64* lspec =
          GST_PYLON_PARAM_SPEC_SELECTOR_INT64(pspec);
      set_selector_int64_property(nodemap, value, lspec->feature,
                                  lspec->selector, lspec->selector_value);
    } else if (G_TYPE_PARAM_BOOLEAN == feature_type) {
      typedef gboolean (*GGetBool)(const GValue*);
      set_pylon_property<GGetBool, Pylon::CBooleanParameter>(
          nodemap, g_value_get_boolean, value, pspec->name);
    } else if (G_TYPE_PARAM_FLOAT == feature_type) {
      typedef gfloat (*GGetFloat)(const GValue*);
      set_pylon_property<GGetFloat, Pylon::CFloatParameter>(
          nodemap, g_value_get_float, value, pspec->name);
    } else if (G_TYPE_PARAM_STRING == feature_type) {
      typedef const gchar* (*GGetString)(const GValue*);
      set_pylon_property<GGetString, Pylon::CStringParameter>(
          nodemap, g_value_get_string, value, pspec->name);
    } else if (G_TYPE_PARAM_ENUM == feature_type) {
      set_enum_property(nodemap, value, pspec->name);
    } else {
      g_warning("Unsupported GType: %s", g_type_name(pspec->value_type));
      std::string msg =
          "Unsupported GType: " + std::string(g_type_name(pspec->value_type));
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } catch (const Pylon::GenericException& e) {
    GST_ERROR("Unable to set pylon property \"%s\" on \"%s\": %s", pspec->name,
              priv->camera->GetDeviceInfo().GetFullName().c_str(),
              e.GetDescription());
  }
}

static void gst_pylon_camera_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec) {
  GstPylonCamera* self = (GstPylonCamera*)object;
  GstPylonCameraPrivate* priv =
      (GstPylonCameraPrivate*)gst_pylon_camera_get_instance_private(self);

  try {
    GenApi::INodeMap& nodemap = priv->camera->GetNodeMap();
    switch (g_type_fundamental(pspec->value_type)) {
      case G_TYPE_INT64:
        g_value_set_int64(value,
                          get_pylon_property<gint64, Pylon::CIntegerParameter>(
                              nodemap, pspec->name));
        break;
      case G_TYPE_BOOLEAN:
        g_value_set_boolean(
            value, get_pylon_property<gboolean, Pylon::CBooleanParameter>(
                       nodemap, pspec->name));
        break;
      case G_TYPE_FLOAT:
        g_value_set_float(value,
                          get_pylon_property<gfloat, Pylon::CFloatParameter>(
                              nodemap, pspec->name));
        break;
      case G_TYPE_STRING:
        g_value_set_string(
            value,
            get_pylon_property<GenICam::gcstring, Pylon::CStringParameter>(
                nodemap, pspec->name)
                .c_str());
        break;
      case G_TYPE_ENUM:
        g_value_set_enum(value, get_enum_property(nodemap, pspec->name));
        break;
      default:
        g_warning("Unsupported GType: %s", g_type_name(pspec->value_type));
        std::string msg =
            "Unsupported GType: " + std::string(g_type_name(pspec->value_type));
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } catch (const Pylon::GenericException& e) {
    GST_ERROR("Unable to get pylon property \"%s\" on \"%s\": %s", pspec->name,
              priv->camera->GetDeviceInfo().GetFullName().c_str(),
              e.GetDescription());
  }
}

GObject* gst_pylon_camera_new(
    std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera) {
  gchar* type_name = gst_pylon_camera_get_sanitized_name(*camera);

  GType type = g_type_from_name(type_name);
  g_free(type_name);

  GObject* obj = G_OBJECT(g_object_new(type, NULL));
  GstPylonCamera* self = (GstPylonCamera*)obj;
  GstPylonCameraPrivate* priv =
      (GstPylonCameraPrivate*)gst_pylon_camera_get_instance_private(self);

  priv->camera = camera;

  return obj;
}

static void gst_pylon_camera_finalize(GObject* object) {
  GstPylonCamera* self = (GstPylonCamera*)object;
  GstPylonCameraPrivate* priv =
      (GstPylonCameraPrivate*)gst_pylon_camera_get_instance_private(self);

  priv->camera = NULL;

  G_OBJECT_CLASS(gst_pylon_camera_parent_class)->finalize(object);
}
