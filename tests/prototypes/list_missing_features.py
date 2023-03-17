#! /usr/bin/env python3
# Copyright (C) 2023 Basler AG
#
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#     1. Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#     2. Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#     3. Neither the name of the copyright holder nor the names of
#        its contributors may be used to endorse or promote products
#        derived from this software without specific prior written
#        permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.
#


import featurewalker
import pypylon.pylon as py
import gi
import os

gi.require_version("Gst", "1.0")
from gi.repository import Gst

Gst.init()


def create_gst_feature_name(fn: featurewalker.FeatureNode, prefix: str):
    f_name = f"{prefix}::{fn.node.Node.GetName()}"
    for sel in fn.selectors:
        f_name += f"-{sel.value}"
    return f_name

def patch_to_gst_name_convention(name: str):
    return name.replace("_", "-")

def get_pylon_features(device_index:int = 0):
    tlf = py.TlFactory.GetInstance()
    devs = tlf.EnumerateDevices()
    # capture all_features
    cam = py.InstantCamera(tlf.CreateDevice(devs[device_index]))
    cam.Open()

    features = featurewalker.get_feature_nodes(
        cam.GetNodeMap(), with_selectors=True, only_implemented=True
    )
    cam_features = []
    for f in features:
        cam_features.append(create_gst_feature_name(f, "cam"))


    features = featurewalker.get_feature_nodes(
        cam.GetStreamGrabberNodeMap(), with_selectors=True, only_implemented=True
    )

    stream_features = []
    for f in features:
        stream_features.append(create_gst_feature_name(f, "stream"))

    features = featurewalker.get_feature_nodes(
        cam.GetTLNodeMap(), with_selectors=True, only_implemented=True
    )
    tl_features = []
    for f in features:
        tl_features.append(create_gst_feature_name(f, "tl"))

    cam.Close()

    return cam_features, stream_features, tl_features


def get_gst_features(device_index:int = 0):
    pipe = Gst.parse_launch(f"pylonsrc name=src device-index={device_index} ! fakesink")
    pylon = pipe.get_by_name("src")
    cam = pylon.get_child_by_name("cam")
    stream = pylon.get_child_by_name("stream")


    gst_cam_features = []
    for f in cam.props:
        gst_cam_features.append(f"cam::{f.name}")


    gst_stream_features = []
    for f in stream.props:
        gst_stream_features.append(f"stream::{f.name}")

    return gst_cam_features, gst_stream_features, []


print("MISSING FEATURES")
pylon_features = get_pylon_features(0)
gst_features = get_gst_features(0)

for p_feat, g_feat in zip(pylon_features, gst_features):
    for f in [x for x in p_feat if patch_to_gst_name_convention(x) not in set(g_feat)]:
        print(f)

