#!/usr/bin/env python3
"""Pull buffers from pylonsrc via appsink and verify the buffer count."""

import argparse
import os
import sys

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", default="0815-0000")
    parser.add_argument("--buffers", type=int, default=10)
    args = parser.parse_args()

    Gst.init(None)

    pipeline = Gst.parse_launch(
        f"pylonsrc name=src device-serial-number={args.serial} num-buffers={args.buffers} "
        "! video/x-raw,format=GRAY8,width=640,height=480 "
        "! appsink name=sink emit-signals=false sync=false"
    )
    appsink = pipeline.get_by_name("sink")
    if appsink is None:
        print("appsink not found", file=sys.stderr)
        return 1

    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("failed to set PLAYING", file=sys.stderr)
        return 1

    received = 0
    while received < args.buffers:
        sample = appsink.emit("try-pull-sample", 10 * Gst.SECOND)
        if sample is None:
            break
        received += 1

    pipeline.set_state(Gst.State.NULL)

    if received != args.buffers:
        print(
            f"expected {args.buffers} buffers, got {received}",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
