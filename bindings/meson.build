py_mod = import('python')
py3 = py_mod.find_installation('python3')
py3_dep = py3.dependency()
py3_link_args = []
# With Py_LIMITED_API, pyconfig.h requests python3.lib (the stable-ABI import
# library for python3.dll). Meson's dependency links python3XY.lib, which would
# bind the module to one interpreter version, so keep only headers here.
if cxx.get_argument_syntax() == 'msvc'
  py3_libdir = run_command(py3, '-c',
    'import os, sys; print(os.path.join(sys.base_prefix, "libs"))',
    check: true).stdout().strip()
  if not fs.is_file(py3_libdir / 'python3.lib')
    error('python3.lib not found in ' + py3_libdir + '; it is required for the stable-ABI pygstpylon module')
  endif
  py3_dep = py3_dep.partial_dependency(compile_args: true, includes: true)
  py3_link_args = ['/LIBPATH:' + py3_libdir]
endif
message(py3.full_path())
message(py3.get_install_dir())

pygstpylon_sources = ['src/pygstpylon_abi3.cpp']
gstpylon_dep = dependency('gstpylon')

pygstpylon_deps = [
    py3_dep,
    gstvideo_dep,
    gst_dep,
    gstpylon_dep
]

# Prefer the interpreter install dir so Debian packaging with
# DEB_PYTHON_INSTALL_LAYOUT=deb lands under dist-packages.
py_install_dir = py3.get_install_dir()

# Same CPython stable ABI floor as pypylon (3.9 / cp39-abi3). Meson 0.61 does
# not have python.extension_module(limited_api:), so name the shared module
# explicitly with the standard abi3 suffix.
if host_system == 'windows'
  pymeta_suffix = 'pyd'
else
  pymeta_suffix = 'abi3.so'
endif

binding_rpath = pylon_install_rpath
if host_system == 'linux' or host_system == 'darwin'
  relative_libdir = run_command(python3, '-c',
    'import posixpath, sys; print(posixpath.relpath(sys.argv[1].replace(chr(92), "/"), sys.argv[2].replace(chr(92), "/")))',
    get_option('prefix') / get_option('libdir'),
    get_option('prefix') / py_install_dir,
    check: true).stdout().strip()
  binding_rpath = loader_origin / relative_libdir
  if pylon_install_rpath != ''
    binding_rpath += ':' + pylon_install_rpath
  endif
endif

pymeta = shared_module('pygstpylon',
  pygstpylon_sources,
  name_prefix : '',
  name_suffix : pymeta_suffix,
  install : true,
  install_dir : py_install_dir,
  build_rpath : pylon_rpath,
  install_rpath : binding_rpath,
  cpp_args: ['-DHAVE_CONFIG_H=1', '-DPy_LIMITED_API=0x03090000'],
  link_args : py3_link_args,
  include_directories : [configinc],
  gnu_symbol_visibility: 'inlineshidden',
  dependencies : pygstpylon_deps,
)

binding_env = environment()
binding_env.prepend('PYTHONPATH', meson.current_build_dir())
meson.add_devenv(binding_env)

# Register here: root meson.build runs subdir('tests') before subdir('bindings'),
# so this suite cannot depend on pymeta if it lived only under tests/.
if meson.is_cross_build() and not meson.can_run_host_binaries()
  message('Skipping pygstpylon tests while cross-building')
else
  gi_probe = run_command(
    py3,
    '-c',
    'import gi; gi.require_version("Gst", "1.0"); from gi.repository import Gst',
    check: false,
  )
  if gi_probe.returncode() != 0
    warning('Skipping pygstpylon tests: PyGObject (gi) not available')
  else
    pygstpylon_test_env = pylon_build_env
    pygstpylon_test_env.set('PYLON_TEST_BINDING', pymeta.full_path())
    pygstpylon_test_env.set('PYLON_CAMEMU', '4')
    pygstpylon_test_env.set('PYLONSRC_SKIP_TRANSPORT_LAYERS', 'usb,gige,cxp,cameralink')
    pygstpylon_test_env.set('GST_REGISTRY_1_0', meson.current_build_dir() / 'registry.bin')
    pygstpylon_test_env.set('PYTHONPATH', meson.current_build_dir())
    pygstpylon_test_env.set('MALLOC_PERTURB_', '0')

    # test(depends:) needs Meson >= 0.63; Ubuntu 22.04 ships 0.61. ninja test
    # rebuilds default targets (including pymeta) before running tests.
    pygstpylon_test_script = files('../tests/pygstpylon/test_pygstpylon.py')
    if meson.version().version_compare('>= 0.63')
      test(
        'pygstpylon',
        py3,
        args: [pygstpylon_test_script],
        env: pygstpylon_test_env,
        depends: [pymeta, gstpylon_plugin],
        timeout: 180,
        is_parallel: false,
        workdir: meson.project_source_root(),
      )
    else
      test(
        'pygstpylon',
        py3,
        args: [pygstpylon_test_script],
        env: pygstpylon_test_env,
        timeout: 180,
        is_parallel: false,
        workdir: meson.project_source_root(),
      )
    endif
  endif
endif
