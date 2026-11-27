# Changelog
All notable changes to this project will be documented in this file.

## [2.0.0] - Unreleased

### Added
- Modular `GstPylon` session architecture (`gstpylonsession`, `gstpyloncapture`,
  `gstpyloncaps`, `gstpyloninspect`, `gstpylonschema`) with ADRs 0001, 0002, and
  0003
- Lazy `class_init` (`gst_pylon_should_inspect_devices`) so normal pipeline and
  OpenCV startups skip Pylon runtime initialization and camera enumeration
  unless running `gst-inspect*` or `GST_PYLON_INSPECT_DEVICES=1`
- Declarative Basler Gen2 feature priors (`gstpylonfeatureprior`) and cached
  enum `GParamFlags` to eliminate redundant `TLParamsLocked` and invalidator
  probing on 2nd-generation cameras (`ace 2`, `dart`, `boost`, `racer 2`)
- Metadata-only `GstPylonDeviceProvider` (`pylondeviceprovider`) at
  `GST_RANK_NONE` for safe, non-blocking camera discovery without opening
  devices or hijacking OpenCV / `autovideosrc`
- Transport-layer filtering via `PYLONSRC_SKIP_TRANSPORT_LAYERS` /
  `GST_PYLON_SKIP_TL` (e.g. `gige,cxp`) with RAII `CTlFactory::ReleaseTl`
- High bit-depth pixel formats: `Mono10`/`Mono12`/`Mono16` → `GRAY16_LE`
  (zero-copy; 10/12-bit values stay LSB-aligned and are not rescaled), packed
  `Mono10p`/`Mono12p`/`Mono12Packed` unpacked to `GRAY16_LE`, and 10/12/16-bit
  Bayer (`bggr10le`, `bggr12le`, `bggr16le`, etc. on GStreamer ≥ 1.24)
- Pool-starvation guard: frames are copied when downstream holds more than
  `MaxNumBuffer - 2` pylon buffers, and padded frames are repacked when
  downstream has no `GstVideoMeta` support
- `GstPylonMeta` survives `gst_buffer_copy()`
- Ref-counted `GstPylonRuntime` guard: zero-copy buffers may outlive the element
  without leaving pylon initialized or terminated early
- Emulator-backed regression tests for state-change loops, buffers outliving the
  element, concurrent child-property access, disconnect/reconnect, arrival
  timestamps and allocation failures; opt-in `-Dtest-hooks=true`; ASan+UBSan+LSan
  and TSan CI jobs; `-Werror` GCC/Clang CI build
- `docs/architecture.md` and ADR 0004
- Basler Compression Beyond transparent decompression via
  `Pylon::CImageDecompressor` (#98, #99)
- Zero-copy GenDC container (`PayloadType_GenDC`) image component extraction and
  metadata resolution for GeV 3.0 GenDC-only cameras and USB3 Vision cameras
  with optional `GenDCStreamingMode` (`gstpylongendc`, `generic_gendc`)
- Multi-process camera startup coordination: direct filter open with bounded
  jittered retry on transient device contention while failing immediately on
  non-existent devices, plus process-safe atomic cache file commits
- GenICam `ICommand` execution via `execute-command` action signal on
  `pylonsrc` (supporting selector syntax such as `TriggerSoftware-FrameStart`,
  #58)
- Capture reliability controls: `grab-timeout` property (#45), structured
  `pylonsrc-capture-error` / `pylonsrc-reconnect` element bus messages (#17),
  and opt-in `auto-reconnect` recovery (#7)
- Dynamic runtime controls: runtime `AcquisitionFrameRate` updates refresh
  buffer duration and post `GST_MESSAGE_LATENCY` (#85), `ReverseX`/`ReverseY`
  live probing (#87), and PFS/UserSet-driven caps fixation (#47, #81)
- Dual cache model (`GstPylonCache` for limits/flags and `GstPylonInspectCache`
  for `gst-inspect` blurbs) and read-only `gst-inspect` by default
  (`GST_PYLON_PROBE_LIMITS=1` opt-in for full dynamic probing)
- Golden `gst-inspect` compatibility test suite (`tests/compat/`),
  `generic_featureprior`, `generic_meta`, and `pylon_pylonsrc_camemu` tests
- Automated `v1.1.0` vs `v2.0.0` A/B backwards-compatibility proof harness
  (`tests/compat/run_ab_compat.py`) with machine-checked intentional-delta
  ledger (`tests/compat/intentional_deltas_v2.json`), zero-recompile C/Python
  drop-in and `libgstpylon-1.0` ABI verification, `~/.cache/gstpylon` migration
  checks, A/B performance benchmark (`tests/compat/bench_ab_v1_vs_v2.py`,
  supporting both `PYLON_CAMEMU` and physical `--serial` cameras), hardware
  sign-off matrix (`tests/compat/HARDWARE_SIGNOFF.md`), and Linux x86_64 CI gate
- Jetson Multimedia API (`nvbufsurface.h`) fallback detection for NVMM builds
  without DeepStream
- Boolean and string chunk support in CPython `abi3` `pygstpylon`, plus Windows
  `.pyd` + `.dll`/`.pdb` release zip packaging and CI `clang-format` /
  ASan+UBSan checks

### Fixed
- Eliminated double `UserSetLoad` execution during session startup
- Restored GenICam selector state (including `Stream.TransmissionType`,
  `TriggerSelector`, `CounterSelector`, etc.) after property introspection and
  skipped selector writes on warm cache hits, fixing GigE Vision stream routing
  and reducing warm-start latency
- Registered all implemented `GenApi::IEnumeration` entries in `GEnumClass` and
  guarded unavailable/out-of-range `GParamSpec` defaults, eliminating GLib
  critical assertions on real USB3 and GigE Vision cameras
- Linked dynamic `libcudart` instead of `cudart_static` in NVMM/DeepStream
  builds and ensured `MEM_NVMM` handles GenDC and Compression Beyond payloads
- Made dynamic `GstPylonObject` private offset per-subclass and thread-safe for
  multi-camera pipelines, and cleaned up sessions opened in `NULL` state on
  finalize
- Optional `OffsetX`/`OffsetY` handling for line-scan cameras
- Direct camera open by serial number or user-defined name without full device
  enumeration
- Relaxed PFS nodemap float verification on load
- Preserved `AcquisitionFrameRate` set via child property or PFS for hardware
  triggering
- `GstPylonMeta.block_id` now stores transport `GetBlockID()` (falling back to
  `image_number` when the transport reports `UINT64_MAX`, matching `GstBuffer`
  offset)
- `capture-error=keep` marks delivered buffers with `GST_BUFFER_FLAG_CORRUPTED`
- Hardened NVMM `NvBufSurfaceAllocate` / `NvBufSurfaceMap` error handling,
  `NvBufSurfaceUnMap` on free, and strict `memory:NVMM` format filtering
- Fixed warm-run feature cache misses by narrowing `GstPylonCache` dimension
  validation to exact `Width`/`Height` matches (preventing false invalidation
  of read-only `SensorWidth`/`SensorHeight`) and negative-caching unavailable
  features and unselectable selector states with `flags = 0` so warm startups
  perform zero re-introspection and zero `.config` cache rewrites
- Fixed Hotdoc C++ source globs in `docs/meson.build`

### Changed
- Bumped project version to `2.0.0` and set explicit `cpp_std=c++17` in
  `meson.build`
- Buffer PTS is the arrival time captured in the pylon grab callback instead of
  the time the streaming thread dequeues the frame
- With `auto-reconnect=true` a camera disconnect is posted as a warning, not an
  error; the reconnect wait no longer delays a state change to `NULL`
- Replaced the C `gstpylon.h` facade with the `GstPylonSession` class and hid
  `GstPylonObjectPrivate`; limit probing moved to `gstpylonlimitprobe`
- The plugin version is the Meson project version (`VERSION`). Each build also
  reports its identity from `tools/build_id.py`: `<version>+g<commit>` for an
  untagged commit, `.dirty` for uncommitted tracked changes, and `+unknown`
  without git metadata. Debian packages use the same identity;
  `GST_PYLON_STRICT_VERSION=1` makes packaging refuse `+unknown` and `.dirty`
  builds. The git-describe `version.h` generation was removed
- Restarting `pylonsrc` on the same device reuses the session and skips
  configuration when nothing changed

## [1.1.0] - 2026-11-26

### Added
- `max-pending-frames` property on `pylonsrc` to queue up to *N* grabbed frames
  using `GrabStrategy_OneByOne` instead of keeping only the latest frame
  (`1` by default, preserving previous behavior)
- macOS (`arm64`) build and camera-emulator test support (`pylon` 26.06+)
- Universal Debian/Ubuntu binary packages built once per architecture (`amd64`,
  `arm64`) and verified across Ubuntu 22.04, Ubuntu 24.04, Debian 12
  (bookworm), and Debian 13 (trixie)
- CPython stable-ABI (`abi3`, Python 3.10+) `python3-pygstpylon` package so a
  single `.deb` works across distro Python 3.10–3.13+ runtimes
- NVIDIA Jetson `nvidia` Debian build profile supporting JetPack 6 / DeepStream
  6.4, 7.0, and 7.1 (with CUDA toolkit fallback when `cuda.pc` is absent)
- Per-package CycloneDX 1.6 build SBOMs (`*.cdx.json`) and Grype vulnerability
  scan summaries (`*.grype.txt`) published with GitHub Releases
- Expanded automated test suite covering NVMM colorimetry, pending-frame stall
  recovery, `pygstpylon` multi-stream metadata, and 4096×4096 pipeline-restart
  FD/RSS leak regression checks

### Fixed
- NVMM: YUY2 and UYVY caps with full-range colorimetry (`1:4:5:1`) now allocate
  extended-range surfaces (`NVBUF_COLOR_FORMAT_YUYV_ER` / `_UYVY_ER`) so
  downstream `nvvidconv` conversions do not clamp samples to `16..235`
- Fixed frame-buffer and GenTL pipe file-descriptor leaks across repeated
  `NULL → PLAYING → NULL` restarts and abrupt pipeline stops (distinguishing
  flush/unlock from device disconnect, unreffing the stream-grabber object, and
  destroying the `InstantCamera` device on teardown)
- Fixed `GstPylon` teardown ordering before `PylonTerminate` on start failures,
  freed `pfs-location` in `finalize`, and released grab results on NVMM CUDA
  copy failures
- Mapped `unlock` to `GST_FLOW_FLUSHING` (with `unlock_stop` clearing the
  interrupt) instead of prematurely signaling EOS
- Guarded `WaitForImage` against empty-queue wakeups in the image handler

### Changed
- Debian package versions now use epoch `1:` (`1:1.1.0+pylon<ver>`) to cleanly
  upgrade former distro-suffixed `1.0.0-1~...` packages, record the exact
  build-time SDK in `Depends: pylon (= <ver>)` and `Pylon-Built-Against`, and
  encode `+pylon<ver>` in the `.deb` filename
- Source builds via `PYLON_ROOT` continue to support `pylon` 7.1+ (and 6.x on
  `aarch64`) without requiring a `pylon` dpkg package
- Unified GitHub Actions CI builds and tests Linux (`x86_64`, `aarch64`),
  Windows (`MSVC`, GStreamer 1.26.9), macOS (`arm64`), Debian/Ubuntu packages,
  and DeepStream 6.4–7.1 against the latest `pylon` release

### Removed
- Unused `bindings/packaging/setup.py` (`pygstpylon` is built and installed by
  Meson)

## [1.0.0] - 2024-08-14
## Added
- added script to generate release notes
- added python example to show use of pylonmeta from pyth
  * fixes #114

### Fixed
- NVMM image width is in byte not pixels!
  * fixes #100
- removing source while active buffers in pipeline
  * fixes lifetime issue between buffers in pipeline and src
  * fixes #54
- Proper init/terminate of pylon sdk from gst-plugin-scanner
  * avoids output of an exception message after new install
- Removed unused pylonc library dependency

### Changed
- Only export external header files to access PylonMeta


## [0.7.3] - 2024-07-30
### Added
- publish artifacts on release page

### Fixed
- build debian package in case meson/ninja already installed

## [0.7.2] - 2024-07-12
### Changed
- add debian packaging
  * CI builds for std x86/aarch64 ubuntu and debian targets
  * Building deb packates for NVIDIA Jetson is documented in the README

### Fixed
- workaround for failed tests for Ubuntu 22.04
  * fixes #111
- fix nvmm error if no camera connected
  * fixes #88 

## [0.7.1] - 2024-06-06
### Changed
- check for cuda version >= 11 to enable nvmm code
  * nvmm support implementation is only compatible to cuda >= 11
  * fixes #60
- update documentation to build on Windows
  * fixes #68
- latency update
  * src now sends a message to recompute pipeline latency when it becomes known
  * fixed by #104

### Fixed
- float features working again
  * fixes #80
- build now works with the changes in meson >= 1.3.0
  * fixes #72
- build now works with the changes in meson >= 1.4.0
- fixed CI on ubuntu 24.04
- fixed invalid buffer size with enabled chunks ( non NVMM mode ) 
  * fixed by #101
- fixed possible segfault path in NVMM error path
  * fixed by #103

### Added
- Pylon 7.5
  * supports to build the plugin with pylon 7.5 now
- Pylon 7.4
  * supports to build the plugin with pylon 7.4 now

## [0.7.0] - 2023-05-08

### Changed
- offset x/y are cached when pipeline is not playing.
  * setting the offset an ROI configured via caps is possible now
  * fixes #44

### Added
- Restructing of buffer pool management to support platform specific optimal buffer types
- NVMM support
  * This feature is automatically enabled when both the CUDA library and the DeepStream library are installed on the system.
  * If enabled pylonsrc can directly generate output buffers into nvmm, that can be used by other nvidia elements.
  * Current restrictions:
    * only runs on NVIDIA Jetson at the moment

- Pylon 7.3
  * meson supports to build the plugin with pylon 7.3 now


## [0.6.2] - 2023-04-04

### Changed
- automatic rounding of values is default
  * behavioural change
  * if a value set via gstreamer is not valid for the camera
    it will be automaticaylly roundedi
  * this can be disabled by `enable-correction=false`

- Filter gev control features to speed up introspection
  * exclude gev control dependencies

## [0.6.1] - 2023-03-27

### Changed
- Filter event data nodes
  * exclude from registration until supported in gstreamer

### Fixed
- Process feature limits for ace gige
  * extend heuristics
  * fixes #37

## [0.6.0] - 2023-03-24

### Added
- Caching infrastructure to reduce runtime of property introspection process
  * ranges and access behaviour of features are saved during introspection
  * cache data is saved in [glib_user_cache_dir](https://docs.gtk.org/glib/func.get_user_cache_dir.html)/gstpylon/
- Python bindings to access pylon image metadata from python-gstreamer scripts
  * can be enabled during configuration using meson option `-Dpython-bindings=enabled`
- gstreamer property added to automatically round properties to the nearest valid camera values
  * `enable-correction=<true/false>`  activates automatic correction of values.
- git version is reflected in the plugin version string


### Changed
- Changed codebase from mixed c/c++ to c++
  * symbol default visibility 'hidden' on all platforms
- Exclude the following feature groups from introspection until properly supported
  * SequencerControl
  * FileAccessControl
  * MultiROI
  * Events
  * Commmands

### Fixed
- Disable the `DeviceLinkSelector` on all devices
  * fixes an issue with specific dart1 models
- Concurrent start of pylonsrc from multiple processes
  * opening a device is now retried for 30s in case of multiprocess collision
  * fixes #25
- ace2/dart2/boost feature dependency introspection fixed
  * fixes #32


## [0.5.1] - 2022-12-28

### Fixed
- Properly handle cameras, where OffsetX/Y is readonly after applying startup settings ( Fixes #26)


## [0.5.0] - 2022-12-09

### Added
- Access to all pylon image metadata
  * GrabResult
    * BlockID
    * ImageNumber
    * SkippedImages
    * OffsetX/Y
    * Camera Timestamp
  * Chunkdata
    * all enabled chunks are added as key/value GstStructure elements
  * for sample user code see the [show_meta](tests/examples/pylon/show_meta.c) example
- Generation of includes, library and pkg-config files to access the GstMeta data of the plugin
- Camera properties accessible by integer based selectors are now accessible as gstreamer properties
  * some properties e.g. ActionGroupMask are selected by an integer index. Support for these properties is now integrated.

### Changed
- Breaking change for width/height fixation:
  * old: prefer min(1080P, camera.max)
  * new: prefer current camera value after user-set and pfs-file
- Startup time on some camera models extended up to ~5s
  * This is a side effect of the fixes to properly capture the absolute min/max values of a property.
  * The first gst-inspect-1.0 after compilation/installation will block twice as long.
  * A caching infrastructure to skip this time on subsequent usages of pylonsrc is scheduled for the next release.

### Fixed
- Properties have now proper flags to allow changing in PLAYING state if valid for pylon.
- Plugin uses only a single 'pylonsrc' debug channel (Fixes #22)
  * usage of 'default' and 'pylonsrc' channel was root cause of stability issues with extensive logging
- Detect the absolute min/max values of properties
  * the feature limits of GenICam based cameras can change depending on the operating point. The plugin now explores the min/max values possible with the current device.
- Allow generic introspection of plugin properties ( Fixes #18)
  * internal restructuring of property type system
  * for sample user code see the [list_properties](tests/examples/pylon/list_properties.c) example
- Update readme to cover exact steps to build the plugin (Fixes #23, #19, #20)
- Upstream gstreamer fix to properly detect typos in gst-launch-1.0 pipeline definitions [!2908](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2908)
  * Fix is available in gstreamer >= 1.21




## [0.4.0] - 2022-10-06

### Added
- Add property to control behaviour in case of capture errors. Allow to abort capture ( current behaviour ) / skip / keep bad buffer
- Add support for proper handling of a device disconnect

### Changed
- Dynamically adjust a feature's access mode. This will make a feature writable, if it's access mode depends on another features state. ( Fixes issue #12 )

### Fixed
- Fix building gst-plugin-pylon with pylon 6 in monorepo configuration ( Fixes issue #15 )


## [0.3.0] - 2022-09-13

### Added
- Support to access stream grabber parameters via `stream::` prefix
- Bayer 8bit video data support
- Add property to configure camera from a pylon feature stream file ( `pfs-location=<filename>` )
- Support to build gst-plugin-pylon as monorepo subproject to test against upcoming changes in gstreamer monorepo
- Add pygobject example to access camera and stream grabber features in the folder [examples](https://github.com/basler/gst-plugin-pylon/tree/main/tests/examples/pylon)

### Changed
- Remove fixed grab timeout of 5s. pylonsrc will now wait forever. If required timeout/underrun detection can be handled in the gstreamer pipeline. ( e.g. via queue underrun )
- Remove limititation to only map features that are streamable
  Effect is that now all accessible features of a camera are mapped to gstreamer properties
- Output more information in case of missing selection of a camera. Now: serial number, model name and user defined name.
- Run all tests in github actions

### Fixed
- Allow loading of default user set on some early Basler USB3Vision cameras


## [0.2.0] - 2022-08-09
### Added
- Initial public release

