# v2.0.0 Hardware & Jetson NVMM Backwards-Compatibility Sign-Off

`tests/compat/run_ab_compat.py` proves ABI, drop-in binary, GObject/ChildProxy,
caps fixation, cache migration, and performance compatibility against `v1.1.0`
using `PYLON_CAMEMU`. This checklist covers hardware-only paths that CamEmu
cannot exercise.

## Prerequisites

- Both `v1.1.0` and `v2.0.0` builds (or `.deb` packages) available on the test host.
- Run every scenario under `G_DEBUG=fatal-criticals` on `v2.0.0` to catch any
  GLib/GObject warnings or out-of-range `GParamSpec` defaults.

## 1. Physical USB3 Vision & GigE Vision Cameras (ace / ace 2 / dart)

| # | Scenario | Expected (`v2.0.0` vs `v1.1.0`) | Status |
|---|----------|----------------------------------|--------|
| 1.1 | Unconstrained pipeline (`pylonsrc device-serial-number=<SN> num-buffers=30 ! fakesink`) | Same negotiated format, resolution, and framerate; faster cold & warm startup | [ ] |
| 1.2 | `gst-inspect-1.0 pylonsrc` against live camera | Does not wipe active camera configuration (`UserSetLoad(Default)` not called); completes faster than `v1.1.0` | [ ] |
| 1.3 | `user-set=UserSet1` and `cam::Gain=... user-set=UserSet1` | UserSet applied cleanly once (no double `UserSetLoad`); final camera state matches `v1.1.0` | [ ] |
| 1.4 | `pfs-location=<file.pfs>` (including float parameters) | Loads without spurious float-verification errors; negotiated caps match PFS | [ ] |
| 1.5 | GigE Vision `Stream.TransmissionType` / `TriggerSelector` | Selector state preserved after introspection; multicast/unicast routing unchanged | [ ] |
| 1.6 | Hardware trigger + `AcquisitionFrameRate` via `cam::` or PFS | Frame rate preserved; frames arrive on trigger with valid `GstPylonMeta.block_id` (real transport BlockID) | [ ] |
| 1.7 | Multi-camera pipeline (2+ cameras in one process and in concurrent processes) | No `GstPylonObject` class collision, no cache corruption, no startup contention failure | [ ] |
| 1.8 | In-place cache upgrade (`~/.cache/gstpylon/` created by `v1.1.0`) | `v2.0.0` starts cleanly with `G_DEBUG=fatal-criticals` | [ ] |

## 2. Line-Scan Camera (No `OffsetX` / `OffsetY`)

| # | Scenario | Expected (`v2.0.0`) | Status |
|---|----------|---------------------|--------|
| 2.1 | `pylonsrc device-serial-number=<SN> num-buffers=20 ! fakesink` | Starts cleanly without failing on absent `OffsetX`/`OffsetY` nodes | [ ] |

## 3. NVIDIA Jetson / DeepStream (`memory:NVMM`)

| # | Scenario | Expected (`v2.0.0` vs `v1.1.0`) | Status |
|---|----------|----------------------------------|--------|
| 3.1 | `RUN_NVMM_FULL_RANGE=1 python3 tests/camemu/nvmm_full_range_contract.py` | PASS; YUY2/UYVY full-range colorimetry (`1:4:5:1`) preserved through `nvvidconv` | [ ] |
| 3.2 | `pylonsrc ! video/x-raw(memory:NVMM),format=YUY2 ! nvvidconv ! fakesink` | Same caps, zero CUDA/NvBufSurface errors, zero FD/surface leaks across 10 restarts | [ ] |
| 3.3 | `.deb` in-place upgrade (`1:1.1.0+pylon*` -> `1:2.0.0+pylon*`) | `dpkg -i` upgrades cleanly; existing user C/Python apps run without recompiling | [ ] |
| 3.4 | NVMM CPU-copy paths (compressed/unpacked formats into `memory:NVMM`) | `NvBufSurfaceSyncForDevice` flushes CPU writes before downstream VIC/GPU reads; no stale/corrupt frames | [ ] |
| 3.5 | NVMM buffer factory allocation failure (exhaustion / invalid params) | `AllocateBuffer` frees partially allocated `NvBufSurface` and throws `std::bad_alloc`; element posts error cleanly with zero FD/surface leaks | [ ] |
