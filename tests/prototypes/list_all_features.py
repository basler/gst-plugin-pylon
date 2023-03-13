#! /usr/bin/env python3

import featurewalker

import pypylon.pylon as py

def create_gst_feature_name(fn: featurewalker.FeatureNode, prefix: str):
    f_name = f"{prefix}::{fn.node.Node.GetName()}"
    for sel in fn.selectors:
        f_name += f"-{sel.value}"
    return f_name


tlf = py.TlFactory.GetInstance()


cam = py.InstantCamera(tlf.CreateFirstDevice())
cam.Open()


features = featurewalker.get_feature_nodes(
    cam.GetNodeMap(), with_selectors=True, only_implemented=True
)
for f in features:
    print(create_gst_feature_name(f, "cam"))

features = featurewalker.get_feature_nodes(
    cam.GetStreamGrabberNodeMap(), with_selectors=True, only_implemented=True
)
for f in features:
    print(create_gst_feature_name(f, "stream"))

features = featurewalker.get_feature_nodes(
    cam.GetTLNodeMap(), with_selectors=True, only_implemented=True
)
for f in features:
    print(create_gst_feature_name(f, "tl"))
