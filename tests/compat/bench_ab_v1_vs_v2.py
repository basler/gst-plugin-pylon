#!/usr/bin/env python3
"""Quantitative A/B performance & resource benchmark: v1.1.0 vs v2.0.0.

Measures under identical isolated CamEmu conditions:
  1. Cold-start pipeline time-to-first-frame (empty XDG_CACHE_HOME)
  2. Warm-start pipeline time-to-first-frame (populated XDG_CACHE_HOME)
  3. Cold & warm `gst-inspect-1.0 pylonsrc` wall time
  4. Sustained 60-frame capture wall time (640x480 GRAY8)
  5. Repeated NULL -> PLAYING -> NULL restart pipe-FD & VmRSS growth
"""

import argparse
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(SCRIPT_DIR))

from run_ab_compat import (  # noqa: E402
    EMU_SERIAL_0,
    build_v1_worktree,
    find_gi_python,
    make_isolated_env,
    resolve_build_artifacts,
    run_cmd,
)


def time_cmd(cmd: list[str], env: dict, timeout: int = 120) -> float:
    t0 = time.perf_counter()
    res = run_cmd(cmd, env=env, timeout=timeout)
    dt = time.perf_counter() - t0
    if res.returncode != 0:
        raise RuntimeError(f"Benchmark command failed (rc={res.returncode}): {cmd}\n{res.stderr}")
    return dt


def bench_version(
    label: str,
    art: dict,
    *,
    temp_root: Path,
    pylon_root: str,
    gi_python: str,
    serial: str,
    real_camera: bool,
    cold_runs: int,
    warm_runs: int,
) -> dict:
    base = temp_root / f"bench_{label}_{serial}"
    base.mkdir(parents=True, exist_ok=True)
    reg = base / "registry.bin"

    extra_env = {}
    camemu = "3"
    if real_camera:
        camemu = "0"
        extra_env["PYLONSRC_SKIP_TRANSPORT_LAYERS"] = ""

    # Prime the GStreamer plugin registry once so registry scan doesn't skew startup
    prime_cache = base / "prime_cache"
    prime_cache.mkdir()
    prime_env = make_isolated_env(
        art, cache_dir=prime_cache, registry_file=reg, pylon_root=pylon_root,
        camemu=camemu, extra=extra_env,
    )
    run_cmd(["gst-inspect-1.0", "fakesink"], env=prime_env)

    if real_camera:
        pipeline_1_frame = [
            "gst-launch-1.0",
            "-q",
            "pylonsrc",
            f"device-serial-number={serial}",
            "num-buffers=1",
            "!",
            "fakesink",
        ]
        pipeline_60_frames = [
            "gst-launch-1.0",
            "-q",
            "pylonsrc",
            f"device-serial-number={serial}",
            "num-buffers=30",
            "!",
            "fakesink",
        ]
    else:
        pipeline_1_frame = [
            "gst-launch-1.0",
            "-q",
            "pylonsrc",
            f"device-serial-number={serial}",
            "num-buffers=1",
            "!",
            "video/x-raw,format=GRAY8,width=640,height=480",
            "!",
            "fakesink",
        ]
        pipeline_60_frames = [
            "gst-launch-1.0",
            "-q",
            "pylonsrc",
            f"device-serial-number={serial}",
            "num-buffers=60",
            "!",
            "video/x-raw,format=GRAY8,width=640,height=480",
            "!",
            "fakesink",
        ]

    # 1. Cold-start time-to-first-frame
    cold_times = []
    for i in range(cold_runs):
        cdir = base / f"cold_ttff_{i}"
        cdir.mkdir()
        env = make_isolated_env(
            art, cache_dir=cdir, registry_file=reg, pylon_root=pylon_root,
            camemu=camemu, extra=extra_env,
        )
        cold_times.append(time_cmd(pipeline_1_frame, env))

    # 2. Warm-start time-to-first-frame (reuse populated cache)
    warm_cache = base / "warm_cache"
    warm_cache.mkdir()
    warm_env = make_isolated_env(
        art, cache_dir=warm_cache, registry_file=reg, pylon_root=pylon_root,
        camemu=camemu, extra=extra_env,
    )
    time_cmd(pipeline_1_frame, warm_env)  # warm up cache
    cache_mtimes_before = {
        p.name: p.stat().st_mtime_ns for p in warm_cache.rglob("*.config")
    }
    warm_times = [time_cmd(pipeline_1_frame, warm_env) for _ in range(warm_runs)]
    cache_mtimes_after = {
        p.name: p.stat().st_mtime_ns for p in warm_cache.rglob("*.config")
    }
    warm_rewrote_cache = cache_mtimes_before != cache_mtimes_after

    # 3. Cold & warm gst-inspect-1.0 pylonsrc
    inspect_cold_cache = base / "inspect_cold"
    inspect_cold_cache.mkdir()
    inspect_cold_env = make_isolated_env(
        art, cache_dir=inspect_cold_cache, registry_file=reg, pylon_root=pylon_root,
        camemu=camemu, extra=extra_env,
    )
    inspect_cold_s = time_cmd(["gst-inspect-1.0", "pylonsrc"], inspect_cold_env)
    inspect_warm_s = statistics.median(
        [time_cmd(["gst-inspect-1.0", "pylonsrc"], inspect_cold_env) for _ in range(3)]
    )

    # 4. Sustained capture
    sustained_s = statistics.median([time_cmd(pipeline_60_frames, warm_env) for _ in range(2 if real_camera else 3)])

    # 5. Restart resource leak check (CamEmu only — real cameras may not support 4096x4096 RGB)
    leak_script = PROJECT_ROOT / "tests" / "camemu" / "restart_resource_leak.py"
    leak_summary = "n/a (real camera)"
    if not real_camera and leak_script.is_file() and os.path.isdir("/proc/self/fd"):
        leak_res = run_cmd(
            [
                gi_python,
                str(leak_script),
                "--serial",
                serial,
                "--cycles",
                "4",
                "--max-pipe-growth",
                "8",
                "--max-rss-frames-per-cycle",
                "0.5",
                "--settle-ms",
                "300",
            ],
            env=warm_env,
            timeout=180,
        )
        if leak_res.returncode == 0:
            lines = [ln.strip() for ln in leak_res.stdout.splitlines() if ln.strip()]
            leak_summary = lines[-1] if lines else "PASS"
        else:
            leak_summary = f"FAIL (rc={leak_res.returncode})"

    return {
        "cold_ttff_ms": statistics.median(cold_times) * 1000.0,
        "warm_ttff_ms": statistics.median(warm_times) * 1000.0,
        "warm_rewrote_cache": warm_rewrote_cache,
        "inspect_cold_ms": inspect_cold_s * 1000.0,
        "inspect_warm_ms": inspect_warm_s * 1000.0,
        "sustained_ms": sustained_s * 1000.0,
        "sustained_frames": 30 if real_camera else 60,
        "restart_leak": leak_summary,
    }


def fmt_speedup(v1_ms: float, v2_ms: float) -> str:
    if v2_ms <= 0:
        return "n/a"
    ratio = v1_ms / v2_ms
    pct = ((v1_ms - v2_ms) / v1_ms) * 100.0 if v1_ms > 0 else 0.0
    sign = "-" if pct >= 0 else "+"
    return f"{ratio:.2f}x ({sign}{abs(pct):.1f}%)"


def main() -> int:
    parser = argparse.ArgumentParser(description="A/B performance benchmark: v1.1.0 vs v2.0.0")
    parser.add_argument("--baseline-ref", default="v1.1.0")
    parser.add_argument("--v1-worktree", type=Path)
    parser.add_argument("--v1-build", type=Path)
    parser.add_argument("--v2-build", type=Path)
    parser.add_argument(
        "--serial",
        action="append",
        dest="serials",
        help="Physical camera serial number(s) to benchmark (disables CamEmu)",
    )
    parser.add_argument("--cold-runs", type=int, default=3)
    parser.add_argument("--warm-runs", type=int, default=5)
    parser.add_argument("--output", type=Path, help="Optional path to write Markdown report")
    args = parser.parse_args()

    pylon_root = os.environ.get("PYLON_ROOT", "/opt/pylon")
    gi_python = find_gi_python()

    if args.v2_build:
        v2_build = args.v2_build.resolve()
    elif (PROJECT_ROOT / "build-v2" / "ext" / "pylon" / "libgstpylon.so").is_file():
        v2_build = (PROJECT_ROOT / "build-v2").resolve()
    else:
        v2_build = (PROJECT_ROOT / "build").resolve()

    v2_art = resolve_build_artifacts(v2_build)

    temp_root = Path(tempfile.mkdtemp(prefix="gstpylon-ab-bench-"))
    created_wt = False
    v1_wt = args.v1_worktree.resolve() if args.v1_worktree else (temp_root / "v1-src")
    v1_build = args.v1_build.resolve() if args.v1_build else (temp_root / "v1-build")

    real_camera = bool(args.serials)
    serials = args.serials if real_camera else [EMU_SERIAL_0]

    try:
        if not args.v1_worktree:
            created_wt = True
        build_v1_worktree(args.baseline_ref, v1_wt, v1_build, pylon_root)
        v1_art = resolve_build_artifacts(v1_build)

        md_lines = [
            "# A/B Performance & Resource Benchmark (`v1.1.0` vs `v2.0.0`)",
            "",
            f"- Mode: {'Physical hardware' if real_camera else 'PYLON_CAMEMU=3'}, `PYLON_ROOT={pylon_root}`",
            "",
        ]

        for serial in serials:
            print(f"Benchmarking v1.1.0 on serial={serial}...")
            m1 = bench_version(
                "v1", v1_art, temp_root=temp_root, pylon_root=pylon_root,
                gi_python=gi_python, serial=serial, real_camera=real_camera,
                cold_runs=args.cold_runs, warm_runs=args.warm_runs,
            )
            print(f"Benchmarking v2.0.0 on serial={serial}...")
            m2 = bench_version(
                "v2", v2_art, temp_root=temp_root, pylon_root=pylon_root,
                gi_python=gi_python, serial=serial, real_camera=real_camera,
                cold_runs=args.cold_runs, warm_runs=args.warm_runs,
            )

            rows = [
                ("Cold-start time-to-first-frame (ms)", f"{m1['cold_ttff_ms']:.1f}", f"{m2['cold_ttff_ms']:.1f}", fmt_speedup(m1["cold_ttff_ms"], m2["cold_ttff_ms"])),
                ("Warm-start time-to-first-frame (ms)", f"{m1['warm_ttff_ms']:.1f}", f"{m2['warm_ttff_ms']:.1f}", fmt_speedup(m1["warm_ttff_ms"], m2["warm_ttff_ms"])),
                ("Warm run re-wrote `.config` cache?", "YES" if m1["warm_rewrote_cache"] else "NO", "YES" if m2["warm_rewrote_cache"] else "NO", "Cache complete" if not m2["warm_rewrote_cache"] else "Cache miss"),
                ("`gst-inspect-1.0 pylonsrc` cold (ms)", f"{m1['inspect_cold_ms']:.1f}", f"{m2['inspect_cold_ms']:.1f}", fmt_speedup(m1["inspect_cold_ms"], m2["inspect_cold_ms"])),
                ("`gst-inspect-1.0 pylonsrc` warm (ms)", f"{m1['inspect_warm_ms']:.1f}", f"{m2['inspect_warm_ms']:.1f}", fmt_speedup(m1["inspect_warm_ms"], m2["inspect_warm_ms"])),
                (f"Sustained {m2['sustained_frames']} frames (ms)", f"{m1['sustained_ms']:.1f}", f"{m2['sustained_ms']:.1f}", fmt_speedup(m1["sustained_ms"], m2["sustained_ms"])),
            ]
            if not real_camera:
                rows.append(("4x 4096x4096 restart FD/RSS check", m1["restart_leak"], m2["restart_leak"], "PASS" if "FAIL" not in m2["restart_leak"] else "REGRESSION"))

            md_lines.extend([
                f"## Camera `{serial}`",
                "",
                "| Metric | `v1.1.0` | `v2.0.0` | Delta / Speedup |",
                "|--------|----------|----------|-----------------|",
            ])
            for metric, s1, s2, delta in rows:
                md_lines.append(f"| {metric} | {s1} | {s2} | {delta} |")
            md_lines.append("")

        report = "\n".join(md_lines) + "\n"
        print("\n" + report)
        if args.output:
            args.output.write_text(report, encoding="utf-8")
            print(f"Benchmark report written to {args.output}")
        return 0
    finally:
        if created_wt:
            run_cmd(["git", "worktree", "remove", "--force", str(v1_wt)], cwd=PROJECT_ROOT)
        shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
