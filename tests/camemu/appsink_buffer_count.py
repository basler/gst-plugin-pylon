#!/usr/bin/env python3
"""Verify pylonsrc delivers the requested number of buffers to appsink."""

import argparse
import sys

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--buffers", type=int, required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    Gst.init(None)

    pipeline = Gst.parse_launch(
        "pylonsrc device-serial-number={serial} num-buffers={buffers} "
        "! video/x-raw,format=GRAY8,width=640,height=480 "
        "! appsink name=sink emit-signals=false sync=false".format(
            serial=args.serial,
            buffers=args.buffers,
        )
    )
    appsink = pipeline.get_by_name("sink")
    if appsink is None:
        print("appsink not found", file=sys.stderr)
        return 1

    pipeline.set_state(Gst.State.PLAYING)
    received = 0
    try:
        while received < args.buffers:
            sample = appsink.emit("try-pull-sample", 5 * Gst.SECOND)
            if sample is None:
                print(
                    "timed out after receiving {} of {} buffers".format(
                        received, args.buffers
                    ),
                    file=sys.stderr,
                )
                return 1
            received += 1
        return 0
    finally:
        pipeline.set_state(Gst.State.NULL)


if __name__ == "__main__":
    sys.exit(main())
