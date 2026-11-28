dependencies = [gstpylon_dep]
plugin_inc = [configinc]
cpp_args = [gst_plugin_pylon_args]
if get_option('test-hooks')
  cpp_args += ['-DGST_PYLON_ENABLE_TEST_HOOKS', '-DGST_PYLON_IMAGEHANDLER_ENABLE_TEST_HOOKS']
endif
plugin_build_rpath = pylon_rpath
plugin_rpath = pylon_install_rpath
if host_system == 'linux' or host_system == 'darwin'
  plugin_relative_libdir = run_command(python3, '-c',
    'import posixpath, sys; print(posixpath.relpath(sys.argv[1].replace(chr(92), "/"), sys.argv[2].replace(chr(92), "/")))',
    get_option('prefix') / get_option('libdir'),
    get_option('prefix') / plugins_install_dir,
    check: true).stdout().strip()
  plugin_rpath = loader_origin / plugin_relative_libdir
  if pylon_install_rpath != ''
    plugin_rpath += ':' + pylon_install_rpath
  endif
endif

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
nvmm_root_prefix = meson.is_cross_build() ? cross_sysroot : ''
nvmm_probe_defaults = not meson.is_cross_build() or cross_sysroot != ''

custom_nvds_libdir = get_option('nvds-libdir')
nvds_libdir = []
if custom_nvds_libdir != ''
  nvds_libdir += [custom_nvds_libdir]
endif
nvds_default_libdirs = [
  '/opt/nvidia/deepstream/deepstream/lib',
  '/usr/lib/aarch64-linux-gnu/nvidia',
  '/usr/lib/aarch64-linux-gnu/tegra',
]
if nvmm_probe_defaults
  foreach d : nvds_default_libdirs
    nvds_libdir += [nvmm_root_prefix + d]
  endforeach
endif

custom_nvds_incdir = get_option('nvds-incdir')
nvds_incdir = ''
if custom_nvds_incdir != '' and fs_nvmm.exists(custom_nvds_incdir / 'nvbufsurface.h')
  nvds_incdir = custom_nvds_incdir
elif nvmm_probe_defaults
  foreach d : [
    '/opt/nvidia/deepstream/deepstream/sources/includes',
    '/usr/src/jetson_multimedia_api/include',
  ]
    if nvds_incdir == '' and fs_nvmm.exists(nvmm_root_prefix + d / 'nvbufsurface.h')
      nvds_incdir = nvmm_root_prefix + d
    endif
  endforeach
endif

cuda_default_libdirs = ['/usr/local/cuda/lib64']
if host_machine.cpu_family() == 'aarch64'
  cuda_default_libdirs += ['/usr/local/cuda/targets/aarch64-linux/lib']
elif host_machine.cpu_family() == 'x86_64'
  cuda_default_libdirs += ['/usr/local/cuda/targets/x86_64-linux/lib']
endif
cuda_libdirs = []
cuda_incdir = ''
if nvmm_probe_defaults
  foreach d : cuda_default_libdirs
    cuda_libdirs += [nvmm_root_prefix + d]
  endforeach
  cuda_incdir = nvmm_root_prefix + '/usr/local/cuda/include'
endif

nvmm_opt = get_option('nvmm')
if not nvmm_opt.disabled()
  nvds_dep = cc.find_library('nvbufsurface', dirs: nvds_libdir, required: false)
  cuda_dep = dependency('cudart', version : '>=11', required: false)
  if not cuda_dep.found()
    # JetPack often has no cudart.pc; fall back to the toolkit layout before
    # dependency('cuda') so we link dynamic libcudart.so instead of cudart_static.
    cuda_lib = cc.find_library('cudart', dirs: cuda_libdirs, required: false)
    if cuda_lib.found() and cuda_incdir != '' and fs_nvmm.exists(cuda_incdir / 'cuda_runtime.h')
      cuda_dep = declare_dependency(
        dependencies: cuda_lib,
        include_directories: include_directories(cuda_incdir),
      )
    else
      cuda_dep = dependency('cuda', version : '>=11', required: false)
    endif
  endif

  if nvds_dep.found() and cuda_dep.found() and nvds_incdir != '' and fs_nvmm.exists(nvds_incdir / 'nvbufsurface.h')
    pylon_sources += ['gstpylondsnvmmbufferfactory.cpp']
    dependencies += [nvds_dep, cuda_dep]
    plugin_inc += [include_directories(nvds_incdir)]
    cpp_args += ['-DNVMM_ENABLED']
    if host_system == 'linux'
      nvmm_build_rpaths = ['/opt/nvidia/deepstream/deepstream/lib']
      nvmm_install_rpaths = ['/opt/nvidia/deepstream/deepstream/lib']
      if custom_nvds_libdir != ''
        if not nvmm_build_rpaths.contains(custom_nvds_libdir)
          nvmm_build_rpaths += [custom_nvds_libdir]
        endif
        if not meson.is_cross_build()
          if not nvmm_install_rpaths.contains(custom_nvds_libdir)
            nvmm_install_rpaths += [custom_nvds_libdir]
          endif
        elif cross_sysroot != ''
          nvds_target_libdir = run_command(python3, '-c',
            'import posixpath, sys\n' +
            'root = posixpath.normpath(sys.argv[1].replace(chr(92), "/"))\n' +
            'path = posixpath.normpath(sys.argv[2].replace(chr(92), "/"))\n' +
            'if root != "/" and path.startswith(root + "/"):\n' +
            '    print(path[len(root):])\n',
            cross_sysroot, custom_nvds_libdir,
            check: true).stdout().strip()
          if nvds_target_libdir != '' and not nvmm_install_rpaths.contains(nvds_target_libdir)
            nvmm_install_rpaths += [nvds_target_libdir]
          endif
        endif
      endif
      if nvmm_probe_defaults
        foreach d : nvds_default_libdirs + cuda_default_libdirs
          probe_d = nvmm_root_prefix + d
          if fs_nvmm.is_dir(probe_d)
            if not nvmm_build_rpaths.contains(probe_d)
              nvmm_build_rpaths += [probe_d]
            endif
            if not nvmm_install_rpaths.contains(d)
              nvmm_install_rpaths += [d]
            endif
          endif
        endforeach
      endif
      nvmm_build_rpath_str = ':'.join(nvmm_build_rpaths)
      nvmm_install_rpath_str = ':'.join(nvmm_install_rpaths)
      plugin_build_rpath = plugin_build_rpath != '' ? plugin_build_rpath + ':' + nvmm_build_rpath_str : nvmm_build_rpath_str
      plugin_rpath = plugin_rpath != '' ? plugin_rpath + ':' + nvmm_install_rpath_str : nvmm_install_rpath_str
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
  build_rpath : plugin_build_rpath,
  install_rpath : plugin_rpath
)

plugins += [gstpylon_plugin]
