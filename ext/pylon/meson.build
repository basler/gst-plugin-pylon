
pylon_sources = [
  'gstpylonsrc.cpp',
  'gstpylonplugin.cpp',
  'gstchildinspector.cpp',
  'gstpylon.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylondisconnecthandler.cpp'
]

gstpylon_plugin = library('gstpylon',
  pylon_sources,
  c_args : gst_plugin_pylon_args,
  cpp_args : gst_plugin_pylon_args,
  link_args : [noseh_link_args],
  include_directories : [configinc],
  dependencies : [gstpylon_dep],
  install : true,
  install_dir : plugins_install_dir
)

plugins += [gstpylon_plugin]
