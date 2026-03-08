# Pylon OO Refactor Notes

This branch keeps the original `origin/main` behavior while steadi-
ly moving the internal API toward clearer, schema-driven ownership.

## Key changes

- `ext/pylon/gstpylon.cpp` now builds explicit `GstPylonObjectSchema`
  instances for camera and stream—caching the `device_full_name`, schema
  cache key, nodemap, and `GstPylonCache` in one place so the live child
  objects and the introspection helpers all share the same metadata.
- `gst-libs/gst/pylon/gstpylonobject.{h,cpp}` now treat the schema as the
  source of truth for dynamic `GType` identity and walk the schema into
  `GstPylonFeatureWalker::install_properties()`.
- `gst-libs/gst/pylon/gstpylonfeaturewalker.{h,cpp}` and
  `gst-libs/gst/pylon/gstpylonparamfactory.{h,cpp}` were refactored so
  they no longer accept ad-hoc nodemap/caches: both layers now consume
  the schema object and its owned cache helpers, keeping selectors, cache
  keys, and enum registrations in sync with the PM runtime.
- `gst-libs/gst/pylon/gstpylonintrospection.cpp` kept the existing
  heuristics; the surrounding callers now pass the schema so introspection
  work uses the same cache as the running element instead of duplicating
  nodemap lookups.

## Validation

- `PYLON_ROOT=/opt/pylon CCACHE_TEMPDIR=/tmp/ccache-tmp meson compile -C build`
- `PYLON_CAMEMU=1 GST_PLUGIN_PATH=build/ext/pylon gst-inspect-1.0 pylonsrc`
  (output differs from `origin/main` only in `Filename`/`Version`)
- `PYLON_CAMEMU=1 GST_PLUGIN_PATH=build/ext/pylon gst-launch-1.0 pylonsrc num-buffers=10 ! fakesink` (runs to EOS)

The baseline commands reported earlier in this thread are still the ones
to use for regression checks.
