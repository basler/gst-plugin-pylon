# Changelog
All notable changes to this project will be documented in this file.

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

