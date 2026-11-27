# ADR 0002: Session Owns Camera Lifecycle

## Status

Accepted (updated for `GstPylonSession` C++ class and encapsulated locking)

## Context

`pylonsrc` must open cameras, apply user-set/PFS configuration, tear down on
failure, handle disconnect/auto-reconnect, and optionally reuse an already-open
device across GStreamer state changes. Pylon also requires balanced
`PylonInitialize` / `PylonTerminate` across every active session and every
in-flight zero-copy `GstBuffer`.

Before the v2.0.0 hardening cleanup, configuration branching lived partly in
`gstpylonsrc.cpp` and partly behind a C-style wrapper facade (`gstpylon.h` /
`gstpylon.cpp`), session fields were public in a `_GstPylon` struct, and
`GstPylonObjectPrivate` was exposed in a public header for direct mutation.

## Decision

- `GstPylonSession` (`ext/pylon/gstpylonsession.h`) is a C++ class with private
  state that owns the open camera session: device selection (`AttachAndOpen` /
  `ReleaseDevice`), child objects, applied-config tracking, capture handlers,
  caps negotiation/caching, and `ApplySessionConfig` / `EnsureConfigured`.
- `GstPylonSrc` holds `std::unique_ptr<GstPylonSession> session`, guarded by
  `session_lock`, and owns GStreamer state transitions.
- Pylon runtime lifetime is managed by a ref-counted `GstPylonRuntime` guard
  (`ext/pylon/gstpylonruntime.h`), held by each `GstPylonSession` (destroyed
  last) and by each in-flight zero-copy `GstBuffer` (`GstPylonBufferRef`), as
  well as `GstPylonDeviceProvider` and `gstpyloninspect`.
- Camera/nodemap access across `GstPylonSession` and its child `GstPylonObject`
  instances is serialized by a shared `std::shared_ptr<std::recursive_mutex>`
  (`camera_mutex`), acquired strictly after `session_lock` and before the image
  handler mutex.
- `GstPylonObjectPrivate` is private to `gstpylonobject.cpp`. External callers
  use typed accessors:
  - `gst_pylon_object_rebind_nodemap()`
  - `gst_pylon_object_get_dimension_cache()`
  - `gst_pylon_object_take_offset_cache()`
  - `gst_pylon_object_get_enable_correction()` / `gst_pylon_object_set_enable_correction()`
  - `gst_pylon_object_is_framerate_configured()` / `gst_pylon_object_mark_framerate_configured()`
- Child objects use `gst_object_set_parent()` for element-scoped error logging
  through `GST_OBJECT_PARENT`, not a custom owner pointer.

## Consequences

- `start` policy is: same device → `EnsureConfigured`; else replace session and
  apply config.
- Zero-copy `GstBuffer`s safely outlive `GstPylonSession` teardown and
  `GST_STATE_NULL` transitions without dangling Pylon runtime state.
- Session implementation is split across `gstpylonsession.cpp`,
  `gstpyloncapture.cpp`, and `gstpyloncaps.cpp` implementing `GstPylonSession`,
  with shared enums in `gstpylontypes.h`.
