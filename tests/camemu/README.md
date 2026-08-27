# Camemu functional tests

End-to-end tests for `pylonsrc` using Basler camera emulators
(`PYLON_CAMEMU`). No physical camera is required.

## Requirements

- Built plugin, for example `ninja -C build`
- Pylon SDK at `PYLON_ROOT` (default `/opt/pylon`)
- `PYLON_CAMEMU=3` exposes emulators `0815-0000`, `0815-0001`, and
  `0815-0002`

The tests select devices by serial number so they stay stable when real cameras
are also attached to the host.

## Run

```bash
PYLON_ROOT=/opt/pylon ninja -C build test
```

Or only this suite:

```bash
PYLON_ROOT=/opt/pylon PYLON_CAMEMU=3 ./tests/camemu/run_camemu_tests.sh
```

## Coverage

| Area | Tests |
|------|-------|
| Plugin load | `gst-inspect pylonsrc`, static and child properties |
| Device selection | serial, index, ambiguous-device error, wrong-serial fast failure |
| Formats | GRAY8, Bayer, framerate caps |
| Configuration | `user-set=Auto`, `enable-correction` |
| Property order | `cam::` before `user-set`/`pfs-location` still applies final config |
| Pipeline | queue, videoconvert, sequential open/close |
| Buffers | PyGObject appsink count when PyGObject is available |
| Restart cleanup | `restart_resource_leak.py`: 4096x4096 RGB; pipe FD + RSS across EOS and abrupt-stop cycles |

See also [MANUAL_TESTS.md](../MANUAL_TESTS.md) for camera/multi-process scenarios.
