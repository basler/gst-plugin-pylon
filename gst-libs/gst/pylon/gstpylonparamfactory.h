#ifndef GSTPYLONPARAMFACTORY_H
#define GSTPYLONPARAMFACTORY_H

#include <gst/pylon/gstpyloncache.h>
#include <gst/pylon/gstpylonincludes.h>

class GstPylonParamFactory {
 public:
  GstPylonParamFactory(GenApi::INodeMap &nodemap,
                       const std::string &device_fullname,
                       GstPylonCache &feature_cache)
      : nodemap(nodemap),
        device_fullname(device_fullname),
        feature_cache(feature_cache){};

  GParamSpec *make_param(GenApi::INode *node, GenApi::INode *selector,
                         guint64 selector_value);

 private:
  GParamSpec *gst_pylon_make_spec_int64(GenApi::INode *node);
  GParamSpec *gst_pylon_make_spec_selector_int64(GenApi::INode *node,
                                                 GenApi::INode *selector,
                                                 guint64 selector_value);
  GParamSpec *gst_pylon_make_spec_bool(GenApi::INode *node);
  GParamSpec *gst_pylon_make_spec_selector_bool(GenApi::INode *node,
                                                GenApi::INode *selector,
                                                guint64 selector_value);
  GParamSpec *gst_pylon_make_spec_double(GenApi::INode *node);
  GParamSpec *gst_pylon_make_spec_selector_double(GenApi::INode *node,
                                                  GenApi::INode *selector,
                                                  guint64 selector_value);
  GParamSpec *gst_pylon_make_spec_str(GenApi::INode *node);
  GParamSpec *gst_pylon_make_spec_selector_str(GenApi::INode *node,
                                               GenApi::INode *selector,
                                               guint64 selector_value);
  GType gst_pylon_make_enum_type(GenApi::INode *node);
  GParamSpec *gst_pylon_make_spec_enum(GenApi::INode *node);
  GParamSpec *gst_pylon_make_spec_selector_enum(GenApi::INode *node,
                                                GenApi::INode *selector,
                                                guint64 selector_value);

 private:
  GenApi::INodeMap &nodemap;
  const std::string &device_fullname;
  GstPylonCache &feature_cache;
};

#endif  // GSTPYLONPARAMFACTORY_H
