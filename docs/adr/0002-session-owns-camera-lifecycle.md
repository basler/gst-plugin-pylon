# ADR 0002: Session Owns Camera Lifecycle

## Status

Accepted

## Context

`pylonsrc` must open cameras, apply user-set/PFS configuration, tear down on
failure, and optionally reuse an already-open device across GStreamer state
changes. Pylon also requires balanced `PylonInitialize` / `PylonTerminate` at
the plugin edge.

Before the lean-up, configuration branching lived partly in `gstpylonsrc.cpp`
and partly in `GstPylon`, and child objects mutated session fields through raw
back-pointers (`framerate_configured*`, `owner`).

## Decision

- `GstPylon` owns the open camera session: device selection, child objects,
  applied-config tracking, capture handlers, and
  `ApplySessionConfig` / `EnsureConfigured`.
- `GstPylonSrc` owns GStreamer state transitions and Pylon runtime
  `PylonInitialize` / `PylonTerminate`. It creates, replaces, or reuses a
  session, then asks the session to ensure configuration.
- Child `GstPylonObject` instances record whether framerate was configured
  locally and expose that via
  `gst_pylon_object_is_framerate_configured()` /
  `gst_pylon_object_mark_framerate_configured()`.
- Child objects use `gst_object_set_parent()` for element-scoped error logging
  through `GST_OBJECT_PARENT`, not a custom owner pointer.

## Consequences

- `start` policy is: same device → `EnsureConfigured`; else replace session and
  apply config.
- Failed start cleanup remains at the element edge so runtime terminate happens
  exactly once.
- Session implementation is split across `gstpylonsession.cpp`,
  `gstpyloncapture.cpp`, and `gstpyloncaps.cpp` behind `gstpylon-private.h`.
