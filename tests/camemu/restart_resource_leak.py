#!/usr/bin/env python3
"""Validate pylonsrc resource cleanup across pipeline restart cycles.

Uses PYLON_CAMEMU with large ``4096x4096 RGB`` frames (~50 MiB each) so a
leaked grab result is visible in RSS. Checks:

1. ``pipe:[...]`` FD count must not grow linearly with NULL→PLAYING→NULL
   cycles (InstantCamera / GenTL FD leak when ``gstream_grabber`` was not
   unref'd on free; ~30 pipes/cycle).
2. Abrupt stop while a frame is pending in the image handler must not grow
   RSS by ~one frame per cycle (grab-thread vs unlock race).
3. Clean EOS restarts must also keep pipe FDs and RSS stable.

Exit codes:
  0  pass
  1  failure / timeout
  2  environment error
"""

from __future__ import annotations

import argparse
import ctypes
import gc
import os
import sys
import time
from pathlib import Path

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402

# Default camemu stress geometry: large enough that one leaked GrabResult is
# obvious in VmRSS (~50 MiB for RGB). Downsized automatically on hosts with
# little free RAM (e.g. Jetson Orin NX 8 GB with DeepStream).
DEFAULT_WIDTH = 4096
DEFAULT_HEIGHT = 4096
DEFAULT_FORMAT = "RGB"


def mem_available_bytes() -> int | None:
    path = Path("/proc/meminfo")
    if not path.is_file():
        return None
    for line in path.read_text().splitlines():
        if line.startswith("MemAvailable:"):
            return int(line.split()[1]) * 1024
    return None


def default_stress_geometry() -> tuple[int, int]:
    """Pick 4096² when RAM allows; otherwise a smaller RGB frame."""
    available = mem_available_bytes()
    if available is None or available >= 2 * 1024 * 1024 * 1024:
        return DEFAULT_WIDTH, DEFAULT_HEIGHT
    if available >= 768 * 1024 * 1024:
        print(
            f"low MemAvailable ({available / (1024 * 1024):.0f} MiB); "
            "using 1920x1080 RGB for restart leak test",
            flush=True,
        )
        return 1920, 1080
    print(
        f"low MemAvailable ({available / (1024 * 1024):.0f} MiB); "
        "using 640x480 RGB for restart leak test",
        flush=True,
    )
    return 640, 480


def caps_string(width: int, height: int, fmt: str) -> str:
    return f"video/x-raw,format={fmt},width={width},height={height}"


def frame_bytes(width: int, height: int, fmt: str) -> int:
    bpp = {
        "RGB": 3,
        "BGR": 3,
        "RGBx": 4,
        "BGRx": 4,
        "RGBA": 4,
        "BGRA": 4,
        "GRAY8": 1,
    }.get(fmt)
    if bpp is None:
        raise ValueError(f"unsupported format for size estimate: {fmt}")
    return width * height * bpp


def count_pipe_fds(pid: int | None = None) -> int:
    """Count open file descriptors that are anonymous pipes."""
    fd_dir = Path("/proc") / str(pid or os.getpid()) / "fd"
    pipes = 0
    try:
        entries = list(fd_dir.iterdir())
    except OSError as exc:
        raise RuntimeError(f"cannot list {fd_dir}: {exc}") from exc

    for entry in entries:
        try:
            target = os.readlink(entry)
        except OSError:
            continue
        if target.startswith("pipe:"):
            pipes += 1
    return pipes


def rss_bytes() -> int:
    """Current VmRSS in bytes from /proc/self/status."""
    for line in Path("/proc/self/status").read_text().splitlines():
        if line.startswith("VmRSS:"):
            # Field is in kB.
            return int(line.split()[1]) * 1024
    raise RuntimeError("VmRSS not found in /proc/self/status")


def trim_allocator(settle_s: float) -> None:
    """Return free heap pages to the OS so leaked C++ buffers show in RSS."""
    gc.collect()
    try:
        libc = ctypes.CDLL("libc.so.6")
        libc.malloc_trim(0)
    except OSError:
        pass
    time.sleep(settle_s)


def wait_state(element: Gst.Element, state: Gst.State, timeout_s: float = 15.0) -> None:
    ret = element.set_state(state)
    if ret == Gst.StateChangeReturn.FAILURE:
        raise RuntimeError(f"set_state({state.value_nick}) failed")
    if ret == Gst.StateChangeReturn.ASYNC:
        ret, reached, pending = element.get_state(int(timeout_s * Gst.SECOND))
        if ret != Gst.StateChangeReturn.SUCCESS or reached != state:
            raise RuntimeError(
                f"wait for {state.value_nick} failed "
                f"(ret={ret.value_nick}, reached={reached.value_nick}, "
                f"pending={pending.value_nick})"
            )


def run_eos_cycle(serial: str, num_buffers: int, caps: str) -> None:
    """One full start → EOS → NULL cycle (clean shutdown path)."""
    pipeline = Gst.parse_launch(
        f"pylonsrc device-serial-number={serial} num-buffers={num_buffers} "
        f"! {caps} ! fakesink sync=false"
    )
    try:
        wait_state(pipeline, Gst.State.PLAYING)
        bus = pipeline.get_bus()
        msg = bus.timed_pop_filtered(
            60 * Gst.SECOND,
            Gst.MessageType.EOS | Gst.MessageType.ERROR,
        )
        if msg is None:
            raise RuntimeError("timed out waiting for EOS")
        if msg.type == Gst.MessageType.ERROR:
            err, debug = msg.parse_error()
            raise RuntimeError(f"bus ERROR: {err.message} ({debug})")
        wait_state(pipeline, Gst.State.NULL)
    finally:
        pipeline.set_state(Gst.State.NULL)
        pipeline.get_state(10 * Gst.SECOND)
        del pipeline


def run_abrupt_stop_cycle(serial: str, caps: str, pending_wait_s: float) -> None:
    """Stop while a grabbed frame is likely pending in the image handler.

    Pull one buffer, then leave appsink full (``drop=false``, ``max-buffers=1``)
    so basesrc blocks on the next push and ``OnImageGrabbed`` can stash another
    frame. Sleep briefly, then go to NULL to hit the unlock/interrupt path.
    """
    pipeline = Gst.parse_launch(
        f"pylonsrc device-serial-number={serial} "
        f"! {caps} "
        f"! appsink name=sink emit-signals=false sync=false "
        f"drop=false max-buffers=1"
    )
    appsink = pipeline.get_by_name("sink")
    if appsink is None:
        raise RuntimeError("appsink not found")
    try:
        wait_state(pipeline, Gst.State.PLAYING)
        sample = appsink.emit("try-pull-sample", 15 * Gst.SECOND)
        if sample is None:
            raise RuntimeError("timed out waiting for first buffer")
        time.sleep(pending_wait_s)
        wait_state(pipeline, Gst.State.NULL)
    finally:
        pipeline.set_state(Gst.State.NULL)
        pipeline.get_state(10 * Gst.SECOND)
        del pipeline


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True, help="camemu serial number")
    parser.add_argument(
        "--cycles",
        type=int,
        default=10,
        help="restart cycles per scenario (default: 10)",
    )
    parser.add_argument(
        "--eos-buffers",
        type=int,
        default=2,
        help="num-buffers for the clean EOS scenario (default: 2)",
    )
    parser.add_argument("--width", type=int, default=None)
    parser.add_argument("--height", type=int, default=None)
    parser.add_argument("--format", default=DEFAULT_FORMAT)
    parser.add_argument(
        "--max-pipe-growth",
        type=int,
        default=8,
        help=(
            "allowed absolute growth in pipe:[...] FDs over each scenario "
            "(default: 8; pre-fix was ~30 per cycle)"
        ),
    )
    parser.add_argument(
        "--max-rss-frames",
        type=float,
        default=0.5,
        help=(
            "allowed RSS growth as a fraction of one frame buffer over each "
            "scenario after malloc_trim (default: 0.5; a real grab-result leak "
            "is ~1.0 frame per abrupt-stop cycle)"
        ),
    )
    parser.add_argument(
        "--pending-wait-ms",
        type=int,
        default=300,
        help="wait after first buffer before abrupt NULL (default: 300)",
    )
    parser.add_argument(
        "--settle-ms",
        type=int,
        default=250,
        help="settle time after NULL before sampling FDs/RSS (default: 250)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.cycles < 2:
        print("--cycles must be >= 2", file=sys.stderr)
        return 2
    if not Path("/proc/self/fd").is_dir():
        print("FAIL: /proc/self/fd is required", file=sys.stderr)
        return 2

    width = args.width
    height = args.height
    if width is None or height is None:
        auto_w, auto_h = default_stress_geometry()
        width = auto_w if width is None else width
        height = auto_h if height is None else height

    caps = caps_string(width, height, args.format)
    one_frame = frame_bytes(width, height, args.format)
    max_rss_growth = int(args.max_rss_frames * one_frame)
    settle_s = args.settle_ms / 1000.0
    pending_wait_s = args.pending_wait_ms / 1000.0

    Gst.init(None)

    print(
        f"using caps {caps} (~{one_frame / (1024 * 1024):.1f} MiB/frame); "
        f"max_rss_growth={max_rss_growth / (1024 * 1024):.1f} MiB"
    )

    # Warmup so one-time GStreamer/Pylon/allocator cost is not counted.
    run_eos_cycle(args.serial, args.eos_buffers, caps)
    trim_allocator(settle_s)
    baseline_pipes = count_pipe_fds()
    baseline_rss = rss_bytes()
    print(
        f"baseline after warmup: pipe_fds={baseline_pipes} "
        f"rss={baseline_rss / (1024 * 1024):.1f} MiB"
    )

    failed = False

    # Scenario A: clean EOS restarts
    for _ in range(args.cycles):
        run_eos_cycle(args.serial, args.eos_buffers, caps)
        trim_allocator(settle_s)
    after_eos_pipes = count_pipe_fds()
    after_eos_rss = rss_bytes()
    eos_pipe_growth = after_eos_pipes - baseline_pipes
    eos_rss_growth = after_eos_rss - baseline_rss
    print(
        f"after {args.cycles} EOS restarts: "
        f"pipe_fds={after_eos_pipes} (growth={eos_pipe_growth}) "
        f"rss_growth={eos_rss_growth / (1024 * 1024):.1f} MiB "
        f"(~{eos_rss_growth / one_frame:.2f} frames)"
    )

    # Scenario B: abrupt stop with a pending handler frame
    for _ in range(args.cycles):
        run_abrupt_stop_cycle(args.serial, caps, pending_wait_s)
        trim_allocator(settle_s)
    after_abrupt_pipes = count_pipe_fds()
    after_abrupt_rss = rss_bytes()
    abrupt_pipe_growth = after_abrupt_pipes - after_eos_pipes
    abrupt_rss_growth = after_abrupt_rss - after_eos_rss
    total_pipe_growth = after_abrupt_pipes - baseline_pipes
    total_rss_growth = after_abrupt_rss - baseline_rss
    print(
        f"after {args.cycles} abrupt-stop cycles: "
        f"pipe_fds={after_abrupt_pipes} (growth={abrupt_pipe_growth}) "
        f"rss_growth={abrupt_rss_growth / (1024 * 1024):.1f} MiB "
        f"(~{abrupt_rss_growth / one_frame:.2f} frames)"
    )

    if eos_pipe_growth > args.max_pipe_growth:
        print(
            f"FAIL: EOS pipe FD growth {eos_pipe_growth} > {args.max_pipe_growth}",
            file=sys.stderr,
        )
        failed = True
    if abrupt_pipe_growth > args.max_pipe_growth:
        print(
            f"FAIL: abrupt-stop pipe FD growth {abrupt_pipe_growth} "
            f"> {args.max_pipe_growth}",
            file=sys.stderr,
        )
        failed = True
    if total_pipe_growth > args.max_pipe_growth * 2:
        print(
            f"FAIL: total pipe FD growth {total_pipe_growth} "
            f"> {args.max_pipe_growth * 2}",
            file=sys.stderr,
        )
        failed = True

    if eos_rss_growth > max_rss_growth:
        print(
            f"FAIL: EOS RSS growth {eos_rss_growth / (1024 * 1024):.1f} MiB "
            f"> {max_rss_growth / (1024 * 1024):.1f} MiB",
            file=sys.stderr,
        )
        failed = True
    if abrupt_rss_growth > max_rss_growth:
        print(
            f"FAIL: abrupt-stop RSS growth "
            f"{abrupt_rss_growth / (1024 * 1024):.1f} MiB "
            f"> {max_rss_growth / (1024 * 1024):.1f} MiB "
            f"(~{abrupt_rss_growth / one_frame:.2f} leaked frames)",
            file=sys.stderr,
        )
        failed = True

    if failed:
        # Also print to stdout so meson --print-errorlogs captures it when
        # the harness only forwards a truncated tail.
        print("FAIL: restart resource cleanup thresholds exceeded", flush=True)
        return 1

    print(
        f"PASS: restart resource cleanup "
        f"(pipes eos/abrupt/total="
        f"{eos_pipe_growth}/{abrupt_pipe_growth}/{total_pipe_growth}, "
        f"rss eos/abrupt/total MiB="
        f"{eos_rss_growth / (1024 * 1024):.1f}/"
        f"{abrupt_rss_growth / (1024 * 1024):.1f}/"
        f"{total_rss_growth / (1024 * 1024):.1f})"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # noqa: BLE001 - surface as test failure
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
