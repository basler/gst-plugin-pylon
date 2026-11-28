#!/usr/bin/env python3
"""Exercise and restart a live 16-camera compositor on Linux."""

import argparse
import os
from pathlib import Path
import resource
import time

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst

CAMERAS = 16
CAPTURE_LIMIT = 4096
WIDTH, HEIGHT = 1304, 984


class PipelineError(RuntimeError):
    pass


def fd_count():
    return len(list(Path("/proc/self/fd").iterdir()))


def make_grid():
    positions = []
    sources = []
    for i in range(CAMERAS):
        positions.extend((f"sink_{i}::xpos={i % 4 * 328}",
                          f"sink_{i}::ypos={i // 4 * 248}"))
        sources.append(
            f"pylonsrc device-serial-number=0815-{i:04d} user-set=Auto "
            "! video/x-raw,format=GRAY8,width=320,height=240 "
            f"! queue ! videoconvert ! grid.sink_{i}"
        )
    # Force GRAY8 after videoconvert: compositor itself only gained native
    # grayscale output around GStreamer 1.22, but Debian packaging CI still
    # builds against Ubuntu 22.04 / GStreamer 1.20.
    return Gst.parse_launch(
        f"compositor name=grid background=black {' '.join(positions)} "
        f"! videoconvert "
        f"! video/x-raw,format=GRAY8,width={WIDTH},height={HEIGHT},framerate=20/1 "
        "! appsink name=output sync=false max-buffers=2 drop=true "
        + " ".join(sources)
    )


def black_tiles(buffer):
    mapped, info = buffer.map(Gst.MapFlags.READ)
    if not mapped:
        raise PipelineError("Could not read the compositor output")
    try:
        pixels = info.data
        if len(pixels) != WIDTH * HEIGHT:
            raise PipelineError(f"Unexpected GRAY8 buffer size {len(pixels)}")
        black = []
        for index in range(CAMERAS):
            x = index % 4 * 328
            y = index // 4 * 248
            # Sample several rows away from tile edges. The CamEmu test
            # pattern spans the grayscale range and the background is zero.
            if all(max(pixels[row * WIDTH + x + 8 : row * WIDTH + x + 312]) <= 16
                   for row in range(y + 8, y + 232, 16)):
                black.append(f"0815-{index:04d}")
        return black
    finally:
        buffer.unmap(info)


def run_grid(pipeline):
    grid = pipeline.get_by_name("grid")
    output = pipeline.get_by_name("output")
    bus = pipeline.get_bus()
    frames = [0] * CAMERAS
    probes = []
    bad_timestamps = {}

    def count_frame(_pad, info, index):
        frames[index] += 1
        if frames[index] <= 12:
            pts = info.get_buffer().pts
            running_time = pipeline.get_current_running_time()
            if (pts != Gst.CLOCK_TIME_NONE and running_time != Gst.CLOCK_TIME_NONE
                    and pts > running_time + Gst.SECOND):
                bad_timestamps[index] = (pts, running_time)
        return Gst.PadProbeReturn.OK

    def check_error(timeout=0):
        message = bus.timed_pop_filtered(timeout, Gst.MessageType.ERROR)
        if message:
            error, debug = message.parse_error()
            raise PipelineError(f"{message.src.get_name()}: {error.message}\n{debug}")

    try:
        for i in range(CAMERAS):
            pad = grid.get_static_pad(f"sink_{i}")
            probe = pad.add_probe(Gst.PadProbeType.BUFFER, count_frame, i)
            probes.append((pad, probe))

        if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            check_error(Gst.SECOND)
            raise PipelineError("Could not start grid")

        deadline = time.monotonic() + 20
        output_frames = 0
        complete_frames = 0
        black = []
        while output_frames < 12 or min(frames) < 12 or complete_frames < 3:
            check_error()
            if bad_timestamps:
                raise PipelineError(f"Future camera timestamps (PTS, running time) {bad_timestamps}")
            if time.monotonic() >= deadline:
                raise PipelineError(
                    f"Grid incomplete. Inputs {frames}, output {output_frames}, "
                    f"black tiles {black}"
                )
            sample = output.emit("try-pull-sample", Gst.SECOND // 4)
            if sample:
                caps = sample.get_caps().get_structure(0)
                assert (caps.get_value("width"), caps.get_value("height")) == (WIDTH, HEIGHT)
                output_frames += 1
                black = black_tiles(sample.get_buffer())
                complete_frames = 0 if black else complete_frames + 1
        if bad_timestamps:
            raise PipelineError(f"Future camera timestamps (PTS, running time) {bad_timestamps}")
        print(f"Grid delivered {output_frames} composites, inputs {frames}, FDs {fd_count()}",
              flush=True)
    finally:
        pipeline.set_state(Gst.State.NULL)
        for pad, probe in probes:
            pad.remove_probe(probe)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soft-limit", type=int, default=CAPTURE_LIMIT)
    args = parser.parse_args()
    original = resource.getrlimit(resource.RLIMIT_NOFILE)
    if original[1] != resource.RLIM_INFINITY and original[1] < args.soft_limit:
        parser.exit(77, f"Need a hard open-file limit of at least {args.soft_limit}\n")

    # Configure CamEmu before GStreamer loads pylonsrc.
    os.environ["PYLON_CAMEMU"] = str(CAMERAS)
    os.environ["PYLONSRC_SKIP_TRANSPORT_LAYERS"] = "usb,gige,cxp,cameralink"
    Gst.init(None)

    try:
        resource.setrlimit(resource.RLIMIT_NOFILE, (args.soft_limit, original[1]))
        pipeline = make_grid()
        run_grid(pipeline)
        idle_fds = fd_count()
        for _ in range(2):
            run_grid(pipeline)
            after = fd_count()
            if after > idle_fds + 8:
                raise RuntimeError(f"Open descriptors grew from {idle_fds} to {after}")
        print(f"Grid teardown left {after} open descriptors", flush=True)
    finally:
        resource.setrlimit(resource.RLIMIT_NOFILE, original)


if __name__ == "__main__":
    main()
