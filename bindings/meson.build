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



