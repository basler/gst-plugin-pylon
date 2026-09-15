dependencies = [gstpylon_dep]
plugin_inc = [configinc]
cpp_args = [gst_plugin_pylon_args]
plugin_rpath = pylon_rpath

pylon_sources = [
  'gstchildinspector.cpp',
  'gstpylon.cpp',
  'gstpylondisconnecthandler.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylonplugin.cpp',
  'gstpylonsrc.cpp',
  'gstpylonsysmembufferfactory.cpp',
]

# DeepStream installs a versioned tree plus a `deepstream` symlink.
nvds_libdir = [
  '/opt/nvidia/deepstream/deepstream/lib',
  '/usr/lib/aarch64-linux-gnu/nvidia',
]
nvds_incdir = '/opt/nvidia/deepstream/deepstream/sources/includes'
cuda_libdirs = [
  '/usr/local/cuda/lib64',
  '/usr/local/cuda/targets/aarch64-linux/lib',
  '/usr/local/cuda/targets/x86_64-linux/lib',
]
cuda_incdir = '/usr/local/cuda/include'

nvmm_opt = get_option('nvmm')
if not nvmm_opt.disabled()
  nvds_dep = cc.find_library('nvbufsurface', dirs: nvds_libdir, required: false)
  cuda_dep = dependency('cuda', version : '>=11', required: false)
  if not cuda_dep.found()
    # JetPack often has no cuda.pc; fall back to the toolkit layout.
    fs_nvmm = import('fs')
    cuda_lib = cc.find_library('cudart', dirs: cuda_libdirs, required: false)
    if cuda_lib.found() and fs_nvmm.exists(cuda_incdir / 'cuda_runtime.h')
      cuda_dep = declare_dependency(
        dependencies: cuda_lib,
        include_directories: include_directories(cuda_incdir),
      )
    endif
  endif

  if nvds_dep.found() and cuda_dep.found()
    pylon_sources += ['gstpylondsnvmmbufferfactory.cpp']
    dependencies += [nvds_dep, cuda_dep]
    plugin_inc += [include_directories(nvds_incdir)]
    cpp_args += ['-DNVMM_ENABLED']
    nvds_rpath = '/opt/nvidia/deepstream/deepstream/lib'
    if plugin_rpath != ''
      plugin_rpath = plugin_rpath + ':' + nvds_rpath
    else
      plugin_rpath = nvds_rpath
    endif
    message('NVMM support enabled (DeepStream + CUDA)')
  elif nvmm_opt.enabled()
    error('NVMM was enabled (-Dnvmm=enabled) but DeepStream nvbufsurface or CUDA was not found')
  else
    message('DeepStream or CUDA not found, skipping NVMM support')
  endif
endif

gstpylon_plugin = library('gstpylon',
  pylon_sources + git_version,
  c_args : gst_plugin_pylon_args,
  cpp_args : cpp_args,
  link_args : [noseh_link_args],
  include_directories : plugin_inc,
  gnu_symbol_visibility: 'inlineshidden',
  dependencies : dependencies,
  install : true,
  install_dir : plugins_install_dir,
  install_rpath : plugin_rpath
)

plugins += [gstpylon_plugin]
