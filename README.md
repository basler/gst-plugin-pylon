# gst-plugin-pylon

> The official GStreamer source plug-in for Basler cameras powered by Basler pylon Camera Software Suite


This plugin allows to use any Basler 2D camera (supported by Basler pylon Camera Software Suite) as source element in a GStreamer pipeline.

All camera features are available in the plugin by dynamic runtime mapping to gstreamer properties.

**Please Note:**
This project is offered with no technical support by Basler AG.
You are welcome to post any questions or issues on [GitHub](https://github.com/basler/gst-plugin-pylon).

[![CI](https://github.com/basler/gst-plugin-pylon/actions/workflows/ci.yml/badge.svg)](https://github.com/basler/gst-plugin-pylon/actions/workflows/ci.yml)

The next chapters describe how to [install](#downloads-for-debian-and-ubuntu), [use](#getting-started) and [build](#building) the *pylonsrc* plugin.

# Getting started

To display the video stream of a single Basler camera is as simple as:

```
gst-launch-1.0 pylonsrc ! videoconvert ! autovideosink
```

> The camera features are registered dynamically to gstreamer. This registration is executed once the first time a camera model is used in gstreamer and can take up to ~10s. The registration information is cached in the filesystem to speed up subsequent uses of the camera.

The following sections describe how to select and configure the camera.

## Camera selection
If only a single camera is connected to the system, `pylonsrc` will use this camera without any further actions required.

If more than one camera is connected, you have to select the camera.
Cameras can be selected via index, serial-number or device-user-name

If no selection is given `pylonsrc` will show an error message which will list the available cameras. The list contains the serial number, the device model name and if set the user defined name of each camera.

```bash
gst-launch-1.0 pylonsrc ! fakesink
Setting pipeline to PAUSED ...
ERROR: Pipeline doesnt want to pause.
ERROR: from element /GstPipeline:pipeline0/GstPylonSrc:pylonsrc0: Failed to start camera.
Additional debug info:
../ext/pylon/gstpylonsrc.c(524): gst_pylon_src_start (): /GstPipeline:pipeline0/GstPylonSrc:pylonsrc0:
At least 4 devices match the specified criteria, use "device-index", "device-serial-number" or "device-user-name" to select one from the following list:
[0]: 21656705   acA1920-155uc   top-left
[1]: 0815-0000  Emulation
[2]: 0815-0001  Emulation
[3]: 0815-0002  Emulation
```

### Examples
Select second camera from list:

```
gst-launch-1.0 pylonsrc device-index=1 ! videoconvert ! autovideosink
```

Select camera with serial number `21656705`:

```
gst-launch-1.0 pylonsrc device-serial-number="21656705" ! videoconvert ! autovideosink
```

Select camera with user name `top-left`:

```
gst-launch-1.0 pylonsrc device-user-name="top-left" ! videoconvert ! autovideosink
```

## Configuring the camera

The configuration of the camera is defined by
* gstreamer pipeline capabilities
* the active UserSet or supplied PFS file
* feature properties of the plugin

### Capabilities

The pixel format, image width, image height and acquisition framerate ( FPS ) are set during capability negotiation.


#### Examples

Configuring to 640x480 @ 10fps in Mono8 format:

```
gst-launch-1.0 pylonsrc ! "video/x-raw,width=640,height=480,framerate=10/1,format=GRAY8"  ! videoconvert ! autovideosink
```

Configuring to 640x480 @ 10fps in BayerRG8 format:

```
gst-launch-1.0 pylonsrc ! "video/x-bayer,width=640,height=480,framerate=10/1,format=rggb" ! bayer2rgb ! videoconvert ! autovideosink
```

**Important:** The **bayer2rgb** element does not process non 4 byte aligned bayer formats correctly. If no size is specified (or a range is provided) a word aligned width will be automatically selected. If the width is hardcoded and it is not word aligned, the pipeline will fail displaying an error.

#### Pixel format definitions

For the pixel format check the format supported for your camera on https://docs.baslerweb.com/pixel-format

The mapping of the camera pixel format names to the gstreamer format names is:

|Pylon              | GStreamer  |
|-------------------|:----------:|
| Mono8             |  GRAY8     |
| RGB8Packed        |  RGB       |
| RGB8              |  RGB       |
| BGR8Packed        |  BGR       |
| BGR8              |  BGR       |
| YCbCr422_8        |  YUY2      |
| YUV422_8          |  YUY2      |
| YUV422_YUYV_Packed|  YUY2      |
| YUV422_8_UYVY     |  UYVY      |
| YUV422Packed      |  UYVY      |
| BayerBG8          |  bggr      |
| BayerGR8          |  grbg      |
| BayerRG8          |  rggb      |
| BayerGB8          |  gbrg      |
| Mono16            | GRAY16_LE  |
| Mono10 / Mono12   | GRAY16_LE *(zero-copy, values are LSB-aligned: 0..1023 / 0..4095, not rescaled)* |
| Mono10p / Mono12p / Mono12Packed | GRAY16_LE *(unpacked to 16-bit on capture)* |
| BayerBG10 / 12 / 16 | bggr10le / bggr12le / bggr16le *(GStreamer ≥ 1.24)* |
| BayerGR10 / 12 / 16 | grbg10le / grbg12le / grbg16le *(GStreamer ≥ 1.24)* |
| BayerRG10 / 12 / 16 | rggb10le / rggb12le / rggb16le *(GStreamer ≥ 1.24)* |
| BayerGB10 / 12 / 16 | gbrg10le / gbrg12le / gbrg16le *(GStreamer ≥ 1.24)* |

Cameras using **Basler Compression Beyond** or **GenDC** (`PayloadType_GenDC`,
e.g. GeV 3.0 GenDC-only or USB3 Vision cameras with `GenDCStreamingMode`) are
decompressed/extracted transparently into the negotiated GStreamer caps (including
`memory:NVMM`).

### Fixation

If two pipeline elements don't specify which capabilities to choose, a fixation step gets applied.

If _no_ fixed width and height is decided during caps negotiation,
`pylonsrc` will apply the current camera configuration (or the nearest
possible values) after applying the userset and pfs file. It is
recommended to set a caps-filter to explicitly set the wanted
capabilities.

# NVMM Support


NVMM caps are now supported in the current version of the element. This feature
is automatically enabled when both the CUDA library and the DeepStream library
are installed on the system.

By using this support, a memory speedup can be achieved as it eliminates the
need for an additional element to connect the system memory and NVIDIA's GPU
memory.

Here's an example of how to use this feature:

```bash
gst-launch-1.0 pylonsrc ! "video/x-raw(memory:NVMM), width=1920, height=1080"  !  nvvidconv  ! "video/x-raw(memory:NVMM), width=1280, height=720" !  fakesink 
```

### Full-range YUV in NVMM

Basler cameras send YUV 4:2:2 in full range (BT.601, 0..255).
Say so in the caps, and `pylonsrc` allocates extended-range NVMM surfaces for YUY2 and UYVY.
Without it, the surfaces are limited range, and NVMM conversions downstream clamp the samples to 16..235.

```bash
gst-launch-1.0 pylonsrc ! "video/x-raw(memory:NVMM), format=YUY2, colorimetry=1:4:5:1" ! nvvidconv ! "video/x-raw(memory:NVMM), format=NV12" ! fakesink
```

The first field of `colorimetry`, 1, is the full range; the second, 4, is the BT.601 matrix.



### Handle capture errors

`pylonsrc` lets you decide what to do when a capture error happens.

This feature is controlled by the enumeration property `capture-error`. You can choose one of the following options:

* **keep:** Use the partial or corrupted buffers (delivered buffers are marked with `GST_BUFFER_FLAG_CORRUPTED`).
* **skip:** Skip the partial or corrupted buffers. A maximum of 100 buffers can be skipped before the pipeline aborts
* **abort:** Stop pipeline in case of any capture error.

If this property is not set, the default behavior is the `abort` option, meaning that the element will fail to process the buffer and it will post a fatal error to the bus.

As an example, the following pipeline will skip corrupted or partial buffers:

```
gst-launch-1.0 pylonsrc capture-error=skip ! videoconvert ! autovideosink
```

Capture errors and reconnection events are also emitted as structured element bus messages:

* `pylonsrc-capture-error`: fields `action` (`kept`, `skipped`, `aborted`), `skipped-frames`, `error-code` and `description`.
* `pylonsrc-reconnect`: field `status` (`reconnecting`, `reconnected`) and, for `reconnected`, the `attempt` number.

### Grab timeout and auto-reconnect

* **`grab-timeout`:** Per-frame grab timeout in milliseconds (`0` = wait indefinitely, default).
* **`auto-reconnect`:** Opt-in automatic recovery when a camera disconnects (`false` by default). With `auto-reconnect=true` a disconnect is reported as a bus *warning* followed by `pylonsrc-reconnect` messages; the element retries once per second for up to 15 attempts and only then posts an error. Without it, a disconnect is an error.

```bash
gst-launch-1.0 pylonsrc grab-timeout=2000 auto-reconnect=true ! videoconvert ! autovideosink
```

### Executing GenICam commands

GenICam `ICommand` nodes (including selector-qualified commands such as `TriggerSoftware-FrameStart`) can be triggered programmatically via the `execute-command` action signal on `pylonsrc`:

```c
gboolean ok = FALSE;
g_signal_emit_by_name(pylonsrc, "execute-command", "TriggerSoftware-FrameStart", &ok);
```

### Startup optimization and environment variables

* `PYLONSRC_SKIP_TRANSPORT_LAYERS` / `GST_PYLON_SKIP_TL`: Comma-separated list of transport layers to skip during discovery (e.g. `gige,cxp`), reducing startup latency when only specific interfaces are used.
* `PYLONSRC_OPEN_RETRIES`: Number of times opening a camera is retried when it is temporarily busy (default `30`, range `0`..`600`, with a delay growing from 100 ms to 350 ms). Set to `0` to fail immediately.
* `GST_PYLON_PROBE_LIMITS=1`: Opt-in to full dynamic GenICam limit probing during `gst-inspect-1.0` (by default `gst-inspect-1.0` is read-only and non-destructive).
* `GST_PYLON_INSPECT_DEVICES=1`: Force eager device enumeration during `class_init` outside of `gst-inspect-1.0`.
* The plugin also registers a metadata-only `pylondeviceprovider` at `GST_RANK_NONE` for non-blocking camera discovery without hijacking `autovideosrc` or OpenCV.

### Pending frames

`pylonsrc` keeps grabbed frames until its streaming thread takes them.
The property `max-pending-frames` sets how many.

* **1 (default):** Only the latest frame is kept. A new frame replaces one that was not taken yet. This is like a v4l2 source.
* **2 or more:** Frames are queued in grab order. When the queue is full, the oldest frame is dropped. A stall in the streaming thread shorter than the queue loses no frames. pylon then grabs with `GrabStrategy_OneByOne` instead of `GrabStrategy_LatestImageOnly`.

We recommend 2. It bridges short stalls and ties up only one more grab buffer than the default.

Queued frames stay in pylon grab buffers, so no memory is copied or allocated.
Keep `max-pending-frames` well below `stream::MaxNumBuffer`.
The pipeline downstream also holds buffers. If downstream holds nearly all of them (more than `MaxNumBuffer - 2`), `pylonsrc` copies further frames instead of passing the pylon buffer on, so the camera keeps streaming.

As an example, the following pipeline queues up to 2 frames:

```
gst-launch-1.0 pylonsrc max-pending-frames=2 ! videoconvert ! autovideosink
```

### Automatic rounding/correction of property values

The gstreamer model for properties only represents a static range of a property. The pylon feature model has dynamic ranges and increments. These values can change depending on the current values of other properties.

In the registration phase the absolute minimum and maximum values of a feature are registered as gstreamer property.

Per default pylon and the camera will round/correct to the nearest valid value.

This behaviour can be changed by using the gstreamer boolean property `enable-correction`.

The options for this property are:

* `enable-correction=true` the plugin will round or adjust to the nearest valid value. This is the default behaviour.
* `enable-correction=false` the exact value has to be set. If the value is not valid ( out of range or wrong increment) the property setting is ignored and an error log message generated.



### UserSet handling

`pylonsrc` always loads a UserSet of the camera before applying any further properties.

This feature is controlled by the enumeration property `user-set`.

If this property is not set, or set to the value `Auto`, the power-on UserSet gets loaded. How to select another power-on default is documented for [UserSetDefault](https://docs.baslerweb.com/user-sets#choosing-a-startup-set) feature.

To select dynamically another UserSet the `user-set` property accepts any other implemented UserSet of the camera.

**Example**

Activate the `UserSet1` UserSet on a Basler ace camera:

```
gst-launch-1.0 pylonsrc user-set=UserSet1 ! videoconvert ! autovideosink
```

Overall UserSets topics for the camera are documented in [Chapter UserSets](https://docs.baslerweb.com/user-sets) in the Basler product documentation.

### PFS file

`pylonsrc` can load a custom configuration from a PFS file. A PFS file is a file with previously saved settings of camera features.

This feature is controlled by the property `pfs-location`.

To use a PFS file, specify the filepath using the `pfs-location` property.

**Example**

Use a PFS file with name `example-file-name.pfs` on a Basler ace camera:

```
gst-launch-1.0 pylonsrc pfs-location=example-file-name.pfs ! videoconvert ! autovideosink
```

**Important:** Using this property will result in overriding the camera features set by the `user-set` property if also specified.

An example on how to generate PFS files using pylon Viewer is documented in [Chapter Overview of the pylon Viewer](https://docs.baslerweb.com/overview-of-the-pylon-viewer#camera-menu) in the Basler product documentation.

### Features

After applying the UserSet, the optional PFS file and the gstreamer properties, any other camera feature gets applied.

The `pylonsrc` plugin dynamically exposes all features of the camera as gstreamer [child properties](https://gstreamer.freedesktop.org/documentation/gstreamer/gstchildproxy.html?gi-language=c) with the prefix `cam::` and all stream grabber parameters with the prefix `stream::`

Pylon features like `ExposureTime` or `Gain` are mapped to the gstreamer properties `cam::ExposureTime` and `cam::Gain`.

**Example**

Set Exposuretime to 2000µs and Gain to 10.3dB:

```
gst-launch-1.0 pylonsrc cam::ExposureAuto=0 cam::GainAuto=0 cam::ExposureTime=2000 cam::Gain=10.3 ! videoconvert ! autovideosink
```

Pylon stream grabber parameters like MaxTransferSize or MaxNumBuffer are mapped to the gstreamer properties `stream::MaxTransferSize` and `stream::MaxNumBuffer`.

**Example**

Set MaxTransferSize to 4MB and MaxNumBuffer to 10 on an USB3Vision camera:

```
gst-launch-1.0 pylonsrc stream::MaxTransferSize=4194304 stream::MaxNumBuffer=10 ! videoconvert ! autovideosink
```

All available features can be listed by by calling `gst-inspect-1.0`:

```
gst-inspect-1.0 pylonsrc
```

### Selected Features

Some of the camera features are not directly available but have to be selected first.

As an example the feature `TriggerSource` has to be selected by `TriggerSelector` first. ( see [TriggerSource](https://docs.baslerweb.com/trigger-source) in the Basler product documentation)

These two steps ( select and access ) are mapped to `pylonsrc` properties as a single step with this pattern

`cam::<featurename>-<selectorvalue>`

For the above  `TriggerSource` example if  the `TriggerSource` of the trigger `FrameStart` has to be configured the property is:

`cam::TriggerSource-FrameStart`

**Example**

Configure a hardware trigger on Line1 for the trigger FrameStart:

```
gst-launch-1.0 pylonsrc cam::TriggerSource-FrameStart=Line1 cam::TriggerMode-FrameStart=On ! videoconvert ! autovideosink
```

### Chunks and Capture metadata

Chunk support is available. The selected chunks will be appended to each gstreamer buffer as meta data.

The `cam::ChunkModeActive` feature needs to be set to `true` for chunks to be enabled.

The pylon image meta data ( pylon GrabResult ) is appended per default. The values available are:

* GrabResult
* BlockID
* ImageNumber
* SkippedImages
* OffsetX/Y
* Camera Timestamp

**Example**

Enable Timestamp, ExposureTime and CounterValue chunks:
```
gst-launch-1.0 pylonsrc cam::ChunkModeActive=True cam::ChunkEnable-Timestamp=True cam::ChunkEnable-ExposureTime=true cam::ChunkEnable-CounterValue=true ! videoconvert ! autovideosink
```

**GstMetaPylon**

The plugin meta data is defined in [gstpylonmeta.h](gst-libs/gst/pylon/gstpylonmeta.h).

A programming sample using these defintions to decode the data is in [show_meta](tests/examples/pylon/show_meta.c)


**Access to GstMetaPylon from python**

To access the metadata a Python support library is available. The `pygstpylon` provides the required access helper to decode the metadata from plugins and probes.

> Note that Python bindings are disabled by default, refer to the build section for instructions on how to enable them.

One usage example to access camera chunk and metadata from a python plugin is in [snapshot_gpio.py](tests/examples/python/snapshot_gpio.py)

This sample plugin will check the LineStatusAll chunk to detect an edge on one of the inputs to output a single image while per default all images get dropped.

The below usage example will show live video and store a snapshot if the gpio edge is detected on Line4 of the camera.

```bash
# the plugin path for python code has to point to a directory with a 'python' subdirectory
export GST_PLUGIN_PATH=<gst-plugin-pylon>/tests/examples
gst-launch-1.0 pylonsrc cam::ChunkModeActive=True cam::ChunkEnable-LineStatusAll=True \
        ! tee name=t \
        t. ! queue ! pylongpiosnapshot_py trigger-source=4 rising-edge=true ! videoconvert ! pngenc ! multifilesink location=image%05d.png async=false\
        t. ! queue ! videoconvert  ! autovideosink
```

# Downloads for Debian and Ubuntu

The pylonsrc plugin can be downloaded for the following revisions of Ubuntu and Debian on the releases page.

[Download the latest release](https://github.com/basler/gst-plugin-pylon/releases/latest)

## Operating Systems with available downloads

| Distribution | Versions |
|--------------|---------------------|
| Ubuntu       | 24.04, 22.04 |
| Debian       | 13 (Trixie), 12 (Bookworm) |

For any other OS you have to currently [build](#building) the plugin yourself.

# Building

This plugin is built using the [meson](https://mesonbuild.com/) build system. The meson version has to be >= 0.61. GStreamer 1.20 or newer is required.

As a first step install Basler pylon according to your platform. Downloads are available at: [Basler software downloads](https://www.baslerweb.com/en/downloads/software-downloads/#type=pylonsoftware;language=all;version=all)

The supported pylon versions on the different platforms are:

|                 | > 26.06 | 26.06 | 7.5 | 7.4 | 6.2 |
|-----------------|:-------:|:-----:|:---:|:---:|:---:|
| Windows x86_64  |    x    |   x   |  x  |  x  |     |
| Linux x86_64    |    x    |   x   |  x  |  x  |     |
| Linux aarch64   |    x    |   x   |  x  |  x  |  x  |
| macOS           |    x    |   x   |  -  |  -  |  -  |

A from-source meson build does not require C++ SDK 12.2. It still detects
pylon 7.1+ via CMake and falls back to the 6.x finder on aarch64.

CI builds and tests all four platforms against the latest pylon release.
Official Linux Debian packages use a `YY.MM[.P]` release version (e.g. `Version: 26.10.1`).
Each generated gst-plugin-pylon Debian package records that build-time pylon
version in three places that stay in sync:

- runtime `Depends: pylon (= <build-time>)`
- control field `Pylon-Built-Against`
- Debian package `Version` suffix `+pylon<build-time>` (visible in the `.deb`
  filename, e.g. `gst-plugin-pylon_1.1.0+pylon26.10.1_arm64.deb`)

Inspect them with:

```
dpkg-deb -f gst-plugin-pylon_*.deb Version Depends Pylon-Built-Against
```

Installing Basler pylon SDK will also install the Basler pylon viewer. You should use this tool to verify, that the cameras work properly in your system and to learn about the their features.

The differences in the build steps for [Linux](#linux), [Windows](#windows) and [macOS](#macos) are described in the next sections.

Building Debian packages locally is described in the sections [Debian/Ubuntu](#debian-packaging) and [NVIDIA Jetson](#debian-nvidia-packaging).

## Linux
Make sure the dependencies are properly installed. In Debian-based
systems you can run the following commands:

```bash
# Meson and ninja build system (venv avoids Ubuntu 24.04 PEP 668 pip restrictions)
sudo apt install python3 python3-venv python3-pip
python3 -m venv build_python
. build_python/bin/activate
pip install meson ninja --upgrade

# GStreamer
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev cmake
# if you want to use the sample python plugin
sudo apt install gstreamer1.0-python3-plugin-loader
# required only when enabling -Dpython-bindings=enabled
sudo apt install python3-dev python3-gi gir1.2-gstreamer-1.0

```

The build process relies on `-Dpylon-root` (or the `PYLON_ROOT` environment
variable) pointing to a pylon tree with `include/pylon` (official installer
under `/opt/pylon`, or any other extracted SDK). Meson finds pylon via CMake
using that path and sets an install RPATH to `$PYLON_ROOT/lib` on native Linux
builds (overridable via `-Dpylon-rpath`), so a from-source install does **not**
require a `pylon` Debian package.

```bash
# official Linux installer
export PYLON_ROOT=/opt/pylon
# or pass directly to Meson without exporting PYLON_ROOT:
# meson setup builddir --prefix /usr/ -Dpylon-root=/path/to/pylon
```

Then proceed to configure the project. Check `meson_options.txt` for a
list of configuration options. On Debian-based systems, make sure you
configure the project as:

```bash
git clone https://github.com/basler/gst-plugin-pylon.git
cd gst-plugin-pylon
meson setup builddir --prefix /usr/
```

If you want to enable Python bindings, run the following command instead when setting up the meson project, since the bindings are disabled by default:

```bash
meson setup builddir --prefix /usr/ -Dpython-bindings=enabled
```

Build, test and install the project:

```bash
# Build
ninja -C builddir

# Test (camemu uses PYLON_CAMEMU from meson; pygstpylon needs bindings + gi)
ninja -C builddir test
# or a single suite:
# meson test -C builddir pygstpylon --print-errorlogs

# Inspect the uninstalled plugin from this build
meson devenv -C builddir gst-inspect-1.0 pylonsrc

# Install plugin (and pygstpylon if bindings were enabled)
sudo ninja -C builddir install
```

Finally, test for proper installation:

```bash
gst-inspect-1.0 pylonsrc
```

If you enabled Python bindings, `pygstpylon` is installed into the interpreter's
package directory (often under `/usr/local/lib/python3/dist-packages` even when
`--prefix` is `/usr`). Confirm with `python3 -c "import pygstpylon; print(pygstpylon.__version__)"`.

## Linux package building

### Debian Packaging

The release packages are built once per architecture on Ubuntu 22.04, the
oldest supported userspace (glibc and GStreamer 1.20), and install-tested
unchanged on Ubuntu 22.04, Ubuntu 24.04, Debian bookworm, and Debian trixie. Runtime
`Depends` pin the build-time pylon dpkg Version exactly
(`pylon (= <build-time>)`), because pylon builds are not binary-compatible
across versions. The same version is written into `Pylon-Built-Against`
and into the Debian package Version as `+pylon…` so the `.deb` filename
shows which pylon the binary was built for.
A from-source meson build can still use older suites from the table above.

`tools/patch_deb_changelog.sh` reads the installed `pylon` package (or
`PYLON_PKG_VERSION`) and appends that suffix before `dpkg-buildpackage`.
Example, after registering a 7.4.0 stub:

```
# → gst-plugin-pylon_1.1.0+pylon7.4.0_arm64.deb
#   Depends: pylon (= 7.4.0)
#   Pylon-Built-Against: 7.4.0
PYLON_PKG_VERSION=7.4.0 sudo tools/register_pylon_from_tree.sh /path/to/pylon_sdk.tar.gz
```

Inspect a built package with:

```
dpkg-deb -f gst-plugin-pylon_*.deb Version Depends Pylon-Built-Against
```

`dpkg-buildpackage` needs a dpkg `pylon` package (Build-Depends is unversioned;
runtime Depends use the build-time Version). A tree at `PYLON_ROOT` is not
enough by itself.

Install the official `pylon` Debian package (installed under `/opt/pylon`, with
`codemeter` optional unless required by licensed features), **or** register a
local SDK archive as a stub package:

```
# tarball must contain a top-level pylon/ directory (sudo tar -czf ... -C /opt pylon)
# Optional: PYLON_PKG_VERSION=7.4.0 when registering a non-date-versioned SDK tree
sudo tools/register_pylon_from_tree.sh /path/to/pylon_sdk.tar.gz
```

Install the platform dependencies:

```
sudo apt-get install cmake meson ninja-build debhelper dh-python fakeroot pkg-config \
                     libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
                     gstreamer1.0-tools gstreamer1.0-plugins-base \
                     gstreamer1.0-python3-plugin-loader \
                     python3 python3-dev python3-gi
```

Prepare the build setup ( from main project folder ):

```
ln -sfn packaging/debian
tools/patch_deb_changelog.sh
```

Build the debian packages

```
PYLON_ROOT=/opt/pylon dpkg-buildpackage -us -uc -rfakeroot
```

The package build uses the system Meson/Ninja packages (no pip bootstrap) and runs
the Meson test suite, including camemu and pygstpylon (`PYLON_CAMEMU=4`). Skip
tests with `DEB_BUILD_OPTIONS=nocheck` if you have no emulators.
`python3-pygstpylon` uses CPython's stable ABI with a Python 3.10 floor, so the
same package imports with the distro Python on every supported target.

To reproduce the build-once/install-many check locally, provide an SDK archive
containing a top-level `pylon/` directory:

```bash
PYLON_SDK_TGZ=/path/to/pylon_sdk.tar.gz tools/test_deb_in_docker.sh
```

### Debian NVIDIA Packaging

NVMM (`memory:NVMM`) needs DeepStream and CUDA at build time. GitHub CI
compile-checks the NVIDIA Debian profile on a native ARM runner against the
real NVIDIA headers and libraries for these combinations:

| JetPack / L4T | CUDA | DeepStream |
|---------------|------|------------|
| 6.0 DP / 36.2 | 12.2 | 6.4 |
| 6.0 GA / 36.3 | 12.2 | 7.0 |
| 6.2 / 36.4.3 | 12.6 | 7.1 |

The CI jobs have no Jetson GPU: they prove that the packages compile, link to
`libnvbufsurface` and `libcudart`, and contain the expected Debian dependency.
`libnvbufsurface` comes from L4T (`nvidia-l4t-multimedia-utils`), not from the
DeepStream tarball, so CI downloads that Jetson package as a build input only.
They do not execute NVMM and their `.deb` files are deliberately not published.
Runtime qualification requires a real Jetson worker.

DeepStream 6.3 is not in this matrix because its JetPack 5.1.2 image provides
GStreamer 1.16, while this plug-in requires GStreamer 1.20 or newer.

#### Build the package locally on a Jetson

Install the matching JetPack and DeepStream release first, then install the
official `pylon` Debian package (placed under `/opt/pylon`).

Install the platform dependencies (match the DeepStream package to your JetPack):

```
sudo apt-get install cmake meson ninja-build debhelper dh-python fakeroot pkg-config \
                     libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
                     gstreamer1.0-tools gstreamer1.0-plugins-base \
                     gstreamer1.0-python3-plugin-loader \
                     python3 python3-dev python3-gi \
                     gir1.2-gstreamer-1.0 \
                     deepstream-7.1   # use deepstream-6.4 or -7.0 on matching JetPack
```

Download the source directly from GitHub (for example the `v1.1.0` release tag,
or `main` for current development):

```
git clone --depth 1 --branch v1.1.0 \
  https://github.com/basler/gst-plugin-pylon.git
cd gst-plugin-pylon

ln -sfn packaging/debian
tools/patch_deb_changelog.sh
```

Build with the `nvidia` profile. This requires NVMM support to be found instead
of silently producing a system-memory-only package. The build also runs the
Meson and pylon camera-emulator tests on the Jetson. Run
`tools/patch_deb_changelog.sh` first so the `.deb` Version carries
`+pylon<build-time>` and runtime Depends match that pylon:

```
tools/patch_deb_changelog.sh
DEB_BUILD_PROFILES=nvidia PYLON_ROOT=/opt/pylon dpkg-buildpackage -us -uc -rfakeroot
```

On pylon 7.4.0 that yields a name like
`gst-plugin-pylon_1.1.0+pylon7.4.0-1~<L4T>_arm64.deb` with
`Depends: pylon (= 7.4.0)`.

The `.deb` files are written to the parent directory. Install them with:

```
sudo apt-get install ../gst-plugin-pylon_*.deb \
                     ../gst-plugin-pylon-dev_*.deb \
                     ../python3-pygstpylon_*.deb
```

Only if the pylon camera emulator is unavailable, skip tests explicitly with
`DEB_BUILD_OPTIONS=nocheck`; do not use that for a qualified package build.

### Integrating with GStreamer monorepo

The monorepo is a top-level repository that integrates and builds all
GStreamer subprojects (core, plugins, docs, etc...). It is the
recommended way of building GStreamer from source, test different
versions without installing them and even contribute code in the form
of Merge Requests. The monorepo was released starting with GStreamer
1.20 stable release.

GstPluginPylon can integrate seamlessly with GStreamer's monorepo. To
do so run the following commands:

1. Clone the monorepo branch you wish to test. For example 1.20:
```
git clone https://gitlab.freedesktop.org/gstreamer/gstreamer -b 1.20
cd gstreamer
```

2. Clone gst-plugin-pylon into the subprojects directory:
```
cd subprojects/
git clone https://github.com/basler/gst-plugin-pylon
cd -
```

3. Configure and build as usual, specifying the custom subproject:
```
PYLON_ROOT=/opt/pylon meson setup builddir --prefix /usr -Dcustom_subprojects=gst-plugin-pylon
ninja -C builddir
```

4. Test the uninstalled environment:
```
ninja -C builddir devenv
```
This will open a new shell with the evironment configured to load GStreamer from this build.
```
gst-inspect-1.0 pylonsrc
```
Type `exit` to return to the normal shell.

Refer to the [official documentation](https://github.com/GStreamer/gstreamer/blob/main/README.md)
for instructions on how to customize the monorepo build.

### Maintainer configuration

If you are a maintainer or plan to extend the plug-in, we recommend
the following configuration:

```
meson setup builddir --prefix /usr/ --werror --buildtype=debug -Dgobject-cast-checks=enabled -Dglib-asserts=enabled -Dglib-checks=enabled
```

### Cross compilation for linux targets

Pass a Meson cross file (`--cross-file <file>`) and point `-Dpylon-root` (or
`PYLON_ROOT`) at the target pylon SDK:

```bash
meson setup builddir \
  --cross-file cross-aarch64.ini \
  --prefix /usr \
  -Dpylon-root=/path/to/sysroot/opt/pylon \
  -Dtests=disabled
ninja -C builddir
DESTDIR=/path/to/staging meson install -C builddir
```

How cross-compilation paths and RPATHs are handled:

- **Sysroot RPATH stripping**: When `pylon-root` is inside the cross sysroot
  (`sys_root` in the cross file or `PKG_CONFIG_SYSROOT_DIR` in the environment,
  as set by Yocto's `meson.bbclass`), Meson strips the sysroot prefix for the
  installed `DT_RUNPATH` (for example, `${RECIPE_SYSROOT}/opt/pylon` becomes
  `/opt/pylon/lib`, and `/usr/lib` is omitted to avoid standard-RPATH QA
  errors). `libgstpylon.so` and `pygstpylon` also embed relative `$ORIGIN`
  entries to locate `libgstpylon-1.0.so`.
- **SDK staged outside `sys_root`**: If `pylon-root` is outside the sysroot,
  Meson omits the pylon install RPATH so build-host paths never leak into
  installed binaries. Pass `-Dpylon-rpath=/opt/pylon/lib` if the target stores
  pylon outside standard library paths, or `-Dpylon-rpath=none` to omit it
  explicitly.
- **Python**: Build-time helper scripts always use the build-machine Python
  (`native: true`), so cross files may set `[binaries] python` to the target
  interpreter without breaking configure. Runtime `pygstpylon` tests and Hotdoc
  documentation are skipped automatically when host binaries cannot be run.
- **NVMM**: Default DeepStream/CUDA probes are restricted to the cross sysroot
  (or skipped when no `sys_root` is set). Pass `-Dnvds-incdir` and
  `-Dnvds-libdir` if NVMM headers/libraries live in a custom location.

#### Yocto recipe
A reference Yocto recipe for honister is available in the [meta-basler-tools](https://github.com/basler/meta-basler-tools/tree/honister/recipes-multimedia/gstreamer)
repository on GitHub.

Builds on Yocto kirkstone and later work without the patch. Set
`-Dpylon-root=${RECIPE_SYSROOT}/opt/pylon` (or export
`PYLON_ROOT=${RECIPE_SYSROOT}/opt/pylon`); the sysroot prefix is stripped
automatically from installed RPATHs so Yocto `buildpaths`/`rpaths` QA checks
pass without `patchelf` workarounds.

(The honister recipe still needs a backport patch due to the older Meson version in honister.)

## Windows
Install the dependencies:

GStreamer:
* Install the gstreamer runtime **and** development packages ( using the files for MSVC 64-bit (VS 2019, Release CRT ) from [https://gstreamer.freedesktop.org/download/](https://gstreamer.freedesktop.org/download/)
* use latest version.

Meson:
* Install a current Meson (1.x) from [Meson GitHub Releases](https://github.com/mesonbuild/meson/releases)

Visual Studio:
* Install Visual Studio (e.g. Community Edition) from Microsoft
* Select desktop development (C++) package within installer

The following commands should be run from a *Visual Studio command prompt*. The description below doesn't work for PowerShell.

Specify the path to pkgconfig configuration files for GStreamer and the pkg-config binary (shipped as part of GStreamer development):

```cmd
set PKG_CONFIG_PATH=%GSTREAMER_1_0_ROOT_MSVC_X86_64%\lib\pkgconfig
set PATH=%PATH%;%GSTREAMER_1_0_ROOT_MSVC_X86_64%\bin
```

The build process relies on `PYLON_ROOT` (and optionally `CMAKE_PREFIX_PATH`) pointing to Basler pylon cmake support files. This is normally set by the Basler pylon installer.
```bash
set PYLON_ROOT=C:\Program Files\Basler\pylon
set CMAKE_PREFIX_PATH=%PYLON_ROOT%\Development\CMake\pylon\
```


Then the plugin can be compiled and installed using Ninja:

```
git clone https://github.com/basler/gst-plugin-pylon.git
cd gst-plugin-pylon
meson setup build --prefix=%GSTREAMER_1_0_ROOT_MSVC_X86_64%
ninja -C build
```

Or made available as a Visual Studio project:

```
meson setup build --prefix=%GSTREAMER_1_0_ROOT_MSVC_X86_64% --backend=vs2022
```

This will generate a Visual Studio solution in the build directory

To test without install:

```
meson devenv -C build gst-launch-1.0.exe pylonsrc ! videoconvert ! autovideosink
```

To install into main gstreamer directory

```
ninja -C build install
```

>Workaround for running from normal cmdshell in a non US locale:
>As visual studio currently ships a broken vswhere.exe tool, that meson relies on.
Install a current vswhere.exe from [Releases · microsoft/vswhere · GitHub](https://github.com/microsoft/vswhere/releases) that fixes ( [Initialize console after parsing arguments by heaths · Pull Request #263 · microsoft/vswhere · GitHub](https://github.com/microsoft/vswhere/pull/263) )


Finally, test for proper installation:

```bash
gst-inspect-1.0 pylonsrc
```

## macOS
pylon Software Suite 26.06 and newer support macOS (`arm64`). Install GStreamer and the build tools with Homebrew, set `PYLON_ROOT` to the pylon prefix, then use meson/ninja:

```bash
brew install meson ninja cmake pkgconf gstreamer
export PYLON_ROOT=/Library/Frameworks   # or the extracted SDK tree
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix gstreamer)/lib/pkgconfig"
meson setup builddir --prefix "$PWD/install"
ninja -C builddir
ninja -C builddir test
```

Camemu functional tests run on Linux and macOS (and on Windows when `bash` is available). The pipe-FD / RSS leak check needs `/proc` and is skipped on other platforms.

# Testing and Backwards-Compatibility Verification

Run the full Meson test suite (unit tests for metadata, NVMM colorimetry, Gen2 feature priors, GenDC payload extraction, image handler state machine, CamEmu pipelines, `pygstpylon`, and golden `gst-inspect` regression checks):

```bash
PYLON_ROOT=/opt/pylon meson test -C build --print-errorlogs
```

## A/B Backwards-Compatibility Proof (`v1.1.0` vs `v2.0.0`)

To verify that existing `v1.1.0` C, Python, and `gst-launch-1.0` users can upgrade without changing or recompiling their code, run the automated A/B differential suite:

```bash
PYLON_ROOT=/opt/pylon python3 tests/compat/run_ab_compat.py --baseline-ref v1.1.0 --v2-build build
```

This builds `v1.1.0` in an isolated temporary worktree and verifies:
* Zero-recompile drop-in execution of `v1.1.0` C binaries, `run_camemu_tests.sh`, and `test_pygstpylon.py` against `v2.0.0` shared libraries (`libgstpylon-1.0.so`, `libgstpylon.so`, `pygstpylon.abi3.so`).
* `libgstpylon-1.0` exported C symbols, SONAME, and `GstPylonMeta` struct layout compatibility.
* Side-by-side behavioral equivalence across properties, signals, pad templates, lazy `class_init` + `GstChildProxy`, caps fixation, metadata, error paths, and `GstDeviceMonitor` non-interference, checked against `tests/compat/intentional_deltas_v2.json`.
* In-place `~/.cache/gstpylon` migration from `v1.1.0` to `v2.0.0` under `G_DEBUG=fatal-criticals`.

## A/B Performance Benchmark (CamEmu & Physical Cameras)

Compare cold/warm time-to-first-frame, cache completeness, `gst-inspect-1.0` latency, and sustained capture between `v1.1.0` and `v2.0.0`:

```bash
# Camera emulator (CI / headless)
PYLON_ROOT=/opt/pylon python3 tests/compat/bench_ab_v1_vs_v2.py --v2-build build

# Connected physical USB3 / GigE cameras
PYLON_ROOT=/opt/pylon python3 tests/compat/bench_ab_v1_vs_v2.py --v2-build build --serial <SERIAL_1> --serial <SERIAL_2>
```

For physical USB3/GigE, line-scan, and Jetson NVMM release verification, see [tests/compat/HARDWARE_SIGNOFF.md](tests/compat/HARDWARE_SIGNOFF.md).


# Known issues

* Due to an old issue in the pipeline parser, typos and unsupported feature names will be silently ignored on old GStreamer versions. Typos on top-level properties will be ignored on versions prior to 1.18. Typos on child::properties will be ignored on versions prior to 1.21.

* Bayer formats need to be 4 byte aligned to be properly processed by GStreamer. If no size is specified (or a range is provided) a word aligned width will be automatically selected. If the width is hardcoded and it is not word aligned, the pipeline will fail displaying an error.

* Under very specific conditions we've found that a set_state() followed immediately by a get_state() will report a failure. This has been found to be a bug in the GStreamer core, where a state conditional is not protected against spurious wakeups. An [upstream fix](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/4086) was merged to the mainline, and GStreamer 1.23 will include this fix. This issue has been reproduced in certain installations of NVIDIA Jetson boards.
  * As a workaround if you are using older GStreamer versions, is to configure `async=false` in all the sink elements in your pipeline, so that the state condition variable is not needed. Only use this is your pipeline does not require synchronization.

