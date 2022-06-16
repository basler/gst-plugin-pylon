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

#include <queue>

/* prototypes */
static void gst_pylon_camera_install_properties(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera);
static void gst_pylon_camera_class_init(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera);
static void set_int_property(GenApi::INodeMap& nodemap, const GValue* value,
                             const gchar* name);
static void set_bool_property(GenApi::INodeMap& nodemap, const GValue* value,
                              const gchar* name);
static void set_float_property(GenApi::INodeMap& nodemap, const GValue* value,
                               const gchar* name);
static void set_str_property(GenApi::INodeMap& nodemap, const GValue* value,
                             const gchar* name);
static void set_enum_property(GenApi::INodeMap& nodemap, const GValue* value,
                              const gchar* name);
static gint get_int_property(GenApi::INodeMap& nodemap, const gchar* name);
static gboolean get_bool_property(GenApi::INodeMap& nodemap, const gchar* name);
static gfloat get_float_property(GenApi::INodeMap& nodemap, const gchar* name);
static gchar* get_str_property(GenApi::INodeMap& nodemap, const gchar* name);
static gint get_enum_property(GenApi::INodeMap& nodemap, const gchar* name);
static void gst_pylon_camera_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec);
static void gst_pylon_camera_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec);

#define VALID_CHARS G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS

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
            GParamSpec* pspec = GstPylonParamFactory::make_param(node);
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
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera) {
  GObjectClass* oclass = G_OBJECT_CLASS(klass);

  klass->camera = camera;

  oclass->set_property = gst_pylon_camera_set_property;
  oclass->get_property = gst_pylon_camera_get_property;

  gst_pylon_camera_install_properties(klass, camera);
}

static void set_int_property(GenApi::INodeMap& nodemap, const GValue* value,
                             const gchar* name) {
  Pylon::CIntegerParameter param(nodemap, name);
  param.SetValue(g_value_get_int(value));
}

static void set_bool_property(GenApi::INodeMap& nodemap, const GValue* value,
                              const gchar* name) {
  Pylon::CBooleanParameter param(nodemap, name);
  param.SetValue(g_value_get_boolean(value));
}

static void set_float_property(GenApi::INodeMap& nodemap, const GValue* value,
                               const gchar* name) {
  Pylon::CFloatParameter param(nodemap, name);
  param.SetValue(g_value_get_float(value));
}

static void set_str_property(GenApi::INodeMap& nodemap, const GValue* value,
                             const gchar* name) {
  Pylon::CStringParameter param(nodemap, name);
  param.SetValue(g_value_get_string(value));
}

static void set_enum_property(GenApi::INodeMap& nodemap, const GValue* value,
                              const gchar* name) {
  Pylon::CEnumParameter param(nodemap, name);
  param.SetIntValue(g_value_get_int(value));
}

static gint get_int_property(GenApi::INodeMap& nodemap, const gchar* name) {
  Pylon::CIntegerParameter param(nodemap, name);
  return param.GetValue();
}

static gboolean get_bool_property(GenApi::INodeMap& nodemap,
                                  const gchar* name) {
  Pylon::CBooleanParameter param(nodemap, name);
  return param.GetValue();
}

static gfloat get_float_property(GenApi::INodeMap& nodemap, const gchar* name) {
  Pylon::CFloatParameter param(nodemap, name);
  return param.GetValue();
}

static gchar* get_str_property(GenApi::INodeMap& nodemap, const gchar* name) {
  Pylon::CStringParameter param(nodemap, name);
  return g_strdup(param.GetValue().c_str());
}

static gint get_enum_property(GenApi::INodeMap& nodemap, const gchar* name) {
  Pylon::CEnumParameter param(nodemap, name);
  return param.GetIntValue();
}

static void gst_pylon_camera_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec) {
  GstPylonCamera* self = (GstPylonCamera*)object;
  GstPylonCameraClass* klass = GST_PYLON_CAMERA_GET_CLASS(self);

  GST_OBJECT_LOCK(self);

  try {
    GenApi::INodeMap& nodemap = klass->camera->GetNodeMap();
    switch (pspec->value_type) {
      case G_TYPE_INT:
        set_int_property(nodemap, value, pspec->name);
        break;
      case G_TYPE_BOOLEAN:
        set_bool_property(nodemap, value, pspec->name);
        break;
      case G_TYPE_FLOAT:
        set_float_property(nodemap, value, pspec->name);
        break;
      case G_TYPE_STRING:
        set_str_property(nodemap, value, pspec->name);
        break;
      case G_TYPE_ENUM:
        set_enum_property(nodemap, value, pspec->name);
        break;
      default:
        std::string msg =
            "Unsupported GType: " + std::string(g_type_name(pspec->value_type));
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } catch (const Pylon::GenericException& e) {
    GST_ERROR("Unable to set pylon property \"%s\" on \"%s\": %s", pspec->name,
              klass->camera->GetDeviceInfo().GetFullName().c_str(),
              e.GetDescription());
  }

  GST_OBJECT_UNLOCK(self);
}

static void gst_pylon_camera_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec) {
  GstPylonCamera* self = (GstPylonCamera*)object;
  GstPylonCameraClass* klass = GST_PYLON_CAMERA_GET_CLASS(self);

  GST_OBJECT_LOCK(self);

  try {
    GenApi::INodeMap& nodemap = klass->camera->GetNodeMap();
    switch (pspec->value_type) {
      case G_TYPE_INT:
        g_value_set_int(value, get_int_property(nodemap, pspec->name));
        break;
      case G_TYPE_BOOLEAN:
        g_value_set_boolean(value, get_bool_property(nodemap, pspec->name));
        break;
      case G_TYPE_FLOAT:
        g_value_set_float(value, get_float_property(nodemap, pspec->name));
        break;
      case G_TYPE_STRING:
        g_value_set_string(value, get_str_property(nodemap, pspec->name));
        break;
      case G_TYPE_ENUM:
        g_value_set_int(value, get_enum_property(nodemap, pspec->name));
        break;
      default:
        std::string msg =
            "Unsupported GType: " + std::string(g_type_name(pspec->value_type));
        throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }
  } catch (const Pylon::GenericException& e) {
    GST_ERROR("Unable to get pylon property \"%s\" on \"%s\": %s", pspec->name,
              klass->camera->GetDeviceInfo().GetFullName().c_str(),
              e.GetDescription());
  }

  GST_OBJECT_UNLOCK(self);
}

static void gst_pylon_camera_init(GstPylonCamera* self) {}

GType gst_pylon_camera_register(
    const Pylon::CBaslerUniversalInstantCamera& camera) {
  GTypeInfo typeinfo = {
      sizeof(GstPylonCameraClass),
      NULL,
      NULL,
      (GClassInitFunc)gst_pylon_camera_class_init,
      NULL,
      &camera,
      sizeof(GstPylonCamera),
      0,
      (GInstanceInitFunc)gst_pylon_camera_init,
  };

  Pylon::String_t cam_name = camera.GetDeviceInfo().GetFullName();

  /* Convert camera name to a valid string */
  gchar* type_name = g_strcanon(g_strdup(cam_name.c_str()), VALID_CHARS, '_');

  GType type = g_type_from_name(type_name);
  if (!type) {
    type = g_type_register_static(G_TYPE_OBJECT, type_name, &typeinfo,
                                  static_cast<GTypeFlags>(0));
  }

  g_free(type_name);

  return type;
}
