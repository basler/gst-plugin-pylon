
nvds_dep = cc.find_library('nvbufsurface',
  dirs: '/opt/nvidia/deepstream/deepstream/lib/',
  required: true
)
nvds_include = include_directories('/opt/nvidia/deepstream/deepstream/sources/includes/')

pylon_sources = [
  'gstchildinspector.cpp',
  'gstpylon.cpp',
  'gstpylondisconnecthandler.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylondsnvmmbufferfactory.cpp',
  'gstpylonplugin.cpp',
  'gstpylonsrc.cpp',
  'gstpylonsysmembufferfactory.cpp',
]

gstpylon_plugin = library('gstpylon',
  pylon_sources + git_version,
  c_args : gst_plugin_pylon_args,
  cpp_args : gst_plugin_pylon_args,
  link_args : [noseh_link_args],
  include_directories : [configinc, nvds_include],
  gnu_symbol_visibility: 'inlineshidden',
  dependencies : [gstpylon_dep, nvds_dep],
  install : true,
  install_dir : plugins_install_dir
)

plugins += [gstpylon_plugin]
