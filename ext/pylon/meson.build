flatbuffers_dep = dependency('flatbuffers', fallback : ['flatbuffers', 'flatbuffers_dep'])

prog_flatc =  find_program('flatc')

gstpylon_cache_generated_h = custom_target(
  'gstpylon_cache_generated.h',
  input : meson.project_source_root() / 'schema' / 'gstpylon_cache.fbs',
  output : 'gstpylon_cache_generated.h',
  command : [prog_flatc, '-c',  '@INPUT@'],
)

dependencies = [gstpylon_dep, flatbuffers_dep]

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
cuda_dep = dependency('cuda', version : '>=10', required: false)
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
  pylon_sources + git_version + gstpylon_cache_generated_h,
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
