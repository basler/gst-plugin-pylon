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

Or as part of the camemu suite (`restart_resource_cleanup`). The test loops
`NULL → PLAYING → NULL` (clean EOS and abrupt stop-while-streaming) and fails
if `pipe:[...]` FDs in `/proc/self/fd` grow beyond a small budget.

On a physical camera, also watch RSS / heaptrack for frame-sized grab-result
leaks under full load (camemu frames are small, so FD growth is the primary
automated signal).

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

## Multi-process open

Start two processes selecting the same serial. One must fail after retries
without freezing the other process's property access for >30s.
