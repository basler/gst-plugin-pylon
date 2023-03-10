
pylon_sources = [
  'gstchildinspector.cpp',
  'gstpylon.cpp',
  'gstpylonbufferfactory.cpp',
  'gstpylonbufferpool.cpp',
  'gstpylondisconnecthandler.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylonplugin.cpp',
  'gstpylonsrc.cpp',
]

gstpylon_plugin = library('gstpylon',
  pylon_sources + git_version,
  c_args : gst_plugin_pylon_args,
  cpp_args : gst_plugin_pylon_args,
  link_args : [noseh_link_args],
  include_directories : [configinc],
  gnu_symbol_visibility: 'inlineshidden',
  dependencies : [gstpylon_dep],
  install : true,
  install_dir : plugins_install_dir
)

plugins += [gstpylon_plugin]
