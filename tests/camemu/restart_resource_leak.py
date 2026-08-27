#!/usr/bin/env python3
"""Validate pylonsrc resource cleanup across pipeline restart cycles.

Uses PYLON_CAMEMU (no physical camera). Checks:

1. ``pipe:[...]`` FD count must not grow linearly with NULL→PLAYING→NULL
   cycles (regression for InstantCamera / GenTL FD leak when
   ``gstream_grabber`` was not unref'd on free; ~30 pipes/cycle).
2. Abrupt stop while streaming must succeed repeatedly (exercises the
   grab-thread vs unlock race that previously abandoned grab results).

Exit codes:
  0  pass
  1  failure / timeout
  2  configuration error
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402


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


def wait_state(element: Gst.Element, state: Gst.State, timeout_s: float = 10.0) -> None:
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


def run_eos_cycle(serial: str, num_buffers: int) -> None:
    """One full start → EOS → NULL cycle (clean shutdown path)."""
    # fakesink consumes buffers so EOS is reached without an appsink pull loop.
    pipeline = Gst.parse_launch(
        f"pylonsrc device-serial-number={serial} num-buffers={num_buffers} "
        f"! video/x-raw,format=GRAY8,width=640,height=480 "
        f"! fakesink sync=false"
    )
    try:
        wait_state(pipeline, Gst.State.PLAYING)
        bus = pipeline.get_bus()
        msg = bus.timed_pop_filtered(
            30 * Gst.SECOND,
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
        pipeline.get_state(5 * Gst.SECOND)
        del pipeline


def run_abrupt_stop_cycle(serial: str, buffers_before_stop: int) -> None:
    """Start streaming, consume a few frames, then stop (race path)."""
    pipeline = Gst.parse_launch(
        f"pylonsrc device-serial-number={serial} "
        f"! video/x-raw,format=GRAY8,width=640,height=480 "
        f"! appsink name=sink emit-signals=false sync=false "
        f"drop=true max-buffers=1"
    )
    appsink = pipeline.get_by_name("sink")
    if appsink is None:
        raise RuntimeError("appsink not found")
    try:
        wait_state(pipeline, Gst.State.PLAYING)
        received = 0
        while received < buffers_before_stop:
            sample = appsink.emit("try-pull-sample", 5 * Gst.SECOND)
            if sample is None:
                raise RuntimeError(
                    f"timed out after {received}/{buffers_before_stop} buffers"
                )
            received += 1
        # Abrupt teardown while the grab loop may still hold a pending frame.
        wait_state(pipeline, Gst.State.NULL)
    finally:
        pipeline.set_state(Gst.State.NULL)
        pipeline.get_state(5 * Gst.SECOND)
        del pipeline


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True, help="camemu serial number")
    parser.add_argument(
        "--cycles",
        type=int,
        default=20,
        help="number of restart cycles per scenario (default: 20)",
    )
    parser.add_argument(
        "--eos-buffers",
        type=int,
        default=5,
        help="num-buffers for the clean EOS scenario (default: 5)",
    )
    parser.add_argument(
        "--stop-after",
        type=int,
        default=3,
        help="buffers to pull before abrupt NULL in the race scenario (default: 3)",
    )
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
        "--settle-ms",
        type=int,
        default=50,
        help="sleep after each NULL before sampling FDs (default: 50)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.cycles < 2:
        print("--cycles must be >= 2", file=sys.stderr)
        return 2
    if not Path("/proc/self/fd").is_dir():
        print("FAIL: /proc/self/fd is required for pipe FD accounting", file=sys.stderr)
        return 2

    Gst.init(None)

    # Warmup: open/close once so one-time GStreamer/Pylon init is not counted.
    run_eos_cycle(args.serial, args.eos_buffers)
    time.sleep(args.settle_ms / 1000.0)
    baseline = count_pipe_fds()
    print(f"baseline pipe FDs after warmup: {baseline}")

    # Scenario A: clean EOS restarts
    for _ in range(args.cycles):
        run_eos_cycle(args.serial, args.eos_buffers)
        time.sleep(args.settle_ms / 1000.0)
    after_eos = count_pipe_fds()
    eos_growth = after_eos - baseline
    print(
        f"after {args.cycles} EOS restart cycles: "
        f"pipe FDs={after_eos} (growth={eos_growth})"
    )

    # Scenario B: abrupt stop while streaming (grab/unlock race)
    for _ in range(args.cycles):
        run_abrupt_stop_cycle(args.serial, args.stop_after)
        time.sleep(args.settle_ms / 1000.0)
    after_abrupt = count_pipe_fds()
    abrupt_growth = after_abrupt - after_eos
    total_growth = after_abrupt - baseline
    print(
        f"after {args.cycles} abrupt-stop cycles: "
        f"pipe FDs={after_abrupt} (growth={abrupt_growth}, "
        f"total_growth={total_growth})"
    )

    failed = False
    if eos_growth > args.max_pipe_growth:
        print(
            f"FAIL: EOS-restart pipe FD growth {eos_growth} "
            f"> max {args.max_pipe_growth}",
            file=sys.stderr,
        )
        failed = True
    if abrupt_growth > args.max_pipe_growth:
        print(
            f"FAIL: abrupt-stop pipe FD growth {abrupt_growth} "
            f"> max {args.max_pipe_growth}",
            file=sys.stderr,
        )
        failed = True
    if total_growth > args.max_pipe_growth * 2:
        print(
            f"FAIL: total pipe FD growth {total_growth} "
            f"> max {args.max_pipe_growth * 2}",
            file=sys.stderr,
        )
        failed = True

    if failed:
        return 1

    print(
        f"PASS: restart resource cleanup "
        f"(eos_growth={eos_growth}, abrupt_growth={abrupt_growth}, "
        f"total_growth={total_growth})"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # noqa: BLE001 - surface as test failure
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
