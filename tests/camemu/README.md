# Camemu functional test suite

End-to-end tests for `pylonsrc` using Basler camera emulators (`PYLON_CAMEMU`).
No physical camera is required.

## Requirements

- Built plugin (`ninja -C build`)
- Pylon 26.x SDK at `PYLON_ROOT` (default `/opt/pylon`)
- `PYLON_CAMEMU=3` exposes three emulators (`0815-0000`, `0815-0001`, `0815-0002`)

Tests select devices by **serial number** so they remain stable when real cameras
are also attached to the host.

## Run

```bash
PYLON_ROOT=/opt/pylon ninja -C build test
```

Or only the camemu suite:

```bash
PYLON_ROOT=/opt/pylon PYLON_CAMEMU=3 ./tests/camemu/run_camemu_tests.sh
```

## Coverage

| Area | Tests |
|------|-------|
| Plugin load | `gst-inspect pylonsrc`, SDK version, child proxies |
| Device selection | serial, index, ambiguous-device error, wrong-serial fast failure |
| Formats | GRAY8, Bayer, framerate caps |
| Configuration | `user-set=Auto`, `enable-correction` |
| Pipeline | queue, videoconvert, sequential open/close |
| Buffers | PyGObject appsink count (runs in CI; optional for local setups without PyGObject) |

Gstcheck unit tests in `tests/check/pylon/pylonsrc_camemu.c` cover element
creation, state transitions, serial property, selected-device playback, and
ambiguous-device failure. Buffer and format coverage is in the shell suite
above.
