# Architecture

This is a map of how `pylonsrc` is put together. Decisions and their reasoning
live in the [ADRs](adr/).

## Layers

```
 GstPylonSrc (ext/pylon/gstpylonsrc.cpp)      GStreamer element: properties, state
   |                                           changes, create(), allocation, bus
   |  unique_ptr, guarded by session_lock
   v
 GstPylonSession (gstpylonsession.{h,cpp},    Owns the open camera: device selection,
   gstpyloncaps.cpp, gstpyloncapture.cpp)      configuration, caps, capture, reconnect
   |        |            |
   |        |            +--> GstPylonImageHandler    grab-thread -> streaming-thread queue,
   |        |                 GstPylonDisconnectHandler arrival timestamps, disconnect/interrupt
   |        +--> GstPylonBufferFactory (sysmem / NVMM) allocation for zero-copy buffers
   v
 libgstpylon-1.0 (gst-libs/gst/pylon)         Reusable GenICam <-> GObject layer
   GstPylonObject / GstPylonParamFactory      child proxy objects ("cam", "stream", "tl", "device")
   gstpylonintrospection / gstpylonlimitprobe  property flags, limits, access
   gstpylonfeaturewalker / gstpylonschema      node map traversal and selector expansion
   gstpylonmeta                                GstPylonMeta (block id, chunks, offsets, stride)
   gstpyloncache                               persistent limit/flag cache

 GstPylonRuntime (gstpylonruntime.{h,cpp})     Ref-counted PylonInitialize/PylonTerminate
```

## Lifetime

- `GstPylonRuntime::Acquire()` returns a shared guard. Pylon is initialized
  when the first guard exists and terminated when the last one is released.
- The session holds a guard and is the last member destroyed. Every zero-copy
  `GstBuffer` holds one too (`GstPylonBufferRef`), so buffers may safely outlive
  the element.
- Device provider and `gst-inspect` helpers take their own guards.

## Threads and locks

| Thread            | Does                                                      |
|-------------------|-----------------------------------------------------------|
| Application       | property get/set, state changes, bus handling             |
| Streaming         | `create()` -> `Capture()`; waits on the image handler     |
| Pylon grab thread | `OnImageGrabbed` pushes into the handler queue            |
| Pylon event thread| `OnCameraDeviceRemoved` signals the disconnect handler    |

Lock order is always `session_lock` -> `camera_mutex` -> image handler mutex.
`camera_mutex` is a recursive mutex shared between the session and its child
`GstPylonObject`s, so property access from the application thread never races
with caps negotiation, reconnect or start/stop.

## Frame path

1. The grab thread timestamps the frame with the pipeline clock
   (`arrival_time`) and queues it (`max-pending-frames`).
2. `Capture()` waits for a frame, handles failed grabs according to
   `capture-error`, and resolves the payload (image, chunk, GenDC component,
   compressed).
3. Uncompressed raw images are wrapped zero-copy. Packed mono, compressed
   frames and frames produced under pool starvation are copied into a normal
   buffer (see [ADR 0004](adr/0004-zero-copy-buffers-and-mono-formats.md)).
4. `GstPylonMeta`, `GstVideoMeta`, PTS (arrival time), offsets and the pylon
   reference timestamp are attached by the element.

## Disconnect and reconnect

A device removal wakes the streaming thread. With `auto-reconnect=false` the
element posts an error. With `auto-reconnect=true` it posts a warning plus
`pylonsrc-reconnect` element messages (`reconnecting`, `reconnected` with the
attempt number) and retries every second for up to 15 attempts. The wait is
interruptible so a state change to `NULL` never blocks on it.

## Start and device open

- Restarting on the same device reuses the session and skips configuration
  when nothing changed.
- Otherwise the old session is torn down (the runtime guard stays held), a new
  one is created and the configuration is applied.
- A failed start discards the session and releases its runtime guard.
- A serial number or user-defined name without an index opens the device
  directly, without full enumeration. Index selection enumerates and keeps the
  collision retry loop.

## Caches

- `GstPylonCache` stores feature limits and flags per device, with private file
  permissions. Width and height entries are only reused for exact matches.
  Unavailable features are negative-cached, so warm starts do no
  re-introspection.
- `GstPylonInspectCache` stores full property blurbs, validated by a magic
  header and content length.
- `gst-inspect-1.0 pylonsrc` reads limits from the caches and does not probe
  the camera by default. `GST_PYLON_PROBE_LIMITS=1` restores full probing.

## Capture states

The image handler moves between `Idle`, `FrameReady`, `Interrupted` and
`Disconnected`. `Capture()` returns `ok`, `flushing` or `disconnected`. Unlock
clears the flush before resuming, and a disconnect is never reported as a flush.

## Where to look

| Question                                  | File                                    |
|-------------------------------------------|-----------------------------------------|
| How is a camera opened/configured?        | `gstpylonsession.cpp`                   |
| How are caps built from the camera?       | `gstpyloncaps.cpp`                      |
| How is a frame turned into a `GstBuffer`? | `gstpyloncapture.cpp`                   |
| How are GenICam features exposed?         | `gst-libs/gst/pylon/gstpylonobject.cpp`, `gstpylonintrospection.cpp` |
| Pixel format tables                       | `gst-libs/gst/pylon/gstpylonformatmapping.h` |
