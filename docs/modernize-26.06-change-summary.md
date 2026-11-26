# Pylon 26.06 Modernization Change Summary

This branch is a major-version modernization of `gst-plugin-pylon` for Pylon
Software Suite 26.x. It also folds in runtime hardening, a leaner internal
architecture, and regression coverage for the newer SDK baseline.

See also:

- [ADR 0001: Schema-Driven Pylon Introspection](adr/0001-schema-driven-pylon-introspection.md)
- [ADR 0002: Session Owns Camera Lifecycle](adr/0002-session-owns-camera-lifecycle.md)

## Why This Exists

Pylon 26.x changes the supported SDK baseline and the way Basler distributes CI
artifacts internally. Keeping this plugin on the previous pylon 6.x/7.x-era build
model would leave the project testing old runners, old dependency discovery, and
old GStreamer baselines that no longer match the supported pylon release train.

The dynamic property surface also needed cleanup. `pylonsrc` creates GObject
properties from live GenICam nodemaps, so small differences between the runtime
registration path and the `gst-inspect` path can become user-visible API drift.
Both paths now consume the same schema description and cache identity.

## Target Architecture

```text
GstPylonSrc (GStreamer element)
  ├── create / teardown / discard session
  ├── PylonInitialize / PylonTerminate
  └── child-proxy access to "cam" and "stream"
        │
        ▼
GstPylon (camera session)
  ├── gstpylonsession.cpp  open/select/config/identity
  ├── gstpyloncapture.cpp  capture/interrupt/meta
  └── gstpyloncaps.cpp     caps query/set
        │
        ├── GstPylonObject via GstPylonObjectSchema
        └── gstpyloninspect.cpp  (gst-inspect text only)
              ├── GstPylonCache          feature limits/flags
              └── GstPylonInspectCache   inspect blurbs
```

| Layer | Owns | Does not own |
| --- | --- | --- |
| `GstPylonSrc` | Element properties, state changes, Pylon runtime init/terminate | Nodemap details, schema construction |
| `GstPylon` | Open camera, handlers, children, applied config | Plugin lifetime of Pylon runtime |
| `GstPylonObjectSchema` | Display identity + cache key + nodemap view | Camera or cache storage lifetime |
| `GstPylonCache` | Feature limit/flag files | Inspect text |
| `GstPylonInspectCache` | Inspect blurb files | Feature probing |

## Session Ownership

`GstPylon` is a C++ session type with `Open`, `Start`, `Stop`,
`ApplySessionConfig`, `EnsureConfigured`, `Capture`, and identity helpers. The
public C API in `gstpylon.h` stays a thin wrapper.

`pylonsrc` start policy:

- same device → `gst_pylon_ensure_configured()`
- otherwise teardown (without terminating runtime), create, apply config
- failed start → discard session and `PylonTerminate` once

Device open policy:

- serial and/or user name without index opens directly (no full enumeration)
- index selection enumerates and keeps the collision retry loop

## Schema-Driven Dynamic Properties

`GstPylonObjectSchema` is built only by `gst_pylon_make_camera_schema()` /
`gst_pylon_make_stream_schema()`. Live children and inspect both use that shape.

`gst_pylon_object_new_for_schema()` takes the schema alone and uses
`schema.node_map()`. Children are parented with `gst_object_set_parent()` for
element-scoped error logging. Framerate authority is stored on the camera child
and queried by caps code; PFS load marks framerate configured explicitly.

## Dual Cache Model

- Feature cache (`GstPylonCache`): limits/flags, private file perms, invalid
  width/height ignored, bool/string flags-only entries
- Inspect cache (`GstPylonInspectCache`): full property blurbs with magic header
  and content length validation

`gst-inspect-1.0 pylonsrc` uses read-only limit queries by default.
`GST_PYLON_PROBE_LIMITS=1` restores full dynamic probing.

## Capture Interrupt State Machine

Image handler states: `Idle`, `FrameReady`, `Interrupted`, `Disconnected`.
Results: `ok`, `flushing`, `disconnected`. `unlock_stop` clears flush before
resume; disconnect is not conflated with flush.

## Camera Configuration Behavior

Caps negotiation honors AcquisitionFrameRate set via child property or PFS.
Optional `OffsetX`/`OffsetY` are skipped when absent. Bayer geometry does not
rely on `gst_video_info_from_caps()` for formats GStreamer may not parse as raw
video.

## Metadata And Python Binding Ownership

`block_id` stores transport `GetBlockID()`; `image_number` remains the grab-result
image number. `capture-error=keep` marks buffers corrupted. Python metadata
access returns an owned snapshot, including bool/string chunks.

## Build, CI, Jetson

- Version `2.0.0`; Meson `>= 1.4.0`; GStreamer `>= 1.20.0`; Pylon 26.x / SDK
  `>= 12.2`
- CI: Ubuntu 24.04 x86_64/aarch64, Windows, macOS via Conan `pylon-core`
- Debian targets: Ubuntu 22.04/24.04, Debian bookworm
- Jetson NVMM also detects L4T multimedia API paths

## Tests As Architecture Guardrails

- `tests/compat/`: golden `gst-inspect pylonsrc`
- `tests/camemu/` and `tests/check/pylon/pylonsrc_camemu.c`: session/capture
- `tests/check/generic/imagehandler.cpp`: flush vs disconnect

## User-Visible Compatibility Notes

Major version because the dependency baseline moves to Pylon 26.x. Within that
baseline the `pylonsrc` property surface is preserved; intentional inspect
changes update the golden file.

Notable behavior changes:

- faster, less invasive `gst-inspect-1.0 pylonsrc` by default
- `GST_PYLON_PROBE_LIMITS=1` opt-in for full limit probing
- direct serial/user-name selection; fast failure when missing
- same-device restarts skip redundant config when unchanged
- `capture-error=keep` marks buffers corrupted
- `BlockID` is transport block ID; Python meta is a safe snapshot

## Where To Look In The Code

| Concern | Primary files |
| --- | --- |
| Element orchestration | `ext/pylon/gstpylonsrc.cpp` |
| Session open/config | `ext/pylon/gstpylonsession.cpp` |
| Capture / interrupt | `ext/pylon/gstpyloncapture.cpp` |
| Caps | `ext/pylon/gstpyloncaps.cpp` |
| Session type | `ext/pylon/gstpylon-private.h` |
| Inspect text | `ext/pylon/gstpyloninspect.cpp` |
| Schema factories | `ext/pylon/gstpylonschema.*` |
| Child objects | `gst-libs/gst/pylon/gstpylonobject.*` |
| Feature / inspect caches | `gstpyloncache.*`, `gstpyloninspectcache.*` |
