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

pylon_sources = [
  'gstpylonsrc.c',
  'gstpylonplugin.c',
  'gstchildinspector.cpp',
  'gstpylonobject.cpp',
  'gstpylon.cpp',
  'gstpylonintrospection.cpp',
  'gstpylonparamspecs.cpp',
  'gstpylonfeaturewalker.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylondisconnecthandler.cpp'
]

gstpylon = library('gstpylon',
  pylon_sources,
  c_args : gst_plugin_pylon_args,
  link_args : [noseh_link_args, pylon_link_args],
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  install_dir : plugins_install_dir
)

pkgconfig.generate(gstpylon, install_dir : plugins_pkgconfig_install_dir)
plugins += [gstpylon]

