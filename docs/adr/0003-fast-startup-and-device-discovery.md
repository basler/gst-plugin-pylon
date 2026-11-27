# ADR-0003: Fast Startup, Declarative Feature Priors, and Non-Intrusive Device Discovery

- **Status:** Accepted
- **Date:** 2026-04-02

## Context

Historically, `gst_pylon_src_class_init` initialized the Pylon runtime, enumerated every transport layer (including GigE broadcast discovery, USB3, and CoaXPress), opened connected cameras, walked their GenICam node maps, and formatted GObject property descriptions into the `cam` and `stream` blurbs. This caused several issues in production and OpenCV workflows:

1. **Startup latency (1–4+ seconds):** Every `gst-launch-1.0`, Python GStreamer app, or `cv2.VideoCapture("pylonsrc ! ...", cv2.CAP_GSTREAMER)` paid full transport-layer enumeration and camera-open overhead during GType class initialization—even before a single element property was set.
2. **Hardware contention:** Opening connected cameras during `class_init` could steal exclusive camera handles from other processes or fail spuriously while another pipeline was grabbing.
3. **Redundant `UserSetLoad` and enum `TLParamsLocked` probing:** Startup previously executed `UserSetLoad` twice (`Open()` + `ApplySessionConfig()`), and enum `GParamFlags` were re-probed via live `TLParamsLocked` writes even on warm cache hits.
4. **OpenCV / `autovideosrc` coexistence:** Any `GstDeviceProvider` registered above `GST_RANK_NONE` or opening cameras during `probe()` risks interfering with `cv2.VideoCapture(0)` or active streams.

## Decisions

### 1. Lazy `class_init` (`gst_pylon_should_inspect_devices`)
`gst_pylon_src_class_init` no longer initializes Pylon or enumerates hardware during normal pipeline execution. Hardware-backed `cam`/`stream` property blurb generation runs **only** when:
- `g_get_prgname()` starts with `gst-inspect` or `gst-plugins-doc-cache-generator`, or
- `GST_PYLON_INSPECT_DEVICES=1` is explicitly set.

Furthermore, `GstPylonInspectCache` is consulted per device regardless of how many cameras are connected.

### 2. Single-Pass Session Configuration & Cached Enum Flags
- `GstPylonSession::Create()` (`Open()`) applies `user-set` / `pfs-location` once before building the camera `GstPylonSchema`, eliminating the second `UserSetLoad` round-trip.
- `gst_pylon_query_feature_properties_flags()` caches enum `GParamFlags` in `GstPylonCache` and leverages declarative Gen2 feature priors (`gstpylonfeatureprior`) to avoid unnecessary `TLParamsLocked` register writes on Basler 2nd-generation firmware (`ace 2`, `dart M/R/E`, `boost`, `racer 2`).

### 3. Transport-Layer Filtering (`PYLONSRC_SKIP_TRANSPORT_LAYERS`)
`gst_pylon_enumerate_devices()` supports `PYLONSRC_SKIP_TRANSPORT_LAYERS` (and `GST_PYLON_SKIP_TL`, e.g., `gige,cxp`) using RAII `CTlFactory::ReleaseTl` cleanup so deployments with unused CTIs avoid GigE network timeouts or CXP board scans.

### 4. Metadata-Only `GstPylonDeviceProvider` at `GST_RANK_NONE`
- `pylondeviceprovider` enumerates `Pylon::DeviceInfoList_t` metadata without calling `camera.Open()`, so probing never disrupts active OpenCV or GStreamer sessions.
- Both `pylonsrc` and `pylondeviceprovider` remain at `GST_RANK_NONE` so `autovideosrc` and `cv2.VideoCapture(index)` are never hijacked, while applications can explicitly query `gst_device_provider_factory_get_by_name("pylondeviceprovider")`.
