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
pymeta = shared_module('pygstpylon',
  pygstpylon_sources,
  name_prefix : '',
  name_suffix : 'abi3.so',
  install : true,
  install_dir : py_install_dir,
  install_rpath : pylon_rpath,
  cpp_args: ['-DHAVE_CONFIG_H=1', '-DPy_LIMITED_API=0x030A0000'],
  include_directories : [configinc],
  gnu_symbol_visibility: 'inlineshidden',
  dependencies : pygstpylon_deps,
)

# Register here: root meson.build runs subdir('tests') before subdir('bindings'),
# so this suite cannot depend on pymeta if it lived only under tests/.
gi_probe = run_command(
  py3,
  '-c',
  'import gi; gi.require_version("Gst", "1.0"); from gi.repository import Gst',
  check: false,
)
if gi_probe.returncode() != 0
  warning('Skipping pygstpylon tests: PyGObject (gi) not available')
else
  pygstpylon_pylon_root = run_command(
    py3,
    '-c',
    'import os; print(os.environ.get("PYLON_ROOT", "/opt/pylon"))',
    check: false,
  ).stdout().strip()

  pygstpylon_lib_dirs = [
    meson.project_build_root() / 'gst-libs' / 'gst' / 'pylon',
    join_paths(pygstpylon_pylon_root, 'lib'),
    '/opt/nvidia/deepstream/deepstream/lib',
    '/usr/local/cuda/lib64',
  ]
  pygstpylon_lib_path = ':'.join(pygstpylon_lib_dirs)
  pygstpylon_plugin_path = meson.project_build_root() / 'ext' / 'pylon'
  pygstpylon_test_env = {
    'PYLON_CAMEMU': '4',
    'PYLON_ROOT': pygstpylon_pylon_root,
    'PYTHONPATH': meson.current_build_dir(),
    'GST_PLUGIN_PATH': pygstpylon_plugin_path,
    'GST_PLUGIN_PATH_1_0': pygstpylon_plugin_path,
    'LD_LIBRARY_PATH': pygstpylon_lib_path,
    'DYLD_LIBRARY_PATH': pygstpylon_lib_path,
    'MALLOC_PERTURB_': '0',
  }

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
