# gst-plugin-pylon

> The official GStreamer source plug-in for Basler cameras powered by Basler pylon Camera Software Suite


This plugin allows to use any Basler 2D camera (supported by Basler pylon Camera Software Suite) as source element in a GStreamer pipeline.

All camera features are available in the plugin by dynamic runtime mapping to gstreamer properties.

**Please Note:**
This project is offered with no technical support by Basler AG.
You are welcome to post any questions or issues on [GitHub](https://github.com/basler/gst-plugin-pylon).

[![CI](https://github.com/basler/gst-plugin-pylon/actions/workflows/ci.yml/badge.svg)](https://github.com/basler/gst-plugin-pylon/actions/workflows/ci.yml)

The next chapters describe how to [install](#downloads-for-debian-and-ubuntu), [use](#getting-started) and [build](#Building) the *pylonsrc* plugin.

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



### Handle capture errors

`pylonsrc` lets you decide what to do when a capture error happens.

This feature is controlled by the enumeration property `capture-error`. You can choose one of the following options:

* **keep:** Use the partial or corrupted buffers.
* **skip:** Skip the partial or corrupted buffers. A maximum of 100 buffers can be skipped before the pipeline aborts
* **abort:** Stop pipeline in case of any capture error.

If this property is not set, the default behavior is the `abort` option, meaning that the element will fail to process the buffer and it will post a fatal error to the bus.

As an example, the following pipeline will skip corrupted or partial buffers:

```
gst-launch-1.0 pylonsrc capture-error=skip ! videoconvert ! autovideosink
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
| Ubuntu       | 24.04, 22.04, 20.04 |
| Debian       | 12 (Bookworm), 11 (Bullseye) |

For any other OS you have to currently [build](#building) the plugin yourself.

# Building

This plugin is build using the [meson](https://mesonbuild.com/) build system. The meson version has to be >= 0.61.

As a first step install Basler pylon according to your platform. Downloads are available at: [Basler software downloads](https://www.baslerweb.com/en/downloads/software-downloads/#type=pylonsoftware;language=all;version=all)

The supported pylon versions on the different platforms are:


|                 | 7.5  | 7.4  | 6.2  |
|-----------------|:----:|:----:|:----:|
| Windows x86_64  |  x   |   x  |      |
| Linux x86_64    |  x   |   x  |      |
| Linux aarch64   |  x   |   x  |   x  |
| macOS x86_64    |  -   |   -  |   -  |


> macOS build not available for now due to current meson/cmake interaction issues

Installing Basler pylon SDK will also install the Basler pylon viewer. You should use this tool to verify, that the cameras work properly in your system and to learn about the their features.

The differences in the build steps for [Linux](#Linux), [Windows](#Windows) and [macos](#macOS) are described in the next sections.

Building debian packages locally is described in the sections  [Debian/Ubuntu](#debian-packaging) and [Nvidia Jetson](#debian-nvidia-packaging)

## Linux
Make sure the dependencies are properly installed. In Debian-based
systems you can run the following commands:

```bash
# Meson and ninja build system
# Remove older meson and ninja from APT and install newer PIP version
sudo apt remove meson ninja-build
sudo -H python3 -m pip install meson ninja --upgrade

# GStreamer
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev cmake
# if you want to use the sample python plugin
sudo apt install gstreamer1.0-python3-plugin-loader

```

The build process relies on `PYLON_ROOT` pointing to the Basler pylon install directory.

```bash
# for pylon in default location
export PYLON_ROOT=/opt/pylon
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

# Test
ninja -C builddir test

# Install
sudo ninja -C builddir install
```

Finally, test for proper installation:

```bash
gst-inspect-1.0 pylonsrc
```

## Linux package building

### Debian Packaging

Install the pylon and codemeter debian packages. They will install into `/opt/pylon`

Install the platform dependencies:

```
sudo apt-get install cmake meson ninja-build debhelper dh-python fakeroot\
                     libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-python3-plugin-loader \
                     python3 python3-dev python3-pip python3-setuptools pybind11-dev
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

### Debian NVIDIA Packaging

Install the pylon and codemeter debian packages. They will install into `/opt/pylon`

Install the platform dependencies:

```
sudo apt-get install cmake meson ninja-build debhelper dh-python fakeroot\
                     libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-python3-plugin-loader \
                     python3 python3-dev python3-pip python3-setuptools pybind11-dev \
                     deepstream-6.3 # depending on platform deepstream-6.4 or deepstream-7.0
```

Prepare the build setup ( from main project folder ):

```
ln -sfn packaging/debian
tools/patch_deb_changelog.sh
```

Build the debian packages using the nvidia profile

```
DEB_BUILD_PROFILES=nvidia PYLON_ROOT=/opt/pylon dpkg-buildpackage -us -uc -rfakeroot
```

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

#### YOCTO recipe
A reference yocto recipe for honister is available in the [meta-basler-tools](https://github.com/basler/meta-basler-tools/tree/honister/recipes-multimedia/gstreamer)
on github. 

Builds on yocto kirkstone and later work without the patch.

( This recipe still needs a backport patch due to the version of meson tool in honister. )

## Windows
Install the dependencies:

GStreamer:
* Install the gstreamer runtime **and** development packages ( using the files for MSVC 64-bit (VS 2019, Release CRT ) from [https://gstreamer.freedesktop.org/download/](https://gstreamer.freedesktop.org/download/)
* use latest version.

Meson:
* Install the meson build system from github releases https://github.com/mesonbuild/meson/releases
* Use version meson-0.63.1-64.msi

Visual Studio:
* Install Visual Studio (e.g. Community Edition) from Microsoft
* Select desktop development (C++) package within installer

The following commands should be run from a *Visual Studio command prompt*. The description below doesn't work for powershell.

Specify the path to pkgconfig configuration files for GStreamer and the pkg-config binary ( shipped as part of gstreamer development )

```bash
set PKG_CONFIG_PATH=%GSTREAMER_1_0_ROOT_MSVC_X86_64%lib\pkgconfig
set PATH=%PATH%;%GSTREAMER_1_0_ROOT_MSVC_X86_64%\bin
```

The build process relies on CMAKE_PREFIX_PATH pointing to Basler pylon cmake support files. This is normally set by the Basler pylon installer.
```bash
set CMAKE_PREFIX_PATH=C:\Program Files\Basler\pylon 7\Development\CMake\pylon\
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
set GST_PLUGIN_PATH=build/ext/pylon

%GSTREAMER_1_0_ROOT_MSVC_X86_64%\bin\gst-launch-1.0.exe pylonsrc ! videoconvert ! autovideosink
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
Installation on macOS is currently not supported due to conflicts between meson and underlying cmake in the configuration phase.

This target will be integrated after a Basler pylon 7.x release for macOS


# Known issues

* Due to an old issue in the pipeline parser, typos and unsupported feature names will be silently ignored on old GStreamer versions. Typos on top-level properties will be ignored on versions prior to 1.18. Typos on child::properties will be ignored on versions prior to 1.21.

* Bayer formats need to be 4 byte aligned to be properly processed by GStreamer. If no size is specified (or a range is provided) a word aligned width will be automatically selected. If the width is hardcoded and it is not word aligned, the pipeline will fail displaying an error.

* Under very specific conditions we've found that a set_state() followed immediately by a get_state() will report a failure. This has been found to be a bug in the GStreamer core, where a state conditional is not protected against spurious wakeups. An [upstream fix](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/4086) was merged to the mainline, and GStreamer 1.23 will include this fix. This issue has been reproduced in certain installations of NVIDIA Jetson boards.
  * As a workaround if you are using older GStreamer versions, is to configure `async=false` in all the sink elements in your pipeline, so that the state condition variable is not needed. Only use this is your pipeline does not require synchronization.

