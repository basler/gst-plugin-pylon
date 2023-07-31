dependencies = [gstpylon_dep]
include_directories = [configinc]
cpp_args = [gst_plugin_pylon_args]

pylon_sources = [
  'gstchildinspector.cpp',
  'gstpylon.cpp',
  'gstpylondisconnecthandler.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylonplugin.cpp',
  'gstpylonsrc.cpp',
  'gstpylonsysmembufferfactory.cpp',
]

nvds_dep = cc.find_library('nvbufsurface',
    dirs: '/opt/nvidia/deepstream/deepstream/lib/',
    required: false
)
cuda_dep = dependency('cuda', version : '>=11', required: false)
if nvds_dep.found() and cuda_dep.found()
  nvds_include = include_directories('/opt/nvidia/deepstream/deepstream/sources/includes/')

  pylon_sources += ['gstpylondsnvmmbufferfactory.cpp']

  dependencies += [nvds_dep, cuda_dep]
  include_directories += [nvds_include]
  cpp_args += ['-DNVMM_ENABLED']
else
  message('Deepstream or CUDA not found, skipping NVMM support')
endif


gstpylon_plugin = library('gstpylon',
  pylon_sources + git_version,
  c_args : gst_plugin_pylon_args,
  cpp_args : cpp_args,
  link_args : [noseh_link_args],
  include_directories : include_directories,
  gnu_symbol_visibility: 'inlineshidden',
  dependencies : dependencies,
  install : true,
  install_dir : plugins_install_dir
)

plugins += [gstpylon_plugin]
