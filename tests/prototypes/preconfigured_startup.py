#!/usr/bin/env python3
"""Compare eager and deferred GenICam discovery in the preconfigured prototype.

Pass an optimized build and optional physical camera serials. With no serials,
run three identical CamEmu cameras with distinct PFS configurations.
"""

import argparse
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time


def capture(serials, settings, expected_widths):
    import gi

    gi.require_version("Gst", "1.0")
    from gi.repository import Gst

    Gst.init(None)
    start = time.monotonic()
    pipeline = Gst.parse_launch(" ".join(
        f'pylonsrc name=src{i} device-serial-number={serial} {setting} '
        f'max-pending-frames=6 grab-timeout=4000 ! appsink name=sink{i} sync=false'
        for i, (serial, setting) in enumerate(zip(serials, settings))
    ))

    def pull(index):
        sample = pipeline.get_by_name(f"sink{index}").emit(
            "try-pull-sample", 5 * Gst.SECOND
        )
        if sample is None:
            message = pipeline.get_bus().pop_filtered(Gst.MessageType.ERROR)
            raise RuntimeError(message.parse_error() if message else "No sample received")
        return sample

    try:
        if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            message = pipeline.get_bus().pop_filtered(Gst.MessageType.ERROR)
            raise RuntimeError(message.parse_error() if message else "PLAYING failed")
        first = [pull(i) for i in range(len(serials))]
        first_ms = (time.monotonic() - start) * 1000
        caps = [sample.get_caps().get_structure(0) for sample in first]
        widths = [cap.get_value("width") for cap in caps]
        if expected_widths:
            assert widths == expected_widths, widths

        lookup_start = time.monotonic()
        cameras = []
        child_models = []
        for i in range(len(serials)):
            source = pipeline.get_by_name(f"src{i}")
            camera = source.get_child_by_name("cam")
            stream = source.get_property("stream")
            child_models.append(camera.get_property("DeviceModelName"))
            cameras.append(camera)
            assert stream.get_property("MaxNumBuffer") > 0
        lookup_ms = (time.monotonic() - lookup_start) * 1000

        # CamEmu reports monotonically increasing image offsets. Hardware may
        # use transport BlockIDs with different wrap semantics.
        frame_gaps = None
        if all(serial.startswith("0815-") for serial in serials):
            frame_gaps = 0
            assert len(set(child_models)) == 1, child_models
            gains = [camera.get_property("GainRaw") for camera in cameras]
            cameras[0].set_property("GainRaw", gains[0] + 1)
            assert [camera.get_property("GainRaw") for camera in cameras] == [
                gains[0] + 1, *gains[1:]
            ]

            for i, sample in enumerate(first):
                previous = sample.get_buffer().offset
                for _ in range(6):
                    offset = pull(i).get_buffer().offset
                    assert offset > previous, (serials[i], previous, offset)
                    frame_gaps += offset - previous - 1
                    previous = offset
        else:
            for i, sample in enumerate(first):
                assert pull(i).get_buffer().offset != sample.get_buffer().offset
        print(json.dumps(dict(first_ms=first_ms, lookup_ms=lookup_ms,
                              frame_gaps=frame_gaps, models=child_models, caps=[c.to_string() for c in caps])))
    finally:
        pipeline.set_state(Gst.State.NULL)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--pylon-root", type=Path, default=Path("/opt/pylon"))
    parser.add_argument("--serial", action="append")
    parser.add_argument("--camemu-count", type=int, default=3)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--pfs", type=Path, help="Use this PFS for each physical camera")
    parser.add_argument("--user-set", help="Also benchmark this explicit user set")
    args = parser.parse_args()

    build = args.build.resolve()
    serials = args.serial or [f"0815-{i:04d}" for i in range(args.camemu_count)]
    sdk_path = str(args.pylon_root / "lib")
    env = dict(os.environ, PYLON_CAMEMU="0" if args.serial else str(len(serials)),
               PYLONSRC_OPEN_RETRIES="0",
               GST_PLUGIN_PATH_1_0=str(build / "ext/pylon"),
               LD_LIBRARY_PATH=f"{build / 'gst-libs/gst/pylon'}:{sdk_path}",
               PYLONSRC_SKIP_TRANSPORT_LAYERS=(
                   "gige,cxp,cameralink" if args.serial else "usb,gige,cxp,cameralink"))

    with tempfile.TemporaryDirectory(prefix="gstpylon-preconfigured-") as temp:
        root = Path(temp)
        env["GST_REGISTRY_1_0"] = str(root / "registry.bin")
        subprocess.run(["gst-inspect-1.0", "fakesink"], env=env, check=True,
                       stdout=subprocess.DEVNULL)
        workflows = {}
        if args.user_set or args.serial:
            workflows["User set"] = ([f"user-set={args.user_set or 'Auto'}"] *
                                     len(serials), [])
        if args.pfs:
            workflows["PFS"] = ([f'pfs-location="{args.pfs.resolve()}"'] * len(serials), [])
        elif not args.serial:
            widths = [640 + 64 * index for index in range(len(serials))]
            paths = []
            for index, width in enumerate(widths):
                path = root / f"config{index}.pfs"
                path.write_text("# {05D8C294-F295-4dfb-9D01-096BD04049F4}\n"
                                "# GenApi persistence file (version 3.1.0)\n"
                                f"PixelFormat\tMono8\nWidth\t{width}\nHeight\t480\n")
                paths.append(f'pfs-location="{path}"')
            workflows["PFS"] = paths, widths

        for workflow, (settings, widths) in workflows.items():
            results = {}
            for run in range(args.runs):
                for fast in ([False, True] if run % 2 == 0 else [True, False]):
                    env["XDG_CACHE_HOME"] = str(root / f"{workflow}-{fast}-{run}")
                    env["GST_PYLON_PROTOTYPE_FAST_PRECONFIGURED"] = str(int(fast))
                    for phase in ("cold", "repeat"):
                        output = subprocess.check_output(
                            [sys.executable, __file__, "--capture",
                             json.dumps([serials, settings, widths])],
                            env=env, text=True, timeout=45)
                        data = json.loads(output)
                        print(workflow, "deferred" if fast else "eager", phase,
                              json.dumps(data), flush=True)
                        results.setdefault((fast, phase), []).append(data)
            for (fast, phase), samples in results.items():
                print(workflow, "deferred" if fast else "eager", phase,
                      "median all first samples / total late child lookup (ms)",
                      *(round(statistics.median(s[field] for s in samples), 1)
                        for field in ("first_ms", "lookup_ms")), flush=True)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--capture":
        capture(*json.loads(sys.argv[2]))
    else:
        main()
