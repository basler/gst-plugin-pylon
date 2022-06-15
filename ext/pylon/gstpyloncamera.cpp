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

struct _GstPylonCamera {
  GObject parent;
};

/* prototypes */
static void gst_pylon_camera_install_properties(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera);
static void gst_pylon_camera_class_init(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera);
static void gst_pylon_camera_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec);
static void gst_pylon_camera_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec);

#define VALID_CHARS G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS

static void gst_pylon_camera_install_properties(
    GstPylonCameraClass* klass, Pylon::CBaslerUniversalInstantCamera* camera) {
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

  oclass->set_property = gst_pylon_camera_set_property;
  oclass->get_property = gst_pylon_camera_get_property;

  gst_pylon_camera_install_properties(klass, camera);
}

static void gst_pylon_camera_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec) {
  GstPylonCamera* self = (GstPylonCamera*)object;

  GST_OBJECT_LOCK(self);

  // Set property here

  GST_OBJECT_UNLOCK(self);
}

static void gst_pylon_camera_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec) {
  GstPylonCamera* self = (GstPylonCamera*)object;

  GST_OBJECT_LOCK(self);

  // Get property here

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
