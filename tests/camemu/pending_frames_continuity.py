#!/usr/bin/env python3
"""Verify max-pending-frames keeps every frame through a short consumer stall.

appsink with max-buffers=1 blocks the pylonsrc streaming thread while the
application does not pull, so a sleep here stalls the source. Buffer offsets
are pylon image numbers and must stay consecutive.

appsink and the blocked streaming thread already hold about two frames, so
the stall has to be longer than that: with max-pending-frames=1 a stall of
four frame periods loses a frame.
"""

import argparse
import sys
import time

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--max-pending-frames", type=int, default=6)
    parser.add_argument("--stall-frames", type=float, default=4.0)
    parser.add_argument("--buffers", type=int, default=40)
    return parser.parse_args()


def main():
    args = parse_args()
    Gst.init(None)

    pipeline = Gst.parse_launch(
        "pylonsrc device-serial-number={serial} max-pending-frames={pending} "
        "! video/x-raw,format=GRAY8,width=640,height=480 "
        "! appsink name=sink emit-signals=false sync=false max-buffers=1 "
        "drop=false".format(
            serial=args.serial,
            pending=args.max_pending_frames,
        )
    )
    appsink = pipeline.get_by_name("sink")
    if appsink is None:
        print("appsink not found", file=sys.stderr)
        return 1

    warmup = 10
    pipeline.set_state(Gst.State.PLAYING)
    offsets = []
    pts = []
    try:
        while len(offsets) < args.buffers:
            sample = appsink.emit("try-pull-sample", 5 * Gst.SECOND)
            if sample is None:
                print(
                    "timed out after {} buffers".format(len(offsets)),
                    file=sys.stderr,
                )
                return 1
            buf = sample.get_buffer()
            offsets.append(buf.offset)
            pts.append(buf.pts)
            if len(offsets) == warmup:
                period = (pts[-1] - pts[1]) / (warmup - 2) / Gst.SECOND
                stall = args.stall_frames * period
                print(
                    "frame period {:.1f} ms, stalling {:.1f} ms".format(
                        period * 1000, stall * 1000
                    )
                )
                time.sleep(stall)
    finally:
        pipeline.set_state(Gst.State.NULL)

    gaps = [
        (prev, cur) for prev, cur in zip(offsets, offsets[1:]) if cur != prev + 1
    ]
    if gaps:
        print("frames lost between offsets: {}".format(gaps), file=sys.stderr)
        return 1
    print("{} consecutive frames".format(len(offsets)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
