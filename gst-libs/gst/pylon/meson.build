# gst-plugin-pylon
# this plugin has to link against the basler pylon runtime libraries
#

# import PYLON_ROOT environment into meson
pylon_path = run_command(python3, '-c', 'import os; print(os.environ.get("PYLON_ROOT","PYLON_ROOT_NOT_SET"))', check: false).stdout().strip()

if target_machine.system() == 'linux' and pylon_path == 'PYLON_ROOT_NOT_SET'
  # PYLON_ROOT has to be set
  error('PYLON_ROOT has to be set to location of pylon install e.g. /opt/pylon')
endif

# detect pylon 7.x
pylon_dep = dependency('pylon', method : 'cmake', modules : ['pylon::PylonBase', 'pylon::GCBase', 'pylon::GenApi', 'pylon::PylonUtility'],
        version : '>=7.1', cmake_module_path: pylon_path / 'share/pylon/cmake/' , required: false)
if pylon_dep.found()
  # patch rpath on linux
  if target_machine.system() == 'linux'
    pylon_link_args = ['-Wl,--enable-new-dtags', '-Wl,-rpath,' + pylon_path / 'lib']
  else
    pylon_link_args = []
  endif
else
    message('fallback to detect pylon 6.x')
    pylon_dep = dependency('pylon', method : 'cmake', cmake_module_path: meson.project_source_root() + '/cmake', required: true)
    # runtime path is properly defined by cmake
    pylon_link_args = []
endif

gstpylon_sources = [
  'gstpyloncache.cpp',
  'gstpylondebug.c',
  'gstpylonfeaturewalker.cpp',
  'gstpylonintrospection.cpp',
  'gstpylonmeta.cpp',
  'gstpylonobject.cpp',
  'gstpylonparamspecs.cpp'
]

gstpylon_headers = [
  'gstpylon-prelude.h',
  'gstpyloncache.h',
  'gstpylondebug.h',
  'gstpylonfeaturewalker.h',
  'gstpylonintrospection.h',
  'gstpylonmeta.h',
  'gstpylonmetaprivate.h',
  'gstpylonobject.h',
  'gstpylonparamspecs.h'
]

install_headers(gstpylon_headers, subdir : 'gstreamer-1.0/gst/pylon/')

gstpylon = library('gstpylon-' + api_version,
  gstpylon_sources,
  c_args : gst_plugin_pylon_args + '-DBUILDING_EXT_PYLONSRC',
  cpp_args : gst_plugin_pylon_args + '-DBUILDING_EXT_PYLONSRC',
  link_args : [noseh_link_args, pylon_link_args],
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
)

# generate pkgconfig file
pkgconfig.generate(gstpylon, description : 'Basler Pylon gstreamer source')

# set dependency for plugin code examples
gstpylon_dep = declare_dependency(link_with : gstpylon,
  link_args : pylon_link_args,
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep])

# For examples to mimic what they would to on an installed setud
meson.override_dependency('gstpylon', gstpylon_dep)

