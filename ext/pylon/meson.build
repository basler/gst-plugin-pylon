dependencies = [gstpylon_dep]
plugin_inc = [configinc]
cpp_args = [gst_plugin_pylon_args]
if get_option('test-hooks')
  cpp_args += ['-DGST_PYLON_ENABLE_TEST_HOOKS', '-DGST_PYLON_IMAGEHANDLER_ENABLE_TEST_HOOKS']
endif
plugin_rpath = pylon_rpath

pylon_sources = [
  'gstchildinspector.cpp',
  'gstpyloncaps.cpp',
  'gstpyloncapture.cpp',
  'gstpylondeviceprovider.cpp',
  'gstpylondisconnecthandler.cpp',
  'gstpylonimagehandler.cpp',
  'gstpyloninspect.cpp',
  'gstpylonnvmmcolor.cpp',
  'gstpylonplugin.cpp',
  'gstpylonruntime.cpp',
  'gstpylonschema.cpp',
  'gstpylonsession.cpp',
  'gstpylonsrc.cpp',
  'gstpylonsysmembufferfactory.cpp',
]

# DeepStream installs a versioned tree plus a `deepstream` symlink.
# On Jetson without DeepStream, nvbufsurface.h is provided by jetson_multimedia_api.
fs_nvmm = import('fs')
custom_nvds_libdir = get_option('nvds-libdir')
nvds_libdir = []
if custom_nvds_libdir != ''
  nvds_libdir += [custom_nvds_libdir]
endif
nvds_libdir += [
  '/opt/nvidia/deepstream/deepstream/lib',
  '/usr/lib/aarch64-linux-gnu/nvidia',
  '/usr/lib/aarch64-linux-gnu/tegra',
]
custom_nvds_incdir = get_option('nvds-incdir')
if custom_nvds_incdir != '' and fs_nvmm.exists(custom_nvds_incdir / 'nvbufsurface.h')
  nvds_incdir = custom_nvds_incdir
elif fs_nvmm.exists('/opt/nvidia/deepstream/deepstream/sources/includes/nvbufsurface.h')
  nvds_incdir = '/opt/nvidia/deepstream/deepstream/sources/includes'
elif fs_nvmm.exists('/usr/src/jetson_multimedia_api/include/nvbufsurface.h')
  nvds_incdir = '/usr/src/jetson_multimedia_api/include'
else
  nvds_incdir = '/opt/nvidia/deepstream/deepstream/sources/includes'
endif
cuda_libdirs = [
  '/usr/local/cuda/lib64',
  '/usr/local/cuda/targets/aarch64-linux/lib',
  '/usr/local/cuda/targets/x86_64-linux/lib',
]
cuda_incdir = '/usr/local/cuda/include'

nvmm_opt = get_option('nvmm')
if not nvmm_opt.disabled()
  nvds_dep = cc.find_library('nvbufsurface', dirs: nvds_libdir, required: false)
  cuda_dep = dependency('cudart', version : '>=11', required: false)
  if not cuda_dep.found()
    # JetPack often has no cudart.pc; fall back to the toolkit layout before
    # dependency('cuda') so we link dynamic libcudart.so instead of cudart_static.
    cuda_lib = cc.find_library('cudart', dirs: cuda_libdirs, required: false)
    if cuda_lib.found() and fs_nvmm.exists(cuda_incdir / 'cuda_runtime.h')
      cuda_dep = declare_dependency(
        dependencies: cuda_lib,
        include_directories: include_directories(cuda_incdir),
      )
    else
      cuda_dep = dependency('cuda', version : '>=11', required: false)
    endif
  endif

  if nvds_dep.found() and cuda_dep.found() and fs_nvmm.exists(nvds_incdir / 'nvbufsurface.h')
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
    message('NVMM support enabled (' + nvds_incdir + ' + CUDA)')
  elif nvmm_opt.enabled()
    error('NVMM was enabled (-Dnvmm=enabled) but nvbufsurface (DeepStream/MMAPI) or CUDA was not found')
  else
    message('DeepStream/MMAPI or CUDA not found, skipping NVMM support')
  endif
endif

gstpylon_plugin = library('gstpylon',
  pylon_sources + build_id_h,
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
