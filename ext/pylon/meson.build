# gst-plugin-pylon
# this plugin has to link against the basler pylon runtime libraries
# 

# import PYLON_ROOT environment into meson
pylon_path = run_command(python3, '-c', 'import os; print(os.environ.get("PYLON_ROOT"))', check: false).stdout().strip()

# detect pylon 7.x
pylon_dep = dependency('pylon', method : 'cmake', modules : ['pylon::PylonBase', 'pylon::GCBase', 'pylon::GenApi', 'pylon::PylonUtility'], version : '>=7.1', required: false)
if pylon_dep.found() 
  # patch rpath on linux
  if target_machine.system() == 'linux'
    pylon_link_args = ['-Wl,--enable-new-dtags', '-Wl,-rpath,' + pylon_path / 'lib']
  else
    pylon_link_args = []
  endif
else
    message('fallback to detect pylon 6.x')
    pylon_dep = dependency('pylon', method : 'cmake', cmake_module_path: 'cmake', required: true)
    # runtime path is properly defined by cmake
    pylon_link_args = [] 
endif

pylon_sources = [
  'gstpylonsrc.c',
  'gstpylonplugin.c',
  'gstchildinspector.cpp',
  'gstpyloncamera.cpp',
  'gstpylon.cpp',
  'gstpylonintrospection.cpp',
  'gstpylonparamspecs.cpp',
  'gstpylonfeaturewalker.cpp'
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

