# ADR 0001: Schema-Driven Pylon Introspection

## Status

Accepted

## Context

`pylonsrc` exposes camera and stream-grabber GenICam features as dynamic
GObject properties. Building that property surface requires a live camera
nodemap, feature-range introspection, enum registration, and cache lookup.

Historically the nodemap, device name, and cache were passed separately through
the object-registration, feature-walking, and parameter-factory layers. That made
it easy for runtime property installation and `gst-inspect` output to drift.

## Decision

Represent each dynamic property surface with a `GstPylonObjectSchema`. The
schema carries the display identity, cache identity, feature cache, and nodemap
used by both the live child object and the `gst-inspect` introspection path.

Cache keys include device identity, firmware, Pylon SDK version, and plugin
version because rendered introspection text contains device-specific names and
SDK-specific feature metadata.

## Consequences

- Camera and stream child objects install properties through the same schema
  shape used by introspection.
- Introspection can render camera and stream properties in a single device pass.
- Disk cache entries are treated as disposable optimization data. Empty,
  malformed, stale, or truncated cache files are ignored and rebuilt.
- The schema is an internal borrowed view. It must not be exposed as public ABI or
  stored beyond the class-initialization work that consumes it.
