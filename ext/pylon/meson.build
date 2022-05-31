pylon_dep = dependency('pylon', method : 'cmake', cmake_module_path : 'cmake')

pylon_sources = [
  'gstchildinspector.c',
  'gstpylonsrc.c',
  'gstpylonplugin.c',
  'gstpyloncamera.cpp',
  'gstpylon.cpp',
  'gstpylonintrospection.cpp'
]

gstpylon = library('gstpylon',
  pylon_sources,
  c_args : gst_plugin_pylon_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  install_dir : plugins_install_dir,
)


pkgconfig.generate(gstpylon, install_dir : plugins_pkgconfig_install_dir)
plugins += [gstpylon]
