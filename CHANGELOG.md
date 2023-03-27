# Changelog
All notable changes to this project will be documented in this file.

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

