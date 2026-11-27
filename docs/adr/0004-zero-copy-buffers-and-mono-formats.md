# ADR 0004: Zero-Copy Buffer Ownership and High Bit-Depth Mono Formats

## Status

Accepted

## Context

`pylonsrc` wraps pylon grab buffers in `GstBuffer`s without copying. This has
three consequences that need explicit rules:

1. A wrapped buffer keeps a pylon grab buffer (and the pylon runtime) alive for
   as long as downstream holds it. A downstream element (queue, appsink,
   encoder) that retains many frames can pin every buffer in the stream
   grabber's pool (`stream::MaxNumBuffer`) and starve the camera.
2. Earlier v2.0.0 builds rescaled unpacked `Mono10`/`Mono12` to the full 16-bit
   range with a per-pixel copy. That made these formats the only ones that were
   not zero-copy, changed the sample values compared to the camera, and could
   not be reproduced by downstream code that knew the camera format.
3. Packed monochrome formats (`Mono10p`, `Mono12p`, `Mono12Packed`) are bit
   streams that GStreamer has no raw video format for.

## Decision

### Buffer ownership

- Every zero-copy `GstBuffer` owns a `GstPylonBufferRef` (one allocation per
  frame) holding, in destruction order: the grab result, the in-flight counter
  and the pylon runtime guard.
- `GstPylonSession` counts buffers handed downstream (`in_flight`). On start it
  derives a limit of `MaxNumBuffer - 2`. When the limit is reached the next
  frame is **copied** into a normal `GstBuffer` and the pylon buffer is
  returned immediately. The camera is never starved; the cost is one `memcpy`
  per frame while downstream is holding buffers.
- If downstream did not advertise `GstVideoMeta` in the allocation query and the
  camera pads rows (stride differs from the tight stride of the negotiated
  format), the frame is repacked into a tight buffer. Otherwise the stride is
  communicated through `GstVideoMeta` and `GstPylonMeta::stride`.
- `GstPylonMeta` has a transform function so the metadata survives
  `gst_buffer_copy()` and the starvation copy. Sub-buffer copies do not carry
  it.

### Mono formats

| Pylon                              | GStreamer   | Handling                                                  |
|------------------------------------|-------------|-----------------------------------------------------------|
| `Mono16`                           | `GRAY16_LE` | zero-copy                                                 |
| `Mono12`, `Mono10`                 | `GRAY16_LE` | zero-copy, values are LSB-aligned (0..4095 / 0..1023)     |
| `Mono12p`, `Mono12Packed`, `Mono10p` | `GRAY16_LE` | unpacked with `Pylon::CImageFormatConverter` (LSB-aligned `Mono16`) |

When the camera is already configured (PFS, user set) with one of these formats
and downstream negotiates `GRAY16_LE`, that format is kept. Otherwise the
preference order is `Mono16`, `Mono12`, `Mono10`, `Mono12p`, `Mono12Packed`,
`Mono10p`.

Consumers that want a full-range image can scale downstream (for example with
`videoconvert` / `videobalance`) or by reading the original bit depth from the
camera.

## Consequences

- `Mono10`/`Mono12` frames are no longer rescaled. The rescaling only existed in
  unreleased 2.0.0 development builds (v1.1.0 does not expose `GRAY16_LE`), so
  no released behaviour changes, but pipelines displaying 10/12-bit
  `GRAY16_LE` directly will look dark and need downstream scaling.
- Packed formats cost one unpack copy per frame; unpacked formats cost none.
- The pool-starvation guard changes the zero-copy guarantee into "zero-copy
  unless downstream pins nearly all camera buffers", which is the only
  behaviour that keeps the stream alive in that situation.
