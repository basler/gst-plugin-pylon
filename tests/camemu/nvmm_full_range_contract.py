#!/usr/bin/env python3
"""Colorimetry assumptions behind NVMM full-range YUV surface selection.

pylonsrc picks extended-range NVMM surfaces only when the negotiated caps say
full range. That relies on GStreamer parsing caps as follows:
  * YUY2 without colorimetry defaults to limited range
  * colorimetry=1:4:5:1 is full range

On Jetson, RUN_NVMM_FULL_RANGE=1 also runs a short NVMM pipeline with
full-range YUY2 caps.
"""

import argparse
import os
import sys

import gi

gi.require_version("Gst", "1.0")
gi.require_version("GstVideo", "1.0")
from gi.repository import Gst, GstVideo  # noqa: E402


def check(cond, msg):
    if not cond:
        print("FAIL:", msg, file=sys.stderr)
        return False
    print("ok:", msg)
    return True


def color_range(caps_str):
    info = GstVideo.VideoInfo.new_from_caps(Gst.Caps.from_string(caps_str))
    return info.colorimetry.range


def test_caps_colorimetry_policy():
    limited = getattr(GstVideo.VideoColorRange, "16_235")
    full = getattr(GstVideo.VideoColorRange, "0_255")
    ok = check(
        color_range("video/x-raw,format=YUY2,width=64,height=64") == limited,
        "bare YUY2 caps default to limited range",
    )
    ok &= check(
        color_range(
            "video/x-raw,format=YUY2,width=64,height=64,"
            "colorimetry=(string)1:4:5:1"
        )
        == full,
        "colorimetry 1:4:5:1 is full range",
    )
    return ok


def test_nvmm_pipeline_optional(serial):
    if os.environ.get("RUN_NVMM_FULL_RANGE") != "1":
        print("SKIP: NVMM pipeline (set RUN_NVMM_FULL_RANGE=1 on Jetson)")
        return True
    pipeline = Gst.parse_launch(
        "pylonsrc device-serial-number={serial} num-buffers=5 ! "
        "video/x-raw(memory:NVMM),format=YUY2,width=640,height=480,"
        "colorimetry=(string)1:4:5:1 ! fakesink".format(serial=serial)
    )
    if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
        pipeline.set_state(Gst.State.NULL)
        return check(False, "NVMM full-range pipeline starts")
    msg = pipeline.get_bus().timed_pop_filtered(
        15 * Gst.SECOND, Gst.MessageType.EOS | Gst.MessageType.ERROR
    )
    pipeline.set_state(Gst.State.NULL)
    if msg is not None and msg.type == Gst.MessageType.ERROR:
        err, dbg = msg.parse_error()
        print("error:", err, dbg, file=sys.stderr)
    return check(
        msg is not None and msg.type == Gst.MessageType.EOS,
        "NVMM full-range YUY2 pipeline reaches EOS",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", default="0815-0000")
    args = parser.parse_args()
    Gst.init(None)

    ok = test_caps_colorimetry_policy()
    ok &= test_nvmm_pipeline_optional(args.serial)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
