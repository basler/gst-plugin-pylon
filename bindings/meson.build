py_mod = import('python')
py3 = py_mod.find_installation('python3')
py3_dep = py3.dependency()
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

# Use CPython's stable ABI (3.10+) so the package built on Ubuntu 22.04 also
# imports on Debian bookworm (3.11) and Ubuntu 24.04 (3.12). Meson 0.61 does
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

# Stable ABI (python3.lib) is for Linux packaging across distro Python
# versions. On Windows, Py_LIMITED_API pulls in python3.lib via pragma
# while Meson links python3XY.lib; CI images often lack python3.lib.
binding_cpp_args = ['-DHAVE_CONFIG_H=1']
if host_system != 'windows'
  binding_cpp_args += ['-DPy_LIMITED_API=0x030A0000']
endif

pymeta = shared_module('pygstpylon',
  pygstpylon_sources,
  name_prefix : '',
  name_suffix : pymeta_suffix,
  install : true,
  install_dir : py_install_dir,
  build_rpath : pylon_rpath,
  install_rpath : binding_rpath,
  cpp_args: binding_cpp_args,
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
