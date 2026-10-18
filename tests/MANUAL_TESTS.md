# Manual regression tests for gst-plugin-pylon robustness

These scenarios are not fully covered by automated CI (camera / multi-process
required). Run after building and installing the plugin.

## Inspect must not wipe UserSets

1. Configure a camera with a non-Default UserSet and note a distinctive feature
   value (e.g. ExposureTime).
2. Run `gst-inspect-1.0 pylonsrc`.
3. Confirm the camera UserSet and feature value are unchanged.

## Pause / flush

```bash
gst-launch-1.0 pylonsrc ! videoconvert ! fakesink
```

While running, send the pipeline to PAUSED then PLAYING (e.g. via
`gst-launch` interactive mode or a short C app). The stream must resume
without EOS.

## Pipeline restart resource cleanup

Automated with camemu (no physical camera):

```bash
PYLON_ROOT=/opt/pylon PYLON_CAMEMU=3 \
  python3 tests/camemu/restart_resource_leak.py --serial 0815-0000
```

Or as part of the camemu suite (`restart_resource_cleanup`). The test uses
**4096×4096 RGB** (~50 MiB/frame) and loops `NULL → PLAYING → NULL` (clean EOS
and abrupt stop with a pending grab). It fails if `pipe:[...]` FDs grow beyond
a small budget or if VmRSS grows by more than half a frame per cycle after
`malloc_trim` (a real grab-result leak is ~1 frame per cycle).

## Property order (cam:: before userset/pfs)

```bash
gst-launch-1.0 pylonsrc cam::Gain=1 user-set=UserSet1 ! fakesink
```

Confirm UserSet1 is loaded (read back a UserSet1-only feature). Early `cam::`
must not permanently skip userset/PFS application.

## Disconnect

Unplug USB / disconnect GigE while streaming. Expect a bus ERROR (not a clean
EOS). Pipeline must not hang.

## capture-error=keep

Induce incomplete grabs (undersized GevSCPSPacketSize on GigE) with
`capture-error=keep`. Downstream buffers must have `GST_BUFFER_FLAG_CORRUPTED`.

## NVMM full-range YUV (Jetson)

Basler YUV is full range. With an explicit capsfilter, pylonsrc must allocate
extended-range NVMM surfaces (`YUYV_ER` / `UYVY_ER`) so `nvvidconv` does not
clamp to 16..235:

```bash
gst-launch-1.0 pylonsrc ! \
  "video/x-raw(memory:NVMM),format=YUY2,colorimetry=1:4:5:1" ! \
  nvvidconv ! "video/x-raw(memory:NVMM),format=NV12" ! fakesink
```

Caps → `NvBufSurfaceColorFormat` mapping (real DeepStream header, no GPU):

```bash
tools/test_nvmm_color_format_docker.sh
```

Full-range YUY2/UYVY must select `NVBUF_COLOR_FORMAT_*_ER`.

Optional Jetson pipeline (allocation only; set `RUN_NVMM_FULL_RANGE=1`):

```bash
RUN_NVMM_FULL_RANGE=1 PYLON_CAMEMU=3 \
  python3 tests/camemu/nvmm_full_range_contract.py --serial 0815-0000
```

Confirming no clamping still needs a sample comparison (Y below 16 after
`nvvidconv`).

## Multi-process open

Start two processes selecting the same serial. One must fail after retries
without freezing the other process's property access for >30s.
